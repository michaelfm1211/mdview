#include "mdview.h"
#include <stdio.h>
#include <stdlib.h>

void write_html(struct mdview_ctx *ctx, char *html) {
  if (html) {
    if (fputs(html, stdout) == EOF) {
      perror("fwrite");
      exit(EXIT_FAILURE);
    }
  } else {
    if (ctx->error_msg == NULL) {
      perror("memory error");
    } else {
      fprintf(stderr, "parsing error: %s\n", ctx->error_msg);
    }
    mdview_free(ctx);
    exit(EXIT_FAILURE);
  }
}

int main(void) {
  struct mdview_ctx ctx;
  mdview_init(&ctx);

  char buf[BUFSIZ];
  size_t len_read;
  while ((len_read = fread(buf, 1, BUFSIZ - 1, stdin))) {
    buf[len_read] = '\0';

    char *html = mdview_feed(&ctx, buf);
    write_html(&ctx, html);
  }
  if (ferror(stdin)) {
    perror("fread");
    exit(EXIT_FAILURE);
  }

  char *html = mdview_flush(&ctx);
  write_html(&ctx, html);

  mdview_free(&ctx);
  exit(EXIT_SUCCESS);
}
