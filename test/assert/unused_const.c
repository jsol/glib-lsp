#include <glib.h>

static void
missing_assert(G_GNUC_UNUSED const gchar * name) {

  g_print("name: %p\n", name);
}
