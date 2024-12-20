#include <glib.h>
#include <tree_sitter/api.h>

#include "message.h"
#include "parse_utils.h"
#include "parser.h"
#include "process_comments.h"
#include "processor.h"

static void
validate_return(const gchar *content,
                TSNode n,
                const gchar *comment,
                GList **problems)
{
  TSNode ret_type;
  struct problem *p = NULL;
  const gchar *return_doc;

  g_assert(content);
  g_assert(comment);
  g_assert(problems);

  ret_type = ts_node_child_by_field_name(n, "type", strlen("type"));

  if (ts_node_is_null(ret_type)) {
    p = message_problem_new(3, &n, &n, "Function should have a return value");
    *problems = g_list_prepend(*problems, p);
    return;
  }

  return_doc = g_strstr_len(comment, -1, "@return");

  if (parse_utils_node_eq(content, &ret_type, "void")) {
    if (return_doc != NULL) {
      p = message_problem_new(3, &n, &n,
                              "Void functions should not document @return");
      *problems = g_list_prepend(*problems, p);
    }
  } else {
    if (return_doc == NULL) {
      p = message_problem_new(3, &n, &n,
                              "Functions return value should be documented "
                              "with @return");
      *problems = g_list_prepend(*problems, p);
    }
  }
}

static GHashTable *
parse_param_docs(const gchar *content, const gchar *comment)
{
  gchar **lines = NULL;
  GHashTable *res;

  g_assert(content);
  g_assert(comment);

  res = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  lines = g_strsplit(comment, "\n", -1);

  for (guint i = 0; lines[i] != NULL; i++) {
    gchar **words = NULL;
    gboolean next = FALSE;
    if (g_strstr_len(lines[i], -1, "@param") == NULL) {
      continue;
    }

    words = g_strsplit(lines[i], " ", -1);
    for (guint j = 0; words[j] != NULL; j++) {
      if (g_strcmp0(words[j], "@param") == 0) {
        next = TRUE;
        continue;
      }

      if (next && strlen(words[j]) > 0) {
        g_hash_table_add(res, g_strdup(words[j]));
        break;
      }
    }
    g_strfreev(words);
  }
  g_strfreev(lines);

  return res;
}

static void
validate_arg(const gchar *content,
             TSNode arg,
             GHashTable *params,
             GList **problems)
{
  TSNode id;
  gboolean found;
  gchar *name;

  g_assert(content);
  g_assert(params);
  g_assert(problems);

  id = parse_utils_get_first_node_id(arg, SYMBOL_IDENTIFIER, &found);

  if (!found) {
    return;
  }

  name = parse_utils_node_get_string(content, &id);

  if (!g_hash_table_remove(params, name)) {
    struct problem *p;
    p = message_problem_new(3, &id, &id, "Parameter %s is not documented", name);
    *problems = g_list_prepend(*problems, p);
  }

  g_free(name);
}

static void
validate_arg_list(const gchar *content,
                  TSNode n,
                  const gchar *comment,
                  GList **problems)
{
  GHashTable *params;
  TSNode list;
  gboolean found = FALSE;

  g_assert(content);
  g_assert(comment);
  g_assert(problems);

  list = parse_utils_get_first_node_id(n, 258, &found);
  if (!found) {
    return;
  }
  params = parse_param_docs(content, comment);

  for (guint i = 0; i < ts_node_named_child_count(list); i++) {
    TSNode n = ts_node_named_child(list, i);
    validate_arg(content, n, params, problems);
  }

  if (g_hash_table_size(params) > 0) {
    struct problem *p;
    p = message_problem_new(3, &list, &list, "Extra params are documented");
    *problems = g_list_prepend(*problems, p);
  }

  g_clear_pointer(&params, g_hash_table_unref);
}

static void
check_function_comments(const gchar *content, TSNode current, GList **problems)
{
  g_assert(content);
  g_assert(problems);

  for (guint i = 0; i < ts_node_named_child_count(current); i++) {
    TSNode n = ts_node_named_child(current, i);
    TSNode stat;
    TSNode comment;
    struct problem *p = NULL;
    gchar *comment_str;

    gboolean found = FALSE;

    if (ts_node_symbol(n) != SYMBOL_DECLARATION) {
      continue;
    }

    stat = parse_utils_get_first_node_id(n, SYMBOL_STORAGE_SPEC, &found);

    if (found && (parse_utils_node_eq(content, &stat, "static") ||
                  parse_utils_node_eq(content, &stat, "STATIC"))) {
      /* Don't check comments for "internal" functions */
      continue;
    }

    parse_utils_get_first_node_id(n, SYMBOL_FUNC_DECLARATION, &found);

    if (!found) {
      continue;
    }

    comment = ts_node_prev_named_sibling(n);
    if (ts_node_symbol(comment) != SYMBOL_COMMENT) {
      p = message_problem_new(3, &n, &n, "Function should be documented");
      *problems = g_list_prepend(*problems, p);
      continue;
    }

    comment_str = parse_utils_node_get_string(content, &comment);

    if (g_strstr_len(comment_str, -1, "@brief") == NULL) {
      p = message_problem_new(3, &comment, &comment,
                              "Comment should contain a @brief");
      *problems = g_list_prepend(*problems, p);
    }

    validate_return(content, n, comment_str, problems);
    validate_arg_list(content, n, comment_str, problems);

    g_free(comment_str);
  }
}

static gboolean
is_post_comment(const gchar *content, TSNode c)
{
  gchar *str;
  gboolean ret = FALSE;

  g_assert(content);

  if (ts_node_is_null(c)) {
    return ret;
  }

  if (ts_node_symbol(c) != SYMBOL_COMMENT) {
    return ret;
  }

  str = parse_utils_node_get_string(content, &c);

  if (g_str_has_prefix(str, "/**< ")) {
    ret = TRUE;
  }

  g_free(str);

  return ret;
}

static gboolean
is_pre_comment(const gchar *content, TSNode c)
{
  gchar *str;
  gboolean ret = FALSE;

  g_assert(content);
  if (ts_node_is_null(c)) {
    return ret;
  }

  if (ts_node_symbol(c) != SYMBOL_COMMENT) {
    return ret;
  }

  str = parse_utils_node_get_string(content, &c);

  if (g_str_has_prefix(str, "/** ")) {
    ret = TRUE;
  }

  g_free(str);

  return ret;
}

static void
check_struct_comments(const gchar *content, TSNode current, GList **problems)
{
  TSNode fields;
  gboolean found = FALSE;

  g_assert(content);
  g_assert(problems);

  if (ts_node_symbol(current) != SYMBOL_STRUCT_SPEC) {
    return;
  }

  fields = parse_utils_get_first_node_id(current, SYMBOL_FIELD_DECL_LIST,
                                          &found);

  if (!found) {
    return;
  }

  for (guint i = 0; i < ts_node_named_child_count(fields); i++) {
    TSNode n = ts_node_named_child(fields, i);
    TSNode ident;
    struct problem *p = NULL;

    if (ts_node_symbol(n) != SYMBOL_FIELD_DECL) {
      continue;
    }
    if (is_post_comment(content, ts_node_next_named_sibling(n))) {
      continue;
    }
    if (is_pre_comment(content, ts_node_prev_named_sibling(n))) {
      continue;
    }

    ident = parse_utils_get_first_node_id(n, SYMBOL_FIELD_IDENT, &found);
    if (found) {
      p = message_problem_new(3, &ident, &ident,
                              "Struct field should be commented");
      *problems = g_list_prepend(*problems, p);
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

    check_struct_comments(content, n, problems);
    recurse_check(content, n, problems);
  }
}

GList *
process_comments(parser_t *parser, G_GNUC_UNUSED struct process_ctx *ctx)
{
  GList *res = NULL;
  if (parser->message->type == MESSAGE_TYPE_DIAGNOSTIC ||
      parser->message->type == MESSAGE_TYPE_OPEN ||
      parser->message->type == MESSAGE_TYPE_CHANGE) {
    recurse_check(parser->content, parser->root_node, &res);
    check_function_comments(parser->content, parser->root_node, &res);
  }
  return res;
}
