/*
 * Copyright (C) 2012 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Xavier Claessens <xavier.claessens@collabora.co.uk>
 */

#include "config.h"
#include "client-factory-internal.h"

#include <telepathy-glib/telepathy-glib.h>

#include <telepathy-logger/text-channel-internal.h>
#include <telepathy-logger/call-channel-internal.h>

G_DEFINE_TYPE (TplClientFactory, _tpl_client_factory,
    TP_TYPE_AUTOMATIC_CLIENT_FACTORY)

#define chainup ((TpClientFactoryClass *) \
    _tpl_client_factory_parent_class)

static TpChannel *
create_channel_impl (TpClientFactory *self,
    TpConnection *conn,
    const gchar *object_path,
    GVariant *properties,
    GError **error)
{
  const gchar *chan_type;
  GHashTable *asv;
  TpChannel *channel;

  asv = tp_asv_from_vardict (properties);
  chan_type = tp_asv_get_string (asv, TP_PROP_CHANNEL_CHANNEL_TYPE);

  if (!tp_strdiff (chan_type, TP_IFACE_CHANNEL_TYPE_TEXT))
    {
      channel = (TpChannel *) _tpl_text_channel_new_with_factory (self, conn,
          object_path, asv, error);
    }
  else if (!tp_strdiff (chan_type, TP_IFACE_CHANNEL_TYPE_CALL1))
    {
      channel = (TpChannel *) _tpl_call_channel_new_with_factory (self, conn,
          object_path, asv, error);
    }
  else
    {
      channel = chainup->create_channel (self, conn, object_path, properties,
          error);
    }

  g_hash_table_unref (asv);
  return channel;
}

static GArray *
dup_channel_features_impl (TpClientFactory *self,
    TpChannel *channel)
{
  GArray *features;
  GQuark f;

  features = chainup->dup_channel_features (self, channel);

  if (TPL_IS_CALL_CHANNEL (channel))
    {
      f = TPL_CALL_CHANNEL_FEATURE_CORE;
      g_array_append_val (features, f);
    }
  else if (TPL_IS_TEXT_CHANNEL (channel))
    {
      f = TPL_TEXT_CHANNEL_FEATURE_CORE;
      g_array_append_val (features, f);
    }

  return features;
}

static void
_tpl_client_factory_init (TplClientFactory *self)
{
}

static void
_tpl_client_factory_class_init (TplClientFactoryClass *cls)
{
  TpClientFactoryClass *simple_class = (TpClientFactoryClass *) cls;

  simple_class->create_channel = create_channel_impl;
  simple_class->dup_channel_features = dup_channel_features_impl;
}


static TpClientFactory *
_tpl_client_factory_new (TpDBusDaemon *dbus)
{
  return g_object_new (TPL_TYPE_CLIENT_FACTORY,
      "dbus-daemon", dbus,
      NULL);
}

TpClientFactory *
_tpl_client_factory_dup (TpDBusDaemon *dbus)
{
  static TpClientFactory *singleton = NULL;

  if (singleton != NULL)
    return g_object_ref (singleton);

  singleton = _tpl_client_factory_new (dbus);

  g_object_add_weak_pointer (G_OBJECT (singleton), (gpointer) &singleton);

  return singleton;
}
