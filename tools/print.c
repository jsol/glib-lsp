#include <glib.h>
#include <tree_sitter/api.h>

const TSLanguage *tree_sitter_c(void);
static void
recurse_check(const gchar *content, TSNode p, const gchar *level)
{
  g_assert(content);
  g_assert(level);

  gchar *next_level = g_strdup_printf("  %s", level);
  for (guint i = 0; i < ts_node_named_child_count(p); i++) {
    TSNode n = ts_node_named_child(p, i);

    guint s = ts_node_start_byte(n);
    guint e = ts_node_end_byte(n);

    gchar *c = g_strndup(content + s, e - s);
    TSSymbol sy = ts_node_symbol(n);
    const gchar *fn = ts_node_field_name_for_child(p, i);
    g_print("Node: %s %s (%s) %s, %u\n", level, ts_node_type(n), fn, c, sy);
    g_free(c);

    recurse_check(content, n, next_level);
  }
  g_free(next_level);
}

int
main(int argc, char *argv[])
{
  GError *lerr = NULL;
  gchar *content = NULL;
  gsize len = 0;
  TSParser *parser;
  TSTree *tree;
  TSNode root;

  if (argc != 2) {
    g_print("Add the file to parse as the first argument\n");
    return 1;
  }

  if (!g_file_get_contents(argv[1], &content, &len, &lerr)) {
    g_print("Could not open file %s: %s\n", argv[1],
            lerr ? lerr->message : "no error message");
    g_clear_error(&lerr);
    return 2;
  }

  parser = ts_parser_new();

  // Set the parser's language (JSON in this case).
  ts_parser_set_language(parser, tree_sitter_c());

  // Build a syntax tree based on source code stored in a string.
  tree = ts_parser_parse_string(parser, NULL, content, len);

  // Get the root node of the syntax tree.
  root = ts_tree_root_node(tree);

  recurse_check(content, root, "");
  g_free(content);
}
