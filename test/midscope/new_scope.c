#include <glib.h>

static void
midscope(void)
{
  gchar *hello;

  hello = g_strdup("Hellooo");
  if (hello != NULL) {
    gchar *upper = g_ascii_strup(hello, -1);
    g_free(upper);
  }

  g_free(hello);
}
