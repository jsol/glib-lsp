#include <glib.h>
#include <tree_sitter/api.h>

#include "parse_utils.h"

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
