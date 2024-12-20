#include <glib.h>
#include <tree_sitter/api.h>

#include "message.h"
#include "parse_utils.h"
#include "parser.h"
#include "process_asserts.h"
#include "processor.h"

static void
collect_asserts(GHashTable *res, const gchar *content, TSNode check)
{
  g_assert(res);
  g_assert(content);

  for (guint i = 0; i < ts_node_named_child_count(check); i++) {
    TSNode args;
    TSNode arg;
    TSNode function;
    gboolean found = FALSE;
    TSNode n = ts_node_named_child(check, i);

    if (ts_node_symbol(n) != SYMBOL_CALL_EXPRESSION) {
      collect_asserts(res, content, n);
      continue;
    }

    function = ts_node_child_by_field_name(n, "function", strlen("function"));
    args = ts_node_child_by_field_name(n, "arguments", strlen("arguments"));

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
    arg = parse_utils_get_first_node_id(args, SYMBOL_IDENTIFIER, &found);

    if (found) {
      g_hash_table_insert(res, parse_utils_node_get_string(content, &arg), &arg);
    }
  }
}

static void
collect_renames(GHashTable *res, const gchar *content, TSNode check)
{
  gchar *to_str;
  gchar *from_str;

  g_assert(res);
  g_assert(content);

  for (guint i = 0; i < ts_node_named_child_count(check); i++) {
    TSNode n = ts_node_named_child(check, i);
    TSNode decl;
    TSNode to;
    TSNode from;

    if (ts_node_symbol(n) != SYMBOL_DECLARATION) {
      collect_renames(res, content, n);
      continue;
    }

    decl = ts_node_child_by_field_name(n, "declarator", strlen("declarator"));
    to = parse_utils_get_first_node_id(decl, SYMBOL_IDENTIFIER, NULL);

    from = parse_utils_get_first_node_id(decl, SYMBOL_CAST_EXPRESSION, NULL);

    if (parse_utils_is_gobject_cast(content, n, &to_str, &from_str)) {
      g_hash_table_insert(res, from_str, to_str);
      return;
    }

    if (ts_node_symbol(from) != SYMBOL_CAST_EXPRESSION) {
      continue;
    }

    from = parse_utils_get_first_node_id(from, SYMBOL_IDENTIFIER, NULL);

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

static gboolean
contains_gerror_check(const gchar *content, TSNode function)
{
  g_assert(content);

  for (guint i = 0; i < ts_node_named_child_count(function); i++) {
    TSNode n = ts_node_named_child(function, i);
    if (parse_utils_node_eq(content, &n, "err == NULL || *err == NULL") ||
        contains_gerror_check(content, n)) {
      return TRUE;
    }
  }

  return FALSE;
}

static gboolean
invalid_gerror_assert(const gchar *content,
                      TSNode param,
                      TSNode function,
                      struct problem **p)
{
  g_assert(content);
  g_assert(p);

  if (!parse_utils_node_eq(content, &param, "GError **err")) {
    return FALSE;
  }

  if (!contains_gerror_check(content, function)) {
    *p = message_problem_new(3, &param, &param,
                             "GErrors should be asserted (err == NULL || *err "
                             "== NULL)");

    return TRUE;
  }

  return TRUE;
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
      struct problem *p = NULL;

      if (invalid_gerror_assert(content, param_node, function, &p)) {
        if (p != NULL) {
          *problems = g_list_prepend(*problems, p);
        }
      } else if (!g_hash_table_contains(asserts, param) &&
                 !mentioned_with_null(content, param, function) &&
                 !renamed_assert(asserts, renames, param)) {
        p = message_problem_new(3, &param_node, &param_node,
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
recurse_check(const gchar *content, TSNode p, GList **problems)
{
  g_assert(content);
  g_assert(problems);

  for (guint i = 0; i < ts_node_named_child_count(p); i++) {
    TSNode n = ts_node_named_child(p, i);

    check_asserts(content, n, problems);
    recurse_check(content, n, problems);
  }
}

GList *
process_asserts(parser_t *parser, G_GNUC_UNUSED struct process_ctx *ctx)
{
  GList *res = NULL;
  if (parser->message->type == MESSAGE_TYPE_DIAGNOSTIC ||
      parser->message->type == MESSAGE_TYPE_OPEN ||
      parser->message->type == MESSAGE_TYPE_CHANGE) {
    recurse_check(parser->content, parser->root_node, &res);
  }
  return res;
}
