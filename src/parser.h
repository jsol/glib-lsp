#pragma once
#include <glib.h>
#include <tree_sitter/api.h>

#include "message.h"

G_BEGIN_DECLS

struct parser_ctx {
  message_t *message;
  gchar *content;
  gchar *file;
  gchar *language;
  TSParser *parser;
  TSTree *tree;
  TSNode root_node;
};
typedef struct parser_ctx parser_t;

parser_t *parser_new(message_t *msg, GHashTable *files);

void parser_unref(parser_t *parser);

parser_t *parser_ref(parser_t *parser);
G_END_DECLS
