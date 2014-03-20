#include "config.h"

#include <telepathy-glib/intset.h>
#include <telepathy-glib/heap.h>

#include <stdlib.h>
#include <time.h>
#include <stdio.h>

static gint comparator_fn (gconstpointer a, gconstpointer b)
{
    return (a < b) ? -1 : (a == b) ? 0 : 1;
}

typedef struct {
    int dummy;
} Fixture;

static void
setup (Fixture *f,
    gconstpointer data)
{
}

static void
test (Fixture *f,
    gconstpointer data)
{
  TpHeap *heap = tp_heap_new (comparator_fn, NULL);
  guint prev = 0;
  guint i;

  srand (time (NULL));

  for (i=0; i<10000; i++)
    {
      tp_heap_add (heap, GUINT_TO_POINTER (rand ()));
    }

  while (tp_heap_size (heap))
    {
      guint elem = GPOINTER_TO_INT (tp_heap_peek_first (heap));
      g_assert (elem == GPOINTER_TO_UINT (tp_heap_extract_first (heap)));
      g_assert (prev <= elem);
      prev = elem;
    }

  tp_heap_destroy (heap);
}

static void
teardown (Fixture *f,
    gconstpointer data)
{
}

int
main (int argc,
    char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/heap", Fixture, NULL, setup, test, teardown);

  return g_test_run ();
}
