/*
 * simple-channel-dispatch-operation.c - a simple channel dispatch operation
 * service.
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include "simple-channel-dispatch-operation.h"
#include "util.h"

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/telepathy-glib-dbus.h>

static void channel_dispatch_operation_iface_init (gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE (TpTestsSimpleChannelDispatchOperation,
    tp_tests_simple_channel_dispatch_operation,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_DISPATCH_OPERATION,
        channel_dispatch_operation_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
        tp_dbus_properties_mixin_iface_init)
    )

/* TP_IFACE_CHANNEL_DISPATCH_OPERATION is implied */
static const char *CHANNEL_DISPATCH_OPERATION_INTERFACES[] = { NULL };

static const gchar *CHANNEL_DISPATCH_OPERATION_POSSIBLE_HANDLERS[] = {
    TP_CLIENT_BUS_NAME_BASE ".Badger", NULL, };

enum
{
  PROP_0,
  PROP_INTERFACES,
  PROP_CONNECTION,
  PROP_ACCOUNT,
  PROP_CHANNEL,
  PROP_CHANNEL_PROPERTIES,
  PROP_POSSIBLE_HANDLERS,
};

struct _SimpleChannelDispatchOperationPrivate
{
  gchar *conn_path;
  gchar *account_path;
  gchar *chan_path;
  GHashTable *chan_props;
};

static void
tp_tests_simple_channel_dispatch_operation_handle_with (
    TpSvcChannelDispatchOperation *iface,
    const gchar *handler,
    gint64 user_action_timestamp,
    DBusGMethodInvocation *context)
{
  if (!tp_strdiff (handler, "FAIL"))
    {
      GError error = { TP_ERROR, TP_ERROR_INVALID_ARGUMENT, "Nope" };

      dbus_g_method_return_error (context, &error);
      return;
    }

  dbus_g_method_return (context);
}

static void
tp_tests_simple_channel_dispatch_operation_claim (
    TpSvcChannelDispatchOperation *iface,
    DBusGMethodInvocation *context)
{
  tp_svc_channel_dispatch_operation_emit_finished (iface, "", "");

  dbus_g_method_return (context);
}

static void
channel_dispatch_operation_iface_init (gpointer klass,
    gpointer unused G_GNUC_UNUSED)
{
#define IMPLEMENT(x) tp_svc_channel_dispatch_operation_implement_##x (\
  klass, tp_tests_simple_channel_dispatch_operation_##x)
  IMPLEMENT(handle_with);
  IMPLEMENT(claim);
#undef IMPLEMENT
}


static void
tp_tests_simple_channel_dispatch_operation_init (TpTestsSimpleChannelDispatchOperation *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TESTS_TYPE_SIMPLE_CHANNEL_DISPATCH_OPERATION,
      TpTestsSimpleChannelDispatchOperationPrivate);

  /* we need something of the appropriate type so GetAll() won't crash */
  self->priv->chan_path = g_strdup ("/");
  self->priv->account_path = g_strdup ("/");
  self->priv->conn_path = g_strdup ("/");
  self->priv->chan_props = tp_asv_new (NULL, NULL);
}

static void
tp_tests_simple_channel_dispatch_operation_get_property (GObject *object,
              guint property_id,
              GValue *value,
              GParamSpec *spec)
{
  TpTestsSimpleChannelDispatchOperation *self =
    TP_TESTS_SIMPLE_CHANNEL_DISPATCH_OPERATION (object);

  switch (property_id) {
    case PROP_INTERFACES:
      g_value_set_boxed (value, CHANNEL_DISPATCH_OPERATION_INTERFACES);
      break;

    case PROP_ACCOUNT:
      g_value_set_boxed (value, self->priv->account_path);
      break;

    case PROP_CONNECTION:
      g_value_set_boxed (value, self->priv->conn_path);
      break;

    case PROP_CHANNEL:
      g_value_set_boxed (value, self->priv->chan_path);
      break;

    case PROP_CHANNEL_PROPERTIES:
      g_value_set_boxed (value, self->priv->chan_props);
      break;

    case PROP_POSSIBLE_HANDLERS:
      g_value_set_boxed (value, CHANNEL_DISPATCH_OPERATION_POSSIBLE_HANDLERS);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, spec);
      break;
  }
}

static void
tp_tests_simple_channel_dispatch_operation_finalize (GObject *object)
{
  TpTestsSimpleChannelDispatchOperation *self =
    TP_TESTS_SIMPLE_CHANNEL_DISPATCH_OPERATION (object);
  void (*finalize) (GObject *) =
    G_OBJECT_CLASS (tp_tests_simple_channel_dispatch_operation_parent_class)->finalize;

  g_free (self->priv->conn_path);
  g_free (self->priv->account_path);
  g_free (self->priv->chan_path);
  tp_clear_pointer (&self->priv->chan_props, g_hash_table_unref);

  if (finalize != NULL)
    finalize (object);
}

/**
  * This class currently only provides the minimum for
  * tp_channel_dispatch_operation_prepare to succeed. This turns out to be only a working
  * Properties.GetAll().
  */
static void
tp_tests_simple_channel_dispatch_operation_class_init (TpTestsSimpleChannelDispatchOperationClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;

  static TpDBusPropertiesMixinPropImpl a_props[] = {
        { "Interfaces", "interfaces", NULL },
        { "Connection", "connection", NULL },
        { "Account", "account", NULL },
        { "Channel", "channel", NULL },
        { "ChannelProperties", "channel-properties", NULL },
        { "PossibleHandlers", "possible-handlers", NULL },
        { NULL }
  };

  static TpDBusPropertiesMixinIfaceImpl prop_interfaces[] = {
        { TP_IFACE_CHANNEL_DISPATCH_OPERATION,
          tp_dbus_properties_mixin_getter_gobject_properties,
          NULL,
          a_props
        },
        { NULL },
  };

  g_type_class_add_private (klass, sizeof (TpTestsSimpleChannelDispatchOperationPrivate));
  object_class->get_property = tp_tests_simple_channel_dispatch_operation_get_property;
  object_class->finalize = tp_tests_simple_channel_dispatch_operation_finalize;

  param_spec = g_param_spec_boxed ("interfaces", "Extra D-Bus interfaces",
      "In this case we only implement ChannelDispatchOperation, so none.",
      G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INTERFACES, param_spec);

  param_spec = g_param_spec_boxed ("connection", "connection path",
      "Connection path",
      DBUS_TYPE_G_OBJECT_PATH,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION, param_spec);

  param_spec = g_param_spec_boxed ("account", "account path",
      "Account path",
      DBUS_TYPE_G_OBJECT_PATH,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACCOUNT, param_spec);

  param_spec = g_param_spec_boxed ("channel", "channel path",
      "Channel path",
      DBUS_TYPE_G_OBJECT_PATH,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL, param_spec);

  param_spec = g_param_spec_boxed ("channel-properties", "channel properties",
      "Channel properties",
      TP_HASH_TYPE_STRING_VARIANT_MAP,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL_PROPERTIES,
      param_spec);

  param_spec = g_param_spec_boxed ("possible-handlers", "possible handlers",
      "possible handles",
      G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_POSSIBLE_HANDLERS,
      param_spec);

  klass->dbus_props_class.interfaces = prop_interfaces;
  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (TpTestsSimpleChannelDispatchOperationClass, dbus_props_class));
}

void
tp_tests_simple_channel_dispatch_operation_set_conn_path (
    TpTestsSimpleChannelDispatchOperation *self,
    const gchar *conn_path)
{
  g_free (self->priv->conn_path);
  self->priv->conn_path = g_strdup (conn_path);
}

void
tp_tests_simple_channel_dispatch_operation_set_channel (
    TpTestsSimpleChannelDispatchOperation *self,
    TpChannel *chan)
{
  g_hash_table_unref (self->priv->chan_props);
  g_free (self->priv->chan_path);
  self->priv->chan_path = g_strdup (tp_proxy_get_object_path (chan));
  self->priv->chan_props = tp_tests_dup_channel_props_asv (chan);
}

void
tp_tests_simple_channel_dispatch_operation_set_account_path (
    TpTestsSimpleChannelDispatchOperation *self,
    const gchar *account_path)
{
  g_free (self->priv->account_path);
  self->priv->account_path = g_strdup (account_path);
}
