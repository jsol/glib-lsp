#include <glib.h>

static void
missing_assert(gpointer name) {
  GObject *info = G_OBJECT(name);

  g_assert(info);

  g_print("name: %p\n", name);
}
