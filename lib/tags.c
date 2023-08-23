#include "tags.h"
#include "mdview.h"
#include "util.h"
#include <stdio.h>

/*
 * Decorations
 */

#define TOGGLE_DECORATION(bit, tag)                                            \
  if (ctx->text_decoration & (1 << bit)) {                                     \
    if (!bufcat(&ctx->html, "</" tag ">", 3 + sizeof(tag) - 1))                \
      return 0;                                                                \
  } else {                                                                     \
    if (!bufcat(&ctx->html, "<" tag ">", 2 + sizeof(tag) - 1))                 \
      return 0;                                                                \
  }                                                                            \
  ctx->text_decoration ^= (1 << bit);                                          \
  return 1;

// Toggle the decoration bit and add the appropriate tag to the buffer.
int toggle_italics(struct mdview_ctx *ctx) { TOGGLE_DECORATION(0, "i") }
int toggle_bold(struct mdview_ctx *ctx) { TOGGLE_DECORATION(1, "b") }
int toggle_strike(struct mdview_ctx *ctx) { TOGGLE_DECORATION(2, "s") }
int toggle_sub(struct mdview_ctx *ctx) { TOGGLE_DECORATION(3, "sub") }
int toggle_inline_code(struct mdview_ctx *ctx) { TOGGLE_DECORATION(4, "code") }
int toggle_sup(struct mdview_ctx *ctx) { TOGGLE_DECORATION(5, "sup") }

// this is called by mdview_flush()
int end_all_decorations(struct mdview_ctx *ctx) {
  if (ctx->text_decoration & (1 << 0)) {
    if (!toggle_italics(ctx))
      return 0;
  }
  if (ctx->text_decoration & (1 << 1)) {
    if (!toggle_bold(ctx))
      return 0;
  }
  if (ctx->text_decoration & (1 << 2)) {
    if (!toggle_strike(ctx))
      return 0;
  }
  if (ctx->text_decoration & (1 << 3)) {
    if (!toggle_sub(ctx))
      return 0;
  }
  if (ctx->text_decoration & (1 << 4)) {
    if (!toggle_inline_code(ctx))
      return 0;
  }
  if (ctx->text_decoration & (1 << 5)) {
    if (!toggle_sub(ctx))
      return 0;
  }
  return 1;
}

/*
 * Blocks
 */

// WARNING: this function might close tags out of order, making slightly invalid
// HTML, but the browser is able to handle it.
int close_block(struct mdview_ctx *ctx) {
  // create this in advance, just in case we need it
  char header_tag[5] = {'<', '/', 'h', '0' + ctx->block_type, '>'};

  switch (ctx->block_type) {
  case 0:
    return bufcat(&ctx->html, "</p>", 4);
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
    return bufcat(&ctx->html, header_tag, 5);
  case 7:
    return bufcat(&ctx->html, "</li></ul>", 10);
  case 8:
    return bufcat(&ctx->html, "</ol>", 5);
  case 9:
    return bufcat(&ctx->html, "</code></pre>", 13);
  case 10:
    return bufcat(&ctx->html, "</blockquote>", 13);
  default:
    fprintf(stderr, "Undefined newline behavior for block type %d\n",
            ctx->block_type);
    return 0;
  }
  return 1;
}

#define BLOCK_TAG(type, tag)                                                   \
  close_block(ctx);                                                            \
  ctx->block_type = type;                                                      \
  return bufcat(&ctx->html, "<" tag ">", 2 + sizeof(tag) - 1);

int block_paragraph(struct mdview_ctx *ctx) { BLOCK_TAG(0, "p") }
int block_unordered_list(struct mdview_ctx *ctx) { BLOCK_TAG(7, "ul") }
// TODO: ordered list
// static int block_ordered_list(struct mdview_ctx *ctx) { BLOCK_TAG(8, "ol")
// }
int block_code(struct mdview_ctx *ctx) { BLOCK_TAG(9, "pre><code") }
int block_quote(struct mdview_ctx *ctx) { BLOCK_TAG(10, "blockquote") }
int block_header(struct mdview_ctx *ctx, int level) {
  close_block(ctx);
  ctx->block_type = level;
  char open_tag[4] = {'<', 'h', '0' + level, '>'};
  return bufcat(&ctx->html, open_tag, 4);
}

int unordered_list_item(struct mdview_ctx *ctx) {
  if (ctx->block_type != 7) {
    // if we're not in an unordered list, start one
    if (!block_unordered_list(ctx))
      return 0;
  } else {
    // otherwise we need to close the last list item
    if (!bufcat(&ctx->html, "</li>", 5))
      return 0;
  }

  if (!bufcat(&ctx->html, "<li>", 4))
    return 0;
  return 1;
}
