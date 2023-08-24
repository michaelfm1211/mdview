#pragma once

#include <stddef.h>

struct mdview_buf {
  char *buf;
  size_t len;
  size_t cap;
};

struct mdview_ctx {
  // Error message, or NULL if no error.
  const char *error_msg;

  // Contains finished HTML that has been generated and is about to be returned.
  struct mdview_buf html;

  // Parser state
  int feeds; // number of times mdview_feed has been called
  unsigned int special_cnt; // Count consequetive special characters (#, *, `,
                            // etc.).
  char special_type; // what type of special character is being counted. NULL is
                     // none, anythign else is the character being counted.
  int line_start;    // 0 = not beginng of line, 1 = beginning of line
  int indent;        // number of indents at the beginning of this line

  // Decorations/block state
  unsigned int block_type; // 0 = text, 1-6 = heading, 7 = unordered list, 8 =
                           // ordered list, 9 = code block, 10 = blockquote
  unsigned int block_subtype; // 0 = unused. For lists, first 8 bits are the
                              // starter used (1 = -, 2 = +, 3 = *), and next 8
                              // bits are the number of idents. In code blocks,
                              // this is the number of backticks used.
  int escaped;                // 0 = no, 1 = yes
  int pending_link;           // 0 = no, 1 = yes
  unsigned int text_decoration; // bit field: 1 = italics, 2 = bold, 4 = strike,
                                // 8 = subscript, 16 = inline code,
                                // 32 = superscript
};

/**
 * Initialize the mdview context. This must be called before any markdown is
 * fed to the parser.
 * @param ctx The context to initialize.
 * @return 0 on failure, 1 on success.
 */
int mdview_init(struct mdview_ctx *ctx);

/**
 * Feed some markdown to the parser, update the context, and return any HTML
 * that has been generated. Do not free the result, it is owned by the context.
 * @param ctx The context to update.
 * @param md The markdown to parse as a NULL-terminated string.
 * @return Any HTML that has been generated as a NULL-terminated string, or NULL
 *         if an error occured (error is in ctx->error_msg; error is NULL if a
 *         memory-related error occured).
 */
char *mdview_feed(struct mdview_ctx *ctx, const char *md);

/**
 * Return any pending HTML that has been generated but unfinished. You likely
 * want to call this function after you have fed all of your markdown to the
 * parser, but before you free the context. This function will allow you to
 * catch any edge cases in user input that would otherwise be missed. Do not
 * free the result, it is owned by the context.
 * @param ctx The context to flush.
 * @return Any HTML that has been generated as a NULL-terminated string, or NULL
 *         if an error occured (error is in ctx->error_msg; error is NULL if a
 *         memory-related error occured).
 */
char *mdview_flush(struct mdview_ctx *ctx);

/**
 * Free any resources associated with the context. Note: this does not free the
 * context itself, you must do that yourself.
 * @param ctx The context that owns the resources to free.
 */
void mdview_free(struct mdview_ctx *ctx);
