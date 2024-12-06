#include <glib.h>
#include <tree_sitter/api.h>

#include "message.h"
#include "parse_utils.h"
#include "parser.h"
#include "processor.h"
#include "process_asserts.h"

#define DEBUG FALSE
static void
collect_asserts(GHashTable *res, const gchar *content, TSNode check)
{
  g_assert(res);
  g_assert(content);

  for (guint i = 0; i < ts_node_named_child_count(check); i++) {
    TSNode n = ts_node_named_child(check, i);

    if (ts_node_symbol(n) != SYMBOL_CALL_EXPRESSION) {
      collect_asserts(res, content, n);
      continue;
    }

    TSNode function = ts_node_child_by_field_name(n, "function",
                                                  strlen("function"));
    TSNode args = ts_node_child_by_field_name(n, "arguments",
                                              strlen("arguments"));
    TSNode arg;

    if (ts_node_is_null(function) || ts_node_is_null(args)) {
      return;
    }

    if (ts_node_named_child_count(args) != 1) {
      /* g_assert should have exactly 1 arg */
      return;
    }
    if (!parse_utils_node_eq(content, &function, "g_assert")) {
      return;
    }

    arg = ts_node_named_child(args, 0);
    g_hash_table_insert(res, parse_utils_node_get_string(content, &arg), &arg);
    return;
  }
}

static gboolean
mentioned_with_null(G_GNUC_UNUSED const gchar *content, G_GNUC_UNUSED const gchar *var, G_GNUC_UNUSED TSNode function)
{
  return FALSE;
}

static void
check_asserts(const gchar *content, TSNode function, GList **problems)
{
  GHashTable *asserts;
  TSNode decl;
  TSNode params;

  g_assert(content);
  g_assert(problems);

  if (ts_node_symbol(function) != SYMBOL_FUNCTION) {
    return;
  }
  if (!parse_utils_is_function_static(content, function)) {
    return;
  }

  asserts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  collect_asserts(asserts, content, function);

  decl = ts_node_child_by_field_name(function, "declarator",
                                     strlen("declarator"));
  if (ts_node_is_null(decl)) {
    goto ok;
  }
  params = ts_node_child_by_field_name(decl, "parameters", strlen("parameters"));
  if (ts_node_is_null(params)) {
    goto ok;
  }

  for (guint i = 0; i < ts_node_named_child_count(params); i++) {
    gchar *param = NULL;
    TSNode param_node;
    g_message("Checking %d", i);

    param_node = ts_node_named_child(params, i);
    if (parse_utils_parameter_is_pointer(content, param_node,
                                         &param)) {
      if (!g_hash_table_contains(asserts, param) &&
          !mentioned_with_null(content, param, function)) {
        struct problem *p;
        TSPoint start = ts_node_start_point(param_node);
        TSPoint end = ts_node_end_point(param_node);
        p = message_problem_new(3, start.row, start.column, end.row, end.column,
                                "Parameter %s should be asserted", param);
        g_message("Found problem with %s", param);
        *problems = g_list_prepend(*problems, p);
      }
      g_free(param);
    }
  }

  /* Fall through */
ok:
  g_hash_table_unref(asserts);
}

static void
recurse_check(const gchar *content,
              TSNode p,
              const gchar *level,
              GList **problems)
{
  gchar *next_level = g_strdup_printf("  %s", level);
  for (guint i = 0; i < ts_node_named_child_count(p); i++) {
    TSNode n = ts_node_named_child(p, i);

    if (DEBUG) {
      guint s = ts_node_start_byte(n);
      guint e = ts_node_end_byte(n);

      gchar *c = g_strndup(content + s, e - s);
      TSSymbol sy = ts_node_symbol(n);
      const gchar *fn = ts_node_field_name_for_child(p, i);
      g_debug("Node: %s %s (%s) %s, %u", level, ts_node_type(n), fn, c, sy);
      g_free(c);
    }

    check_asserts(content, n, problems);
    g_message("Checking level...");
    recurse_check(content, n, next_level, problems);
  }
  g_free(next_level);
}

GList *
process_asserts(parser_t *parser, G_GNUC_UNUSED struct process_ctx *ctx)
{
  GList *res = NULL;
  if (parser->message->type == MESSAGE_TYPE_DIAGNOSTIC ||
      parser->message->type == MESSAGE_TYPE_OPEN ||
      parser->message->type == MESSAGE_TYPE_CHANGE) {
    recurse_check(parser->content, parser->root_node, "", &res);
    g_message("done checking");
  }
  return res;
}
