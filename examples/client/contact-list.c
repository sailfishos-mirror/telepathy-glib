/*
 * contact-list
 *
 * Copyright © 2011 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/debug.h>

static void
account_manager_prepared_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  TpAccountManager *manager = (TpAccountManager *) object;
  GMainLoop *loop = user_data;  
  GList *accounts;
  GError *error = NULL;

  if (!tp_proxy_prepare_finish (object, res, &error))
    {
      g_print ("Error preparing AM: %s\n", error->message);
      goto OUT;
    }

  for (accounts = tp_account_manager_get_valid_accounts (manager);
       accounts != NULL; accounts = g_list_delete_link (accounts, accounts))
    {
      TpAccount *account = accounts->data;
      TpConnection *connection = tp_account_get_connection (account);
      GPtrArray *contacts;
      guint i;

      if (connection == NULL)
        continue;

      contacts = tp_connection_dup_contact_list (connection);
      for (i = 0; i < contacts->len; i++)
        {
          TpContact *contact = g_ptr_array_index (contacts, i);
          const gchar * const *groups;

          g_print ("contact %s (%s) in groups:\n",
              tp_contact_get_identifier (contact),
              tp_contact_get_alias (contact));

          for (groups = tp_contact_get_contact_groups (contact);
               *groups != NULL; groups++)
            g_print ("  %s\n", *groups);
        }
      g_ptr_array_unref (contacts);
    }

OUT:
  g_main_loop_quit (loop);
}

int
main (int argc,
      char **argv)
{
  TpAccountManager *manager;
  TpSimpleClientFactory *factory;
  GMainLoop *loop;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  loop = g_main_loop_new (NULL, FALSE);

  manager = tp_account_manager_dup ();
  factory = tp_proxy_get_factory (manager);
  tp_simple_client_factory_add_account_features_varargs (factory,
      TP_ACCOUNT_FEATURE_CONNECTION,
      0);
  tp_simple_client_factory_add_connection_features_varargs (factory,
      TP_CONNECTION_FEATURE_CONTACT_LIST,
      0);
  tp_simple_client_factory_add_contact_features_varargs (factory,
      TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_CONTACT_GROUPS,
      TP_CONTACT_FEATURE_INVALID);

  tp_proxy_prepare_async (manager, NULL, account_manager_prepared_cb, loop);

  g_main_loop_run (loop);

  g_object_unref (manager);
  g_main_loop_unref (loop);

  return 0;
}
