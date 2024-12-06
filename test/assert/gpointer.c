#include <glib.h>

static void
missing_assert(gpointer name) {

  g_print("name: %p\n", name);
}
