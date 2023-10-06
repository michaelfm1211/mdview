#include "mdview.h"
#include "util.h"
#include "tags.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int __attribute__((visibility("default"))) mdview_init(struct mdview_ctx *ctx) {
  ctx->error_msg = NULL;

  // setup the HTML buffer
  ctx->html_out.buf = malloc(BUFSIZ);
  ctx->html_out.buf[0] = '\0';
  ctx->html_out.len = 0;
  ctx->html_out.cap = BUFSIZ;

  // setup the temporary buffer
  ctx->temp_buf.buf = NULL;
  ctx->temp_buf.len = 0;
  ctx->temp_buf.cap = 0;

  // set the default buffer to the HTML buffer
  ctx->curr_buf = &ctx->html_out;

  // setup parsing state
  ctx->feeds = 0;
  ctx->special_cnt = 0;
  ctx->special_type = 0;
  ctx->line_start = 1;

  // setup decorations state
  ctx->block_type = -1;
  ctx->block_subtype = 0;
  ctx->indent = 0;
  ctx->escaped = 0;
  ctx->pending_link = 0;
  ctx->text_decoration = 0;

  return 0;
}

char *__attribute__((visibility("default")))
mdview_feed(struct mdview_ctx *ctx, const char *md) {
  // reset the HTML buffer from the last feed, if this is not the first feed
  if (ctx->feeds > 0) {
    bufclear(&ctx->html_out);
    ctx->error_msg = NULL;
  }
  ctx->feeds++;

  // parse the markdown char by char
  while (*md) {
    if (!handle_char(ctx, *md))
      return NULL;
    md++;
  }

  return ctx->html_out.buf;
}

char *__attribute__((visibility("default")))
mdview_flush(struct mdview_ctx *ctx) {
  if (ctx->feeds > 0) {
    bufclear(&ctx->html_out);
    ctx->error_msg = NULL;
  }

  // end any pending special sequences
  if (!end_special_sequence(ctx, 0))
    return NULL;

  // end any pending special sequences
  if (!end_link(ctx))
    return NULL;

  // end all decorations
  if (!end_all_decorations(ctx))
    return NULL;

  // end the last block
  if (!close_block(ctx))
    return NULL;

  return ctx->html_out.buf;
}

void __attribute__((visibility("default")))
mdview_free(struct mdview_ctx *ctx) {
  ctx->error_msg = NULL;

  // free the HTML buffer
  free(ctx->html_out.buf);
  ctx->html_out.len = 0;
  ctx->html_out.cap = 0;

  // free temporary buffer
  free(ctx->temp_buf.buf);
  ctx->temp_buf.len = 0;
  ctx->temp_buf.cap = 0;
}
