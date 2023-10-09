#include "tags.h"
#include "mdview.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

/*
 * Decorations
 */

#define TOGGLE_DECORATION(bit, tag)                                            \
  if (ctx->block_type == -1)                                                   \
    block_paragraph(ctx);                                                      \
  if (ctx->text_decoration & (1 << bit)) {                                     \
    if (!bufcat(ctx->curr_buf, "</" tag ">", 3 + sizeof(tag) - 1))             \
      return 0;                                                                \
  } else {                                                                     \
    if (!bufcat(ctx->curr_buf, "<" tag ">", 2 + sizeof(tag) - 1))              \
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

static inline int close_header_block(struct mdview_ctx *ctx) {
  char header_tag[5] = {'<', '/', 'h', '0' + ctx->block_type, '>'};
  return bufcat(ctx->curr_buf, header_tag, 5);
}

int close_block(struct mdview_ctx *ctx) {
  int retval;
  switch (ctx->block_type) {
  case -1:
    retval = 1;
    break;
  case 0:
    retval = bufcat(ctx->curr_buf, "</p>", 4);
    break;
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
    retval = close_header_block(ctx);
    break;
  case 7:
    retval = bufcat(ctx->curr_buf, "</li></ul>", 10);
    break;
  case 8:
    retval = bufcat(ctx->curr_buf, "</ol>", 5);
    break;
  case 9:
    retval = bufcat(ctx->curr_buf, "</code></pre>", 13);
    break;
  case 10:
    retval = bufcat(ctx->curr_buf, "</blockquote>", 13);
    break;
  default:
    fprintf(stderr, "Failed to close nonexistant block type %d\n",
            ctx->block_type);
    retval = 0;
    break;
  }

  ctx->block_type = -1;
  ctx->block_subtype = 0;
  return retval;
}

#define BLOCK_TAG(type, subtype, tag)                                          \
  if (!close_block(ctx))                                                       \
    return 1;                                                                  \
  ctx->block_type = type;                                                      \
  ctx->block_subtype = subtype;                                                \
  return bufcat(ctx->curr_buf, "<" tag ">", 2 + sizeof(tag) - 1);

int block_paragraph(struct mdview_ctx *ctx) { BLOCK_TAG(0, 0, "p") }
int block_unordered_list(struct mdview_ctx *ctx, char starter) {
  unsigned int subtype = (ctx->indent << 8 | starter);
  BLOCK_TAG(7, subtype, "ul")
}

// TODO: ordered list
// int block_ordered_list(struct mdview_ctx *ctx) { BLOCK_TAG(8, 0, "ol")
// }
int block_code(struct mdview_ctx *ctx, unsigned int fence_len) {
  BLOCK_TAG(9, fence_len, "pre><code")
}
int block_quote(struct mdview_ctx *ctx) { BLOCK_TAG(10, 0, "blockquote") }
int block_header(struct mdview_ctx *ctx, int level) {
  close_block(ctx);
  ctx->block_type = level;
  ctx->block_subtype = 0;

  // get a unique ID by incrementing the counter
  ctx->id_cnt++;
  unsigned int n = ctx->id_cnt;
  size_t n_len = 0;
  while (n > 0) {
    n_len++;
    n /= 10;
  }

  char open_tag[11 + n_len];
  snprintf(open_tag, 11 + n_len, "<h%d id=\"%u\">", level, ctx->id_cnt);
  return bufcat(ctx->curr_buf, open_tag, 10 + n_len);
}

/*
 * Functions unrelated to decorations or blocks.
 */

int unordered_list_item(struct mdview_ctx *ctx, char starter) {
  if (ctx->block_type != 7 || (char)ctx->block_subtype != starter) {
    // if we're not in an unordered list or the starter is different, start a
    // new unordered one
    if (!block_unordered_list(ctx, starter))
      return 0;
  } else {
    // otherwise we need to close the last list item
    if (!bufcat(ctx->curr_buf, "</li>", 5))
      return 0;
  }

  if (!bufcat(ctx->curr_buf, "<li>", 4))
    return 0;
  return 1;
}

int write_link(struct mdview_ctx *ctx, char *url, char *text) {
  return bufcat(ctx->curr_buf, "<a href=\"", 9) &&
         bufcat(ctx->curr_buf, url, strlen(url)) &&
         bufcat(ctx->curr_buf, "\">", 2) &&
         bufcat(ctx->curr_buf, text, strlen(text)) &&
         bufcat(ctx->curr_buf, "</a>", 4);
}
