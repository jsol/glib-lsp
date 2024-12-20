#include <glib.h>
#include <tree_sitter/api.h>

#include "parse_utils.h"

TSNode
parse_utils_get_first_node_id(TSNode check, guint id, gboolean *found)
{
  for (guint i = 0; i < ts_node_named_child_count(check); i++) {
    TSNode n = ts_node_named_child(check, i);
    if (ts_node_symbol(n) == id) {
      if (found) {
        *found = TRUE;
      }
      return n;
    }
  }
  for (guint i = 0; i < ts_node_named_child_count(check); i++) {
    TSNode n = ts_node_named_child(check, i);
    n = parse_utils_get_first_node_id(n, id, found);
    if (ts_node_symbol(n) == id) {
      /* Found already set */
      return n;
    }
  }
  if (found) {
    *found = FALSE;
  }
  return check;
}

gboolean
parse_utils_node_eq(const gchar *content, TSNode *node, const gchar *needle)
{
  const gchar *s;

  g_assert(content);
  g_assert(node);

  s = content + ts_node_start_byte(*node);

  return strncmp(s, needle, strlen(needle)) == 0;
}

gchar *
parse_utils_node_get_string(const gchar *content, TSNode *node)
{
  guint start = ts_node_start_byte(*node);
  guint end = ts_node_end_byte(*node);
  gsize len = end - start;
  return g_strndup(content + start, len);
}

gboolean
parse_utils_is_gobject_cast(const gchar *content,
                             TSNode decl,
                             gchar **to,
                             gchar **from)
{
  gchar *func_name;
  GString *func_name_upper;
  gchar *type_name;
  gchar *type_name_upper;

  gboolean found;
  TSNode type = parse_utils_get_first_node_id(decl, SYMBOL_TYPE, &found);
  if (!found) {
    return FALSE;
  }
  TSNode pdecl = parse_utils_get_first_node_id(decl, SYMBOL_POINTER_DECLARATION,
                                                &found);
  if (!found) {
    return FALSE;
  }

  TSNode call_node = parse_utils_get_first_node_id(decl, SYMBOL_CALL_EXPRESSION,
                                                    &found);

  if (!found) {
    return FALSE;
  }

  TSNode to_node = parse_utils_get_first_node_id(pdecl, SYMBOL_IDENTIFIER,
                                                  &found);
  if (!found) {
    return FALSE;
  }
  TSNode func_name_node = parse_utils_get_first_node_id(call_node,
                                                         SYMBOL_IDENTIFIER,
                                                         &found);
  if (!found) {
    return FALSE;
  }
  TSNode args_node = parse_utils_get_first_node_id(call_node, SYMBOL_ARG_LIST,
                                                    &found);
  if (!found) {
    return FALSE;
  }
  TSNode from_node = parse_utils_get_first_node_id(args_node,
                                                    SYMBOL_IDENTIFIER, &found);
  if (!found) {
    return FALSE;
  }

  func_name = parse_utils_node_get_string(content, &func_name_node);
  type_name = parse_utils_node_get_string(content, &type);

  type_name_upper = g_ascii_strup(type_name, -1);
  func_name_upper = g_string_new(func_name);
  g_string_ascii_up(func_name_upper);
  g_string_replace(func_name_upper, "_", "", -1);

  if (g_strcmp0(type_name_upper, func_name_upper->str) != 0) {
    g_free(func_name);
    g_free(type_name);
    g_free(type_name_upper);
    g_string_free(func_name_upper, TRUE);

    return FALSE;
  }

  *from = parse_utils_node_get_string(content, &from_node);
  *to = parse_utils_node_get_string(content, &to_node);

  g_free(func_name);
  g_free(type_name);
  g_free(type_name_upper);
  g_string_free(func_name_upper, TRUE);

  return TRUE;
}

gboolean
parse_utils_is_function_static(const gchar *content, TSNode p)
{
  g_assert(content);
  for (guint i = 0; i < ts_node_named_child_count(p); i++) {
    TSNode n = ts_node_named_child(p, i);
    if (ts_node_symbol(n) != SYMBOL_STORAGE_SPEC) {
      continue;
    }
    const gchar *s = content + ts_node_start_byte(n);

    if (g_ascii_strncasecmp(s, "static", strlen("static")) == 0) {
      /* compare  ignoring case as some code uses STATIC to make the internal
       * function unit testable
       */
      return TRUE;
    }
  }
  return FALSE;
}

gboolean
parse_utils_parameter_is_pointer(const gchar *content, TSNode param, gchar **name)
{
  g_assert(content);
  g_assert(name);

  if (ts_node_symbol(param) != SYMBOL_PARAM_DECLARATION) {
    return FALSE;
  }

  for (guint i = 0; i < ts_node_named_child_count(param); i++) {
    TSNode n = ts_node_named_child(param, i);
    TSNode c;

    if (ts_node_symbol(n) == SYMBOL_TYPE) {
      /* check if it is a secret pointer (gpointer) */
      if (parse_utils_node_eq(content, &n, "gpointer") ||
          parse_utils_node_eq(content, &n, "gconstpointer")) {
        c = ts_node_next_named_sibling(n);
        while (ts_node_symbol(c) != SYMBOL_IDENTIFIER) {
          if (ts_node_named_child_count(c) == 0) {
            return FALSE;
          }
          c = ts_node_named_child(c, 0);
        }
        *name = parse_utils_node_get_string(content, &c);

        return TRUE;
      }
    }

    if (ts_node_symbol(n) == SYMBOL_POINTER_DECLARATION) {
      c = ts_node_named_child(n, 0);
      while (ts_node_symbol(c) != SYMBOL_IDENTIFIER) {
        if (ts_node_named_child_count(c) == 0 || ts_node_is_null(c)) {
          return FALSE;
        }
        c = ts_node_named_child(c, 0);
      }
      *name = parse_utils_node_get_string(content, &c);

      return TRUE;
    }
  }

  return FALSE;
}
gboolean
parse_utils_parameter_is_unused(const gchar *content, TSNode param)
{
  TSNode type;
  g_assert(content);

  type = ts_node_child_by_field_name(param, "type", strlen("type"));
  if (ts_node_is_null(type)) {
    return false;
  }

  return parse_utils_node_eq(content, &type, "G_GNUC_UNUSED");
}
