#include <glib.h>

static void midscope(void) {
  gchar *hello;

  hello = g_strdup("Hellooo");
  gchar *upper = g_ascii_strup(hello, -1);

  g_free(hello);
  g_free(upper);
}
