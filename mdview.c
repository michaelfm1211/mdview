#include "mdview.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * UTILITY FUNCTIONS
 */

// Add a single character to the buffer.
static int bufadd(struct mdview_buf *buf, char ch) {
  // make sure there is enough space in the buffer
  if (buf->len + 1 >= buf->cap) {
    buf->cap *= 2;
    char *tmp = realloc(buf->buf, buf->cap);
    if (!tmp) {
      perror("realloc");
      return 0;
    }
    buf->buf = tmp;
  }

  // add the character to the buffer and increase the length
  buf->buf[buf->len++] = ch;
  buf->buf[buf->len] = '\0';
  return 1;
}

// Concatenates a string str, of length str_len, to the buffer.
static int bufcat(struct mdview_buf *buf, char *str, size_t str_len) {
  // make sure there is enough space in the buffer
  if (buf->len + str_len >= buf->cap) {
    buf->cap *= 2;
    char *tmp = realloc(buf->buf, buf->cap);
    if (!tmp) {
      perror("realloc");
      return 0;
    }
    buf->buf = tmp;
  }

  // copy the string into the buffer and increase the length
  memcpy(buf->buf + buf->len, str, str_len);
  buf->len += str_len;
  buf->buf[buf->len] = '\0';
  return 1;
}

static void bufclear(struct mdview_buf *buf) {
  buf->len = 0;
  buf->buf[0] = '\0';
}

/*
 * DECORATIONS
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

static int toggle_italics(struct mdview_ctx *ctx) { TOGGLE_DECORATION(0, "i") }
static int toggle_bold(struct mdview_ctx *ctx) { TOGGLE_DECORATION(1, "b") }
static int toggle_strike(struct mdview_ctx *ctx) { TOGGLE_DECORATION(2, "s") }
static int toggle_sub(struct mdview_ctx *ctx) { TOGGLE_DECORATION(3, "sub") }
static int toggle_inline_code(struct mdview_ctx *ctx) {
  TOGGLE_DECORATION(4, "code")
}
static int toggle_sup(struct mdview_ctx *ctx) { TOGGLE_DECORATION(5, "sup") }

// this is called by mdview_flush()
static int end_all_decorations(struct mdview_ctx *ctx) {
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
 * BLOCKS
 */

static int close_block(struct mdview_ctx *ctx) {
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
    return bufcat(&ctx->html, "</ul>", 5);
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

static int block_paragraph(struct mdview_ctx *ctx) { BLOCK_TAG(0, "p") }
static int block_unordered_list(struct mdview_ctx *ctx) { BLOCK_TAG(7, "ul") }
// TODO: ordered list
// static int block_ordered_list(struct mdview_ctx *ctx) { BLOCK_TAG(8, "ol")
// }
static int block_code(struct mdview_ctx *ctx) { BLOCK_TAG(9, "pre><code") }
static int block_quote(struct mdview_ctx *ctx) { BLOCK_TAG(10, "blockquote") }
static int block_header(struct mdview_ctx *ctx, int level) {
  close_block(ctx);
  ctx->block_type = level;
  char open_tag[4] = {'<', 'h', '0' + level, '>'};
  return bufcat(&ctx->html, open_tag, 4);
}

static int unordered_list_item(struct mdview_ctx *ctx) {
  // if we're not in an unordered list, start one
  if (ctx->block_type != 7) {
    if (!block_unordered_list(ctx))
      return 0;
  }

  if (!bufcat(&ctx->html, "<li>", 4))
    return 0;
  return 1;
}

/*
 * PARSING FUNCTIONS
 */

// Handle a newline character and update figure out which block elements need
// to be created/closed.
static int handle_newline(struct mdview_ctx *ctx) {
  if (ctx->line_start && ctx->block_type != 9) {
    // if there has been two consequetive newlines, then close the current
    // block.
    block_paragraph(ctx);
  } else {
    ctx->line_start = 1;
    ctx->indent = 0;
  }

  // different newline behavior depending on block
  switch (ctx->block_type) {
  case 0:  // paragraph
  case 7:  // unordered list
  case 8:  // ordered list
  case 10: // blockquote
    return bufadd(&ctx->html, ' ');
  case 1: // headers
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
    return block_paragraph(ctx);
  case 9:
    return bufadd(&ctx->html, '\n');
  default:
    fprintf(stderr, "Undefined newline behavior for block type %d\n",
            ctx->block_type);
    return 0;
  }

  return 1;
}

// End a special sequence. If it is valid, then write the HTML to the buffer.
// If it is invalid, then write the special characters to the buffer as
// regular characters. Returns 0 on error, 1 on success but no result because
// the sequence was invalid, and 2 on success with a valid sequence. NOTE: all
// special characters must be added in handle_char() too.
static int end_special_sequence(struct mdview_ctx *ctx, char curr_ch) {
  // try to match to a valid special sequence
  switch (ctx->special_type) {
  case '*':
    if (ctx->special_cnt == 1 && ctx->line_start && curr_ch == ' ') {
      if (!unordered_list_item(ctx))
        return 0;
      goto end;
    } else if (ctx->special_cnt == 1) {
      if (!toggle_italics(ctx))
        return 0;
      goto end;
    } else if (ctx->special_cnt == 2) {
      if (!toggle_bold(ctx))
        return 0;
      goto end;
    } else if (ctx->special_cnt == 3) {
      if (!toggle_italics(ctx))
        return 0;
      if (!toggle_bold(ctx))
        return 0;
      goto end;
    }
    break;
  case '-':
    if (ctx->special_cnt == 1 && ctx->line_start && curr_ch == ' ') {
      if (!unordered_list_item(ctx))
        return 1;
      goto end;
    } else if (ctx->special_cnt == 3 && ctx->line_start && curr_ch == '\n') {
      if (!bufcat(&ctx->html, "<hr>", 4))
        return 0;
      goto end;
    }
    break;
  case '#':
    if (ctx->special_cnt >= 1 && ctx->special_cnt <= 6 && curr_ch == ' ') {
      if (!block_header(ctx, ctx->special_cnt))
        return 0;
      goto end;
    }
    break;
  case '`':
    if (ctx->special_cnt == 1) {
      if (!toggle_inline_code(ctx))
        return 0;
      goto end;
    } else if (ctx->special_cnt == 3 && ctx->line_start) {
      if (ctx->block_type == 9) {
        // end block code
        block_paragraph(ctx);
      } else {
        if (!block_code(ctx))
          return 0;
      }
      goto end;
    }
    break;
  case '^':
    if (ctx->special_cnt == 1) {
      if (!toggle_sup(ctx))
        return 0;
      goto end;
    }
    break;
  case '~':
    if (ctx->special_cnt == 1) {
      if (!toggle_sub(ctx))
        return 0;
      goto end;
    } else if (ctx->special_cnt == 2) {
      if (!toggle_strike(ctx))
        return 0;
      goto end;
    }
    break;
  case '>':
    if (ctx->special_cnt == 1 && ctx->line_start && curr_ch == ' ') {
      if (!block_quote(ctx))
        return 0;
      goto end;
    }
    break;
  }

  // if not matched, then write the special characters as regular characters
  for (int i = 0; i < ctx->special_cnt; i++) {
    if (!bufadd(&ctx->html, ctx->special_type))
      return 0;
  }
  ctx->special_cnt = 0;
  ctx->special_type = 0;
  return 1;

end:
  ctx->special_cnt = 0;
  ctx->special_type = 0;
  return 2;
}

// Count a special character and update the context. Returns 0 on error, 1 on
// success, and 2 on success and a valid special sequence has ended.
static int count_special_char(struct mdview_ctx *ctx, char ch) {
  // If the character is the same as the previous, then keep counting.
  // Otherwise, end the previous special sequence and start a new one.
  if (ctx->special_type == ch) {
    ctx->special_cnt++;
    return 1;
  } else {
    int valid_seq = end_special_sequence(ctx, ch);
    if (!valid_seq)
      return 0;
    ctx->special_cnt = 1;
    ctx->special_type = ch;
    return valid_seq;
  }
}

// Write a regular, non-special character to the buffer.
static int handle_regular_char(struct mdview_ctx *ctx, char ch) {
  // add space if this is the beginning of the line
  if (ctx->line_start)
    ctx->line_start = 0;

  if (!bufadd(&ctx->html, ch))
    return 0;
  return 1;
}

// Entry point for handling a single character.
static int handle_char(struct mdview_ctx *ctx, char ch) {
  // If this is code, then only handle handle backtick as special characters.
  // Otherwise, handle all unescaped special characters.
  int is_code;
start:
  is_code = ctx->block_type == 9 || ctx->text_decoration & 16;
  if ((is_code && ch == '`') ||
      (!is_code && !ctx->escaped && strchr("*-#`^~>", ch))) {
    int success = count_special_char(ctx, ch);
    if (!success) {
      return 0;
    } else if (success == 1) {
      // success and we didn't end a valid special sequence
      return 1;
    } else if (success == 2) {
      // success, but we had to end a special sequence, so uncount the current
      // character as a special character and restart handle_char() because
      // the current char might now be considered escaped (ie: in a code
      // block).
      ctx->escaped = 0;
      ctx->special_cnt = 0;
      goto start;
    }
  }

  // If we're here, then this isn't a special character. So we should end any
  // pending speical sequences.
  if (!end_special_sequence(ctx, ch))
    return 0;
  // re-check if we're in a code block; we might have just entered one.
  is_code = ctx->block_type == 9 || ctx->text_decoration & 16;

  // handle escaped characters
  if (ctx->escaped || is_code) {
    // handle escaped character that require rewrites
    if (ch == '<') {
      if (!bufcat(&ctx->html, "&lt;", 4))
        return 0;
      return 1;
    } else if (ch == '>') {
      if (!bufcat(&ctx->html, "&gt;", 4))
        return 0;
      return 1;
    }
    ctx->escaped = 0;
  }

  switch (ch) {
  case '\\':
    ctx->escaped = 1;
    return 1;
  case '\n':
    return handle_newline(ctx);
  default:
    return handle_regular_char(ctx, ch);
  }
}

int mdview_init(struct mdview_ctx *ctx) {
  ctx->error_msg = NULL;

  // setup the buffer
  ctx->html.buf = malloc(BUFSIZ);
  ctx->html.buf[0] = '\0';
  ctx->html.len = 0;
  ctx->html.cap = BUFSIZ;

  // setup parsing state
  ctx->feeds = 0;
  ctx->special_cnt = 0;
  ctx->special_type = 0;
  ctx->line_start = 1;

  // setup decorations state
  ctx->block_type = 0;
  ctx->indent = 0;
  ctx->escaped = 0;
  ctx->pending_link = 0;
  ctx->text_decoration = 0;

  // start with a paragraph
  if (!bufcat(&ctx->html, "<p>", 3))
    return 1;
  return 0;
}

char *mdview_feed(struct mdview_ctx *ctx, const char *md) {
  // reset the HTML buffer from the last feed, if this is not the first feed
  if (ctx->feeds > 0) {
    bufclear(&ctx->html);
    ctx->error_msg = NULL;
  }
  ctx->feeds++;

  // parse the markdown char by char
  while (*md) {
    if (!handle_char(ctx, *md))
      return NULL;
    md++;
  }

  return ctx->html.buf;
}

char *mdview_flush(struct mdview_ctx *ctx) {
  if (ctx->feeds > 0) {
    bufclear(&ctx->html);
    ctx->error_msg = NULL;
  }

  // end any pending special sequences
  if (!end_special_sequence(ctx, 0))
    return NULL;

  // end all decorations
  if (!end_all_decorations(ctx))
    return NULL;

  // end the last block
  if (!close_block(ctx))
    return NULL;

  return ctx->html.buf;
}

void mdview_free(struct mdview_ctx *ctx) {
  ctx->error_msg = NULL;
  free(ctx->html.buf);
  ctx->html.len = 0;
  ctx->html.cap = 0;
}
