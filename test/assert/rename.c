#include <glib.h>

static void
missing_assert(gpointer name) {
  struct test *info = (struct test *) name;

  g_assert(info);

  g_print("name: %p\n", name);
}
