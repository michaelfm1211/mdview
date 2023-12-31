#include "parser.h"
#include "links.h"
#include "mdview.h"
#include "tags.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

// Handle a newline character and update figure out which block elements need
// to be created/closed.
static int handle_newline(struct mdview_ctx *ctx) {
  if (ctx->line_start && ctx->block_type != -1 && ctx->block_type != 9) {
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

int handle_regular_char(struct mdview_ctx *ctx, char ch) {
  // this is a regular char, so the escape should expire after this char.
  ctx->escaped = 0;
  ctx->line_start = 0;

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
    if (ctx->special_cnt >= 1 && ctx->special_cnt <= 6 && ctx->line_start &&
        curr_ch == ' ') {
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

  // If we're here, then this isn't a special sequence. So we should end any
  // pending speical sequences.
  if (!end_special_sequence(ctx, ch))
    return 0;
  // re-check if we're in a code block; we might have just entered one.
  is_code = ctx->block_type == 9 || ctx->text_decoration & 16;

  // handle link special characters, if any. return if one was handled.
  if (!is_code && !ctx->escaped) {
    int link_handled = handle_link_special_char(ctx, ch);
    if (link_handled == 0 || link_handled == 1)
      return link_handled;
  }

  // handle escaped characters that must be rewritten
  if (ctx->escaped || is_code) {
    // handle escaped character that require rewrites
    if (ch == '<') {
      return bufcat(ctx->curr_buf, "&lt;", 4);
    } else if (ch == '>') {
      return bufcat(ctx->curr_buf, "&gt;", 4);
    } else if (ch == '\\') {
      return bufcat(ctx->curr_buf, "&#92;", 5);
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
