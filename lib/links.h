#pragma once

#include "mdview.h"

int handle_link_special_char(struct mdview_ctx *ctx, char ch);

// End a link. If it is valid, then write the HTMl to the buffer. If it is
// invalid, then write the special characters to the buffer as regular
// characters.
int end_link(struct mdview_ctx *ctx);
