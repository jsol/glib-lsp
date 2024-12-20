#include <glib.h>
#include <tree_sitter/api.h>

#include "parser.h"

const TSLanguage *tree_sitter_c(void);

static void
clear(gpointer data)
{
  struct parser_ctx *ctx = (struct parser_ctx *) data;
  if (ctx == NULL) {
    return;
  }

  message_free(ctx->message);
  g_free(ctx->content);
  g_free(ctx->file);
  g_free(ctx->language);
  /* Free tree-sitter stuff */
  ts_tree_delete(ctx->tree);
  ts_parser_delete(ctx->parser);
}

parser_t *
parser_new(message_t *msg, GHashTable *files)
{
  parser_t *parser;
  parser = g_atomic_rc_box_new0(struct parser_ctx);
  parser->message = msg;
  switch (msg->type) {
  case MESSAGE_TYPE_OPEN:
    parser->content = g_strdup(msg->data.open.text);
    parser->file = g_strdup(msg->data.open.uri);
    break;
  case MESSAGE_TYPE_CHANGE:
    parser->content = g_strdup(msg->data.change.text);
    parser->file = g_strdup(msg->data.change.uri);
    break;
  case MESSAGE_TYPE_DIAGNOSTIC:
    parser->content = g_strdup(msg->data.diagnostic.document.text);
    parser->file = g_strdup(msg->data.diagnostic.document.uri);
    break;
  default:
    /* Ignore */
    g_message("ignoring type %u", msg->type);
  }

  if (parser->content != NULL) {
    g_hash_table_insert(files, g_strdup(parser->file),
                        g_strdup(parser->content));
  } else if (parser->file != NULL) {
    parser->content = g_strdup(g_hash_table_lookup(files, parser->file));
  }

  if (parser->content != NULL) {
    parser->parser = ts_parser_new();

    // Set the parser's language (JSON in this case).
    ts_parser_set_language(parser->parser, tree_sitter_c());

    // Build a syntax tree based on source code stored in a string.
    parser->tree = ts_parser_parse_string(parser->parser, NULL, parser->content,
                                          strlen(parser->content));

    // Get the root node of the syntax tree.
    parser->root_node = ts_tree_root_node(parser->tree);
  }

  return parser;
}

parser_t*
parser_ref(parser_t *ctx) {
    return g_atomic_rc_box_acquire(ctx);
}

void
parser_unref(parser_t *ctx)
{
  if (ctx == NULL) {
    return;
  }
  g_atomic_rc_box_release_full(ctx, clear);
}
