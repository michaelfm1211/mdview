#include "links.h"
#include "mdview.h"
#include "parser.h"
#include "tags.h"
#include "util.h"
#include <string.h>

// 0 means error, -1 = success but no link handled, 1 = success and link handled
int handle_link_special_char(struct mdview_ctx *ctx, char ch) {
  switch (ch) {
  case '!':
    // start an image link
    if (!ctx->pending_link) {
      ctx->curr_buf = &ctx->temp_buf;
      ctx->image_link = 1;
      return 1;
    }
  case '[':
    // start link
    if (!ctx->pending_link) {
      if (ctx->image_link)
        bufclear(&ctx->temp_buf);
      else
        ctx->curr_buf = &ctx->temp_buf;
      ctx->pending_link = 1;
      return 1;
    }
    break;
  case ']':
    // end of the text part.
    if (ctx->pending_link)
      return bufadd(&ctx->temp_buf, '\0');
    break;
  case '(':
    // beginning of the URL part. Note: there might be characters between the
    // ']' and the '(' that invalidate the link, so this case and the previous
    // one must be separate. hangle_regular_char() will prematurely end the
    // link if it encounters a character that isn't part of a link.
    if (ctx->pending_link) {
      ctx->pending_link = 2;
      return 1;
    }
    break;
  case ')':
    // end of the URL part, and the link as a whole. end the link and write it.
    if (ctx->pending_link) {
      if (!bufadd(&ctx->temp_buf, '\0'))
        return 0;
      return end_link(ctx);
    }
    break;
  }

  // we had previously predicted an image link, but turns out its not. now we
  // have to print "!"
  if (ctx->image_link && !ctx->pending_link) {
    ctx->curr_buf = &ctx->html_out;
    ctx->image_link = 0;
    if (!handle_regular_char(ctx, '!'))
      return 0;
  }
  
  return -1;
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
  ctx->image_link = 0;
  return 1;
}
