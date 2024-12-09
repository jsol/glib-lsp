#include <glib.h>

static void
missing_assert(gpointer name) {
  /* Name can be NULL */

  g_print("name: %p\n", name);
}
