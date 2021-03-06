/* Feature test for the user's self-handle/self-contact changing.
 *
 * Copyright (C) 2009-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/cli-connection.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>

#include "tests/lib/contacts-conn.h"
#include "tests/lib/debug.h"
#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

typedef struct {
  TpDBusDaemon *dbus;
  TpTestsSimpleConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  gchar *name;
  gchar *conn_path;
  GError *error /* zero-initialized */;
  TpConnection *client_conn;
  TpHandleRepoIface *contact_repo;
  GAsyncResult *result;
} Fixture;

static void
setup (Fixture *f,
    gconstpointer arg)
{
  gboolean ok;

  f->dbus = tp_tests_dbus_daemon_dup_or_die ();

  f->service_conn = TP_TESTS_SIMPLE_CONNECTION (
      tp_tests_object_new_static_class (TP_TESTS_TYPE_CONTACTS_CONNECTION,
        "account", "me@example.com",
        "protocol", "simple",
        NULL));
  f->service_conn_as_base = TP_BASE_CONNECTION (f->service_conn);
  g_object_ref (f->service_conn_as_base);
  g_assert (f->service_conn != NULL);
  g_assert (f->service_conn_as_base != NULL);

  f->contact_repo = tp_base_connection_get_handles (f->service_conn_as_base,
      TP_ENTITY_TYPE_CONTACT);

  ok = tp_base_connection_register (f->service_conn_as_base, "simple",
        &f->name, &f->conn_path, &f->error);
  g_assert_no_error (f->error);
  g_assert (ok);

  f->client_conn = tp_tests_connection_new (f->dbus, f->name, f->conn_path,
      &f->error);
  g_assert_no_error (f->error);
  g_assert (f->client_conn != NULL);

  if (!tp_strdiff (arg, "round-trip"))
    {
      /* Make sure preparing the self-contact requires a round-trip */
      TpClientFactory *factory = tp_proxy_get_factory (f->client_conn);

      tp_client_factory_add_contact_features_varargs (factory,
          TP_CONTACT_FEATURE_CAPABILITIES,
          0);
    }
}

static void
setup_and_connect (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  GQuark connected_feature[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };

  setup (f, unused);

  tp_cli_connection_call_connect (f->client_conn, -1, NULL, NULL, NULL, NULL);
  tp_tests_proxy_run_until_prepared (f->client_conn, connected_feature);
}

/* we'll get more arguments, but just ignore them */
static void
swapped_counter_cb (gpointer user_data)
{
  guint *times = user_data;

  ++*times;
}

static void
test_self_handle (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpContact *before, *after;
  guint contact_times = 0;

  g_signal_connect_swapped (f->client_conn, "notify::self-contact",
      G_CALLBACK (swapped_counter_cb), &contact_times);

  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "me@example.com");

  g_object_get (f->client_conn,
      "self-contact", &before,
      NULL);
  g_assert_cmpuint (tp_contact_get_handle (before), ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));
  g_assert_cmpstr (tp_contact_get_identifier (before), ==, "me@example.com");

  g_assert_cmpuint (contact_times, ==, 0);

  /* similar to /nick in IRC */
  tp_tests_simple_connection_set_identifier (f->service_conn,
      "myself@example.org");
  tp_tests_proxy_run_until_dbus_queue_processed (f->client_conn);

  while (contact_times < 1)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpuint (contact_times, ==, 1);

  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "myself@example.org");

  g_object_get (f->client_conn,
      "self-contact", &after,
      NULL);
  g_assert (before != after);
  g_assert_cmpuint (tp_contact_get_handle (after), ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));
  g_assert_cmpstr (tp_contact_get_identifier (after), ==,
      "myself@example.org");

  g_object_unref (before);
  g_object_unref (after);
}

static void
test_change_early (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpContact *after;
  guint contact_times = 0;
  gboolean ok;
  GQuark features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };

  g_signal_connect_swapped (f->client_conn, "notify::self-contact",
      G_CALLBACK (swapped_counter_cb), &contact_times);

  tp_proxy_prepare_async (f->client_conn, features, tp_tests_result_ready_cb,
      &f->result);
  g_assert (f->result == NULL);

  /* act as though someone else called Connect; emit signals in quick
   * succession, so that by the time the TpConnection tries to investigate
   * the self-handle, it has already changed */
  tp_base_connection_change_status (f->service_conn_as_base,
      TP_CONNECTION_STATUS_CONNECTING,
      TP_CONNECTION_STATUS_REASON_REQUESTED);
  tp_tests_simple_connection_set_identifier (f->service_conn,
      "me@example.com");
  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "me@example.com");
  tp_base_connection_change_status (f->service_conn_as_base,
      TP_CONNECTION_STATUS_CONNECTED,
      TP_CONNECTION_STATUS_REASON_REQUESTED);
  tp_tests_simple_connection_set_identifier (f->service_conn,
      "myself@example.org");
  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "myself@example.org");

  /* now run the main loop and let the client catch up */
  tp_tests_run_until_result (&f->result);
  ok = tp_proxy_prepare_finish (f->client_conn, f->result, &f->error);
  g_assert_no_error (f->error);
  g_assert (ok);

  /* the self-handle and self-contact change once during connection */
  g_assert_cmpuint (contact_times, ==, 1);

  g_object_get (f->client_conn,
      "self-contact", &after,
      NULL);
  g_assert_cmpuint (tp_contact_get_handle (after), ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));
  g_assert_cmpstr (tp_contact_get_identifier (after), ==,
      "myself@example.org");

  g_object_unref (after);
}

static void
test_change_inconveniently (Fixture *f,
    gconstpointer arg)
{
  TpContact *after;
  guint contact_times = 0, got_all_times = 0;
  gboolean ok;
  GQuark features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };

  /* This test exercises what happens if the self-contact changes
   * between obtaining its handle for the first time and having the
   * TpContact fully prepared. In Telepathy 1.0, that can only happen
   * if you are preparing non-core features, because we get the self-handle
   * and the self-ID at the same time. */
  g_assert_cmpstr (arg, ==, "round-trip");

  g_signal_connect_swapped (f->client_conn, "notify::self-contact",
      G_CALLBACK (swapped_counter_cb), &contact_times);
  g_signal_connect_swapped (f->service_conn,
      "got-all::" TP_IFACE_CONNECTION,
      G_CALLBACK (swapped_counter_cb), &got_all_times);

  tp_proxy_prepare_async (f->client_conn, features, tp_tests_result_ready_cb,
      &f->result);
  g_assert (f->result == NULL);

  /* act as though someone else called Connect */
  tp_base_connection_change_status (f->service_conn_as_base,
      TP_CONNECTION_STATUS_CONNECTING,
      TP_CONNECTION_STATUS_REASON_REQUESTED);
  tp_tests_simple_connection_set_identifier (f->service_conn,
      "me@example.com");
  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "me@example.com");
  tp_base_connection_change_status (f->service_conn_as_base,
      TP_CONNECTION_STATUS_CONNECTED,
      TP_CONNECTION_STATUS_REASON_REQUESTED);

  /* run the main loop until just after GetAll(Connection)
   * is processed, to make sure the client first saw the old self handle */
  while (got_all_times == 0)
    g_main_context_iteration (NULL, TRUE);

  DEBUG ("changing my own identifier to something else");
  tp_tests_simple_connection_set_identifier (f->service_conn,
      "myself@example.org");
  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "myself@example.org");

  /* now run the main loop and let the client catch up */
  tp_tests_run_until_result (&f->result);
  ok = tp_proxy_prepare_finish (f->client_conn, f->result, &f->error);
  g_assert_no_error (f->error);
  g_assert (ok);

  /* the self-contact changes once during connection */
  g_assert_cmpuint (contact_times, ==, 1);

  g_assert_cmpuint (
      tp_contact_get_handle (tp_connection_get_self_contact (f->client_conn)),
      ==, tp_base_connection_get_self_handle (f->service_conn_as_base));

  g_object_get (f->client_conn,
      "self-contact", &after,
      NULL);
  g_assert_cmpuint (tp_contact_get_handle (after), ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));
  g_assert_cmpstr (tp_contact_get_identifier (after), ==,
      "myself@example.org");

  g_object_unref (after);
}

static void
teardown (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  g_clear_error (&f->error);

  if (f->client_conn != NULL)
    tp_tests_connection_assert_disconnect_succeeds (f->client_conn);

  tp_clear_object (&f->result);
  tp_clear_object (&f->client_conn);
  tp_clear_object (&f->service_conn_as_base);
  tp_clear_object (&f->service_conn);
  tp_clear_pointer (&f->name, g_free);
  tp_clear_pointer (&f->conn_path, g_free);
  tp_clear_object (&f->dbus);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_set_prgname ("self-handle");
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/self-handle", Fixture, NULL, setup_and_connect,
      test_self_handle, teardown);
  g_test_add ("/self-handle/round-trip", Fixture, "round-trip",
      setup_and_connect, test_self_handle, teardown);
  g_test_add ("/self-handle/change-early", Fixture, NULL, setup,
      test_change_early, teardown);
  g_test_add ("/self-handle/change-early/round-trip", Fixture, "round-trip",
      setup, test_change_early, teardown);
  g_test_add ("/self-handle/change-inconveniently", Fixture,
      "round-trip", setup, test_change_inconveniently, teardown);

  return tp_tests_run_with_bus ();
}
