#include "parser.h"
#include "mdview.h"
#include "tags.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

// Handle a newline character and update figure out which block elements need
// to be created/closed.
static int handle_newline(struct mdview_ctx *ctx) {
  if (ctx->line_start && ctx->block_type != 9) {
    // if there has been two consequetive newlines, then close the current
    // block.
    close_block(ctx);
  } else {
    ctx->line_start = 1;
    ctx->indent = 0;
  }

  // different newline behavior depending on block
  switch (ctx->block_type) {
  case -1:
    return 1;
  case 0:  // paragraph
  case 7:  // unordered list
  case 8:  // ordered list
  case 10: // blockquote
    return bufadd(ctx->curr_buf, ' ');
  case 1: // headers
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
    return close_block(ctx);
  case 9:
    return bufadd(ctx->curr_buf, '\n');
  default:
    fprintf(stderr, "Undefined newline behavior for block type %d\n",
            ctx->block_type);
    return 0;
  }

  return 1;
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
  // this is a regular char, so the escape should expire after this char.
  ctx->escaped = 0;

  // end the current link if we're in one and the link is invalid
  if (ctx->pending_link && ctx->temp_buf.len > 0) {
    char last_link_char = ctx->temp_buf.buf[ctx->temp_buf.len - 1];
    // invalidate this link if we're between the text and the URL, or if there
    // is a space in the URL.
    if ((ctx->pending_link == 1 && last_link_char == '\0') ||
        (ctx->pending_link == 2 && ch == ' ')) {
      if (!end_link(ctx))
        return 0;
    }
  }

  // add space if this is the beginning of the line
  if (ctx->line_start && ctx->block_type != 0)
    ctx->line_start = 0;

  // if we have nowhere to write to, then start a paragraph
  if (ctx->block_type == -1)
    block_paragraph(ctx);

  if (!bufadd(ctx->curr_buf, ch))
    return 0;
  return 1;
}

int end_special_sequence(struct mdview_ctx *ctx, char curr_ch) {
  // quick heuristic to see if we should even bother trying to match a special
  // sequence
  if (ctx->special_cnt == 0)
    return 1;

  // try to match to a valid special sequence
  switch (ctx->special_type) {
  case '*':
    if (ctx->special_cnt == 1 && ctx->line_start && curr_ch == ' ') {
      if (!unordered_list_item(ctx, ctx->special_type))
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
      // WARNING: this might close tags out of order, making slightly invalid
      // HTML, but the browser is able to handle it.
      if (!toggle_italics(ctx))
        return 0;
      if (!toggle_bold(ctx))
        return 0;
      goto end;
    }
    break;
  case '-':
    if (ctx->special_cnt == 1 && ctx->line_start && curr_ch == ' ') {
      if (!unordered_list_item(ctx, ctx->special_type))
        return 1;
      goto end;
    } else if (ctx->special_cnt == 3 && ctx->line_start && curr_ch == '\n') {
      if (!bufcat(ctx->curr_buf, "<hr>", 4))
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
    // we need to be careful here, this can be called from inside a code block!
    if (ctx->special_cnt == 1 && ctx->block_type != 9) {
      // don't do this if we're in a code block
      if (!toggle_inline_code(ctx))
        return 0;
      goto end;
    } else if (ctx->special_cnt >= 3 && ctx->line_start) {
      if (ctx->block_type == 9 && ctx->special_cnt == ctx->block_subtype) {
        // end block code if we're in a code block and the number of backticks
        // matches the number of backticks that started the block
        close_block(ctx);
      } else {
        if (!block_code(ctx, ctx->special_cnt))
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
  case '+':
    if (ctx->special_cnt == 1 && ctx->line_start && curr_ch == ' ') {
      if (!unordered_list_item(ctx, ctx->special_type))
        return 0;
      goto end;
    }
    break;
  }

  // if not matched, then write the special characters as regular characters
  for (; ctx->special_cnt > 0; ctx->special_cnt--) {
    handle_regular_char(ctx, ctx->special_type);
  }
  ctx->special_cnt = 0;
  ctx->special_type = 0;
  return 1;

end:
  ctx->special_cnt = 0;
  ctx->special_type = 0;
  return 2;
}

int end_link(struct mdview_ctx *ctx) {
  // return if we're not in a link
  if (!ctx->pending_link)
    return 1;

  // if we haven't finished the link, then just write the text as regular
  if (ctx->temp_buf.len == 0)
    return bufadd(&ctx->html_out, '[');
  char last_char = ctx->temp_buf.buf[ctx->temp_buf.len - 1];
  if (ctx->pending_link == 1) {
    if (!bufadd(&ctx->html_out, '['))
      return 0;

    if (last_char == '\0') {
      // ended text, but before the URL
      if (!bufcat(&ctx->html_out, ctx->temp_buf.buf, ctx->temp_buf.len - 1) ||
          !bufadd(&ctx->html_out, ']'))
        return 0;
      goto end;
    } else {
      // ended in the middle of the text
      if (!bufcat(&ctx->html_out, ctx->temp_buf.buf, ctx->temp_buf.len))
        return 0;
      goto end;
    }
  } else if (ctx->pending_link == 2 && last_char != '\0') {
    // ended in the middle of the URL
    size_t text_len = strlen(ctx->temp_buf.buf);
    if (!bufadd(&ctx->html_out, '[') ||
        !bufcat(&ctx->html_out, ctx->temp_buf.buf, text_len) ||
        !bufcat(&ctx->html_out, "](", 2) ||
        !bufcat(&ctx->html_out, ctx->temp_buf.buf + text_len + 1,
                ctx->temp_buf.len - text_len - 1))
      return 0;
    goto end;
  }

  // The temporary buffer is split into two consecutive strings, the first is
  // the text of the link, and the second is the URL of the link.
  char *text = ctx->temp_buf.buf;
  char *url = strchr(ctx->temp_buf.buf, '\0') + 1;
  // If the URL is empty, then use the text as the URL.
  if (*url == '\0')
    url = text;

  ctx->curr_buf = &ctx->html_out;
  if (!write_link(ctx, url, text))
    return 0;

end:
  ctx->curr_buf = &ctx->html_out;
  bufclear(&ctx->temp_buf);
  ctx->pending_link = 0;
  return 1;
}

int handle_char(struct mdview_ctx *ctx, char ch) {
  // If this is code, then only handle handle backtick as special characters.
  // Otherwise, handle all unescaped special characters.
  int is_code;
start:
  is_code = ctx->block_type == 9 || ctx->text_decoration & 16;
  if ((is_code && ch == '`') ||
      (!is_code && !ctx->escaped && strchr("*-#`^~>+", ch))) {
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

  // handle link special characters, if any
  switch (ch) {
  case '[':
    // start link
    if (!ctx->escaped && !is_code && !ctx->pending_link) {
      ctx->curr_buf = &ctx->temp_buf;
      ctx->pending_link = 1;
      return 1;
    }
    return handle_regular_char(ctx, ch);
  case ']':
    // end of the text part.
    if (ctx->pending_link)
      return bufadd(&ctx->temp_buf, '\0');
    return handle_regular_char(ctx, ch);
  case '(':
    // beginning of the URL part. Note: there might be characters between the
    // ']' and the '(' that invalidate the link, so this case and the previous
    // one must be separate. hangle_regular_char() will prematurely end the
    // link if it encounters a character that isn't part of a link.
    if (ctx->pending_link) {
      ctx->pending_link = 2;
      return 1;
    }
    return handle_regular_char(ctx, ch);
  case ')':
    // end of the URL part, and the link as a whole. end the link and write it.
    if (ctx->pending_link) {
      if (!bufadd(&ctx->temp_buf, '\0'))
        return 0;
      return end_link(ctx);
    }
    return handle_regular_char(ctx, ch);
  }

  // handle escaped characters that must be rewritten
  if (ctx->escaped || is_code) {
    // handle escaped character that require rewrites
    if (ch == '<') {
      if (!bufcat(ctx->curr_buf, "&lt;", 4))
        return 0;
      return 1;
    } else if (ch == '>') {
      if (!bufcat(ctx->curr_buf, "&gt;", 4))
        return 0;
      return 1;
    } else if (ch == '\\') {
      if (!bufcat(ctx->curr_buf, "&#92;", 5))
        return 0;
      return 1;
    }
  }

  // handle regular characters and special characters that aren't part of a
  // special sequence.
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
