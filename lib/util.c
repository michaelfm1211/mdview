#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int bufadd(struct mdview_buf *buf, char ch) {
  // make sure there is enough space in the buffer
  while (buf->len + 1 >= buf->cap) {
    // if we haven't used this buffer before, then make it the default size
    if (buf->cap == 0)
      buf->cap = BUFSIZ;

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

int bufcat(struct mdview_buf *buf, char *str, size_t str_len) {
  // make sure there is enough space in the buffer
  if (buf->len + str_len >= buf->cap) {
    // if we haven't used this buffer before, then make it the default size
    if (buf->cap == 0)
      buf->cap = BUFSIZ;

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

void bufclear(struct mdview_buf *buf) {
  buf->len = 0;
  buf->buf[0] = '\0';
}
