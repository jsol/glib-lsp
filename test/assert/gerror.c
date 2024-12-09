#include <glib.h>

static void
missing_assert(GError **err) {

  g_clear_error(err);
}
