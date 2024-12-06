#include <glib.h>

static void
missing_assert(G_GNUC_UNUSED gpointer name) {

  g_print("name: %p\n", name);
}
