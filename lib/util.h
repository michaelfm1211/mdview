#pragma once

#include "mdview.h"

/*
 * Buffer-related functions.
 */

// Add a single character to the buffer.
int bufadd(struct mdview_buf *buf, char ch);
// Concatenates a string str, of length str_len, to the buffer.
int bufcat(struct mdview_buf *buf, char *str, size_t str_len);
// Sets a buffer to an empty string without memset-ing the whole thing.
void bufclear(struct mdview_buf *buf);
