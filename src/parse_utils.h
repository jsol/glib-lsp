#pragma once
#include <glib.h>
#include <tree_sitter/api.h>

#define SYMBOL_FUNCTION            196
#define SYMBOL_STORAGE_SPEC        242
#define SYMBOL_CALL_EXPRESSION     299
#define SYMBOL_PARAM_DECLARATION   260
#define SYMBOL_POINTER_DECLARATION 226
#define SYMBOL_IDENTIFIER          1
#define SYMBOL_TYPE                362

gboolean parse_utils_node_eq(const gchar *content,
                             TSNode *node,
                             const gchar *needle);

gchar *parse_utils_node_get_string(const gchar *content, TSNode *node);

gboolean parse_utils_is_function_static(const gchar *content, TSNode p);

gboolean parse_utils_parameter_is_pointer(const gchar *content,
                                          TSNode param,
                                          gchar **name);
