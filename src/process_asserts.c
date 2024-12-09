#include <glib.h>
#include <tree_sitter/api.h>

#include "message.h"
#include "parse_utils.h"
#include "parser.h"
#include "process_asserts.h"
#include "processor.h"

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

static TSNode
get_first_node_id(TSNode check, guint id)
{
  for (guint i = 0; i < ts_node_named_child_count(check); i++) {
    TSNode n = ts_node_named_child(check, i);
    if (ts_node_symbol(n) == id) {
      return n;
    }

    n = get_first_node_id(n, id);
    if (ts_node_symbol(n) == id) {
      return n;
    }
  }
  return check;
}

static void
collect_renames(GHashTable *res, const gchar *content, TSNode check)
{
  g_assert(res);
  g_assert(content);

  for (guint i = 0; i < ts_node_named_child_count(check); i++) {
    TSNode n = ts_node_named_child(check, i);

    if (ts_node_symbol(n) != SYMBOL_DECLARATION) {
      collect_renames(res, content, n);
      continue;
    }

    TSNode decl = ts_node_child_by_field_name(n, "declarator",
                                              strlen("declarator"));
    TSNode to = get_first_node_id(decl, SYMBOL_IDENTIFIER);

    TSNode from = get_first_node_id(decl, SYMBOL_CAST_EXPRESSION);

    if (ts_node_symbol(from) != SYMBOL_CAST_EXPRESSION) {
      continue;
    }
    from = get_first_node_id(from, SYMBOL_IDENTIFIER);

    if (ts_node_symbol(from) != SYMBOL_IDENTIFIER ||
        ts_node_symbol(to) != SYMBOL_IDENTIFIER) {
      continue;
    }
    g_hash_table_insert(res, parse_utils_node_get_string(content, &from),
                        parse_utils_node_get_string(content, &to));
    return;
  }
}

static gboolean
mentioned_with_null(const gchar *content, const gchar *var, TSNode function)
{
  gboolean res = FALSE;
  gchar *var_lower = NULL;
  gchar *comment = NULL;
  gchar *comment_lower = NULL;

  g_assert(content);
  g_assert(var);

  var_lower = g_utf8_strdown(var, -1);

  for (guint i = 0; i < ts_node_named_child_count(function); i++) {
    TSNode n = ts_node_named_child(function, i);
    if (ts_node_symbol(n) == SYMBOL_COMMENT) {
      comment = parse_utils_node_get_string(content, &n);
      comment_lower = g_utf8_strdown(comment, -1);

      if (g_strstr_len(comment_lower, -1, var_lower) != NULL &&
          g_strstr_len(comment_lower, -1, "null") != NULL) {
        res = TRUE;
        goto out;
      }
      g_clear_pointer(&comment, g_free);
      g_clear_pointer(&comment_lower, g_free);
    }
    res = mentioned_with_null(content, var, n);
    if (res) {
      goto out;
    }
  }

  /* Fall through */
out:
  g_clear_pointer(&var_lower, g_free);
  g_clear_pointer(&comment, g_free);
  g_clear_pointer(&comment_lower, g_free);
  return res;
}

static gboolean
renamed_assert(GHashTable *asserts, GHashTable *renames, const gchar *param)
{
  const gchar *p;

  g_assert(asserts);
  g_assert(renames);
  g_assert(param);

  if (!g_hash_table_contains(renames, param)) {
    return FALSE;
  }

  p = g_hash_table_lookup(renames, param);

  if (g_hash_table_contains(asserts, p)) {
    return TRUE;
  }

  return renamed_assert(asserts, renames, p);
}

static void
check_asserts(const gchar *content, TSNode function, GList **problems)
{
  GHashTable *asserts;
  GHashTable *renames;
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
  renames = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  collect_asserts(asserts, content, function);
  collect_renames(renames, content, function);

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

    param_node = ts_node_named_child(params, i);
    if (!parse_utils_parameter_is_unused(content, param_node) &&
        parse_utils_parameter_is_pointer(content, param_node, &param)) {
      if (!g_hash_table_contains(asserts, param) &&
          !mentioned_with_null(content, param, function) &&
          !renamed_assert(asserts, renames, param)) {
        struct problem *p;
        TSPoint start = ts_node_start_point(param_node);
        TSPoint end = ts_node_end_point(param_node);
        p = message_problem_new(3, start.row, start.column, end.row, end.column,
                                "Parameter %s should be asserted", param);
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
  g_assert(content);
  g_assert(level);
  g_assert(problems);

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
  }
  return res;
}
