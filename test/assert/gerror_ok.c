#include <glib.h>

static void
missing_assert(GError **err) {

  g_assert(err == NULL || *err == NULL);
  g_clear_error(err);
}
