#include <glib.h>
#include <tree_sitter/api.h>

#include "message.h"
#include "parse_utils.h"
#include "parser.h"
#include "process_midscope.h"
#include "processor.h"

static void
check_midscope(const gchar *content, TSNode current, GList **problems)
{
  gboolean other = FALSE;

  g_assert(content);
  g_assert(problems);

  if (ts_node_symbol(current) != SYMBOL_BODY) {
    /* Body should be anything within { ... } */
    return;
  }

  for (guint i = 0; i < ts_node_named_child_count(current); i++) {
    TSNode n = ts_node_named_child(current, i);
    struct problem *p;

    if (ts_node_symbol(n) == SYMBOL_DECLARATION) {
      if (other == TRUE) {
        gboolean found;
        TSNode symbol;

        symbol = parse_utils_get_first_node_id(n, SYMBOL_IDENTIFIER, &found);

        if (!found) {
          continue;
        }

        p = message_problem_new(3, &symbol, &symbol,
                                "Declares should be done at the start of a "
                                "body");
        *problems = g_list_prepend(*problems, p);
      }
    } else {
      other = TRUE;
    }
  }
}

static void
recurse_check(const gchar *content, TSNode p, GList **problems)
{
  g_assert(content);
  g_assert(problems);

  for (guint i = 0; i < ts_node_named_child_count(p); i++) {
    TSNode n = ts_node_named_child(p, i);

    check_midscope(content, n, problems);
    recurse_check(content, n, problems);
  }
}

GList *
process_midscope(parser_t *parser, G_GNUC_UNUSED struct process_ctx *ctx)
{
  GList *res = NULL;
  if (parser->message->type == MESSAGE_TYPE_DIAGNOSTIC ||
      parser->message->type == MESSAGE_TYPE_OPEN ||
      parser->message->type == MESSAGE_TYPE_CHANGE) {
    recurse_check(parser->content, parser->root_node, &res);
  }
  return res;
}
