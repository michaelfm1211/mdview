#pragma once

#include "mdview.h"

// End a special sequence. If it is valid, then write the HTML to the buffer.
// If it is invalid, then write the special characters to the buffer as
// regular characters. Returns 0 on error, 1 on success but no result because
// the sequence was invalid, and 2 on success with a valid sequence. NOTE: all
// special characters must be added in handle_char() too.
int end_special_sequence(struct mdview_ctx *ctx, char curr_ch);

// End a link. If it is valid, then write the HTMl to the buffer. If it is
// invalid, then write the special characters to the buffer as regular
// characters.
int end_link(struct mdview_ctx *ctx);

// Entry point for handling a single character.
int handle_char(struct mdview_ctx *ctx, char ch);
