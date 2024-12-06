#include <glib.h>

static void
missing_assert(gconstpointer name) {

  g_print("name: %p\n", name);
}
