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

/*
 * DECORATION AND TAGS
 */

static int toggle_italics(struct mdview_ctx *ctx) {
  if (ctx->text_decoration & 1) {
    if (!bufcat(&ctx->html, "</i>", 4))
      return 0;
  } else {
    if (!bufcat(&ctx->html, "<i>", 3))
      return 0;
  }
  ctx->text_decoration ^= 1;
  return 1;
}

static int toggle_bold(struct mdview_ctx *ctx) {
  if (ctx->text_decoration & 2) {
    if (!bufcat(&ctx->html, "</b>", 4))
      return 0;
  } else {
    if (!bufcat(&ctx->html, "<b>", 3))
      return 0;
  }
  ctx->text_decoration ^= 2;
  return 1;
}

static int toggle_inline_code(struct mdview_ctx *ctx) {
  if (ctx->text_decoration & 16) {
    if (!bufcat(&ctx->html, "</code>", 7))
      return 0;
  } else {
    if (!bufcat(&ctx->html, "<code>", 6))
      return 0;
  }
  ctx->text_decoration ^= 16;
  return 1;
}

static int toggle_sup(struct mdview_ctx *ctx) {
  if (ctx->text_decoration & 32) {
    if (!bufcat(&ctx->html, "</sup>", 6))
      return 0;
  } else {
    if (!bufcat(&ctx->html, "<sup>", 5))
      return 0;
  }
  ctx->text_decoration ^= 32;
  return 1;
}

static int toggle_sub(struct mdview_ctx *ctx) {
  if (ctx->text_decoration & 8) {
    if (!bufcat(&ctx->html, "</sub>", 6))
      return 0;
  } else {
    if (!bufcat(&ctx->html, "<sub>", 5))
      return 0;
  }
  ctx->text_decoration ^= 8;
  return 1;
}

static int toggle_strike(struct mdview_ctx *ctx) {
  if (ctx->text_decoration & 4) {
    if (!bufcat(&ctx->html, "</s>", 4))
      return 0;
  } else {
    if (!bufcat(&ctx->html, "<s>", 3))
      return 0;
  }
  ctx->text_decoration ^= 4;
  return 1;
}

static int toggle_code_block(struct mdview_ctx *ctx) {
  if (ctx->block_type == 8) {
    ctx->block_type = 0;
    if (!bufcat(&ctx->html, "</code></pre>", 13))
      return 0;
  } else {
    ctx->block_type = 8;
    if (!bufcat(&ctx->html, "<pre><code>", 11))
      return 0;
  }
  return 1;
}

static int list_item(struct mdview_ctx *ctx) {
  if (!bufcat(&ctx->html, "<li>", 4))
    return 0;
  ctx->list_type = 1;
  ctx->block_type = 7;
  return 1;
}

static int header(struct mdview_ctx *ctx, int level) {
  char open_tag[4] = {'<', 'h', '0' + level, '>'};
  if (!bufcat(&ctx->html, open_tag, 4))
    return 0;
  ctx->block_type = level;
  return 1;
}

/*
 * PARSING FUNCTIONS
 */

// Handle a newline character and update figure out which block elements need
// to be created/closed.
static int handle_newline(struct mdview_ctx *ctx) {
  if (ctx->line_start && ctx->block_type != 8) {
    // two consequetive newlines, end the current block
    switch (ctx->block_type) {
    case 0:
      if (!bufcat(&ctx->html, "</p><p>", 7))
        return 0;
      break;
    case 7:
      if (!bufcat(&ctx->html, "</li><p>", 8))
        return 0;
      ctx->list_type = 0;
      break;
    }
    ctx->block_type = 0;
  } else {
    ctx->line_start = 1;
  }

  // if there's a header block, end it at the new line (no multi-line headers)
  if (ctx->block_type >= 1 && ctx->block_type <= 6) {
    if (!bufcat(&ctx->html, "</h", 3))
      return 0;
    if (!bufadd(&ctx->html, '0' + ctx->block_type))
      return 0;
    if (!bufcat(&ctx->html, "><p>", 4))
      return 0;
    ctx->block_type = 0;
  }

  // if there's a code block, then we want to print a newline character.
  // otherwise, add a space character between words.
  if (ctx->block_type == 8) {
    if (!bufadd(&ctx->html, '\n'))
      return 0;
  } else {
    if (!bufadd(&ctx->html, ' '))
      return 0;
  }

  return 1;
}

// End a special sequence. If it is valid, then write the HTML to the buffer.
// If it is invalid, then write the special characters to the buffer as regular
// characters. Returns 0 on error, 1 on success but no result because the
// sequence was invalid, and 2 on success with a valid sequence.
// NOTE: all special characters must be added in handle_char() too.
static int end_special_sequence(struct mdview_ctx *ctx, char curr_ch) {
  // try to match to a valid special sequence
  switch (ctx->special_type) {
  case '*':
    if (ctx->special_cnt == 1 && ctx->line_start && curr_ch == ' ') {
      if (!list_item(ctx))
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
      if (!list_item(ctx))
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
      if (!header(ctx, ctx->special_cnt))
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
      if (!toggle_code_block(ctx))
        return 0;
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
  is_code = ctx->block_type == 8 || ctx->text_decoration & 16;
  if ((is_code && ch == '`') ||
      (!is_code && !ctx->escaped && strchr("*-#`^~", ch))) {
    int success = count_special_char(ctx, ch);
    if (!success) {
      return 0;
    } else if (success == 1) {
      // success and we didn't end a valid special sequence
      return 1;
    } else if (success == 2) {
      // success, but we had to end a special sequence, so uncount the current
      // character as a special character and restart handle_char() because the
      // current char might now be considered escaped (ie: in a code block).
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
  is_code = ctx->block_type == 8 || ctx->text_decoration & 16;

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
  ctx->escaped = 0;
  ctx->pending_link = 0;
  ctx->list_type = 0;
  ctx->text_decoration = 0;

  // start with a paragraph
  if (!bufcat(&ctx->html, "<p>", 3))
    return 1;
  return 0;
}

char *mdview_feed(struct mdview_ctx *ctx, const char *md) {
  // reset the HTML buffer from the last feed, if this is not the first feed
  if (ctx->feeds > 0) {
    ctx->html.buf[0] = '\0';
    ctx->html.len = 0;
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
  ctx->error_msg = NULL;
  return "";
}

void mdview_free(struct mdview_ctx *ctx) {
  ctx->error_msg = NULL;
  free(ctx->html.buf);
  ctx->html.len = 0;
  ctx->html.cap = 0;
}
