#pragma once

#include "mdview.h"

/*
 * Decorations
 */

// Toggle a decoration
int toggle_italics(struct mdview_ctx *ctx);
int toggle_bold(struct mdview_ctx *ctx);
int toggle_strike(struct mdview_ctx *ctx);
int toggle_sub(struct mdview_ctx *ctx);
int toggle_inline_code(struct mdview_ctx *ctx);
int toggle_sup(struct mdview_ctx *ctx);

// Closes all decorations. This is called by mdview_flush()
int end_all_decorations(struct mdview_ctx *ctx);

/*
 * Blocks
 */

// Close the current block
int close_block(struct mdview_ctx *ctx);

// Open a new block (closes the current one with close_block)
int block_paragraph(struct mdview_ctx *ctx);
int block_unordered_list(struct mdview_ctx *ctx, char starter);
int block_ordered_list(struct mdview_ctx *ctx);
int block_code(struct mdview_ctx *ctx, unsigned int fence_len);
int block_quote(struct mdview_ctx *ctx);
int block_header(struct mdview_ctx *ctx, int level);

// Create a new unordered list item and calls block_unordered_list() if needed
int unordered_list_item(struct mdview_ctx *ctx, char starter);

// Write the currently
int write_link(struct mdview_ctx *ctx, char *url, char *text);
