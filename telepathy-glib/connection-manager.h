/*
 * connection-manager.h - proxy for a Telepathy connection manager
 *
 * Copyright (C) 2007-2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2009 Nokia Corporation
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
 */

#if !defined (_TP_GLIB_H_INSIDE) && !defined (_TP_COMPILATION)
#error "Only <telepathy-glib/telepathy-glib.h> can be included directly."
#endif

#ifndef __TP_CONNECTION_MANAGER_H__
#define __TP_CONNECTION_MANAGER_H__

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/protocol.h>
#include <telepathy-glib/proxy.h>

#include <telepathy-glib/_gen/genums.h>

G_BEGIN_DECLS

typedef struct _TpConnectionManager TpConnectionManager;
typedef struct _TpConnectionManagerClass TpConnectionManagerClass;
typedef struct _TpConnectionManagerPrivate TpConnectionManagerPrivate;

GType tp_connection_manager_get_type (void);
GType tp_connection_manager_param_get_type (void);

/* TYPE MACROS */
#define TP_TYPE_CONNECTION_MANAGER \
  (tp_connection_manager_get_type ())
#define TP_CONNECTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TYPE_CONNECTION_MANAGER, \
                              TpConnectionManager))
#define TP_CONNECTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TYPE_CONNECTION_MANAGER, \
                           TpConnectionManagerClass))
#define TP_IS_CONNECTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_CONNECTION_MANAGER))
#define TP_IS_CONNECTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_CONNECTION_MANAGER))
#define TP_CONNECTION_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_CONNECTION_MANAGER, \
                              TpConnectionManagerClass))

#define TP_TYPE_CONNECTION_MANAGER_PARAM \
  (tp_connection_manager_param_get_type ())

typedef enum
{
  TP_CM_INFO_SOURCE_NONE,
  TP_CM_INFO_SOURCE_FILE,
  TP_CM_INFO_SOURCE_LIVE
} TpCMInfoSource;

struct _TpConnectionManager {
    /*<private>*/
    TpProxy parent;

    TpConnectionManagerPrivate *priv;
};

struct _TpConnectionManagerClass {
    /*<private>*/
    TpProxyClass parent_class;
    gpointer *priv;
};

TpConnectionManager *tp_connection_manager_new (TpDBusDaemon *dbus,
    const gchar *name, const gchar *manager_filename, GError **error)
  G_GNUC_WARN_UNUSED_RESULT;

gboolean tp_connection_manager_activate (TpConnectionManager *self);

_TP_AVAILABLE_IN_0_18
void tp_list_connection_managers_async (TpDBusDaemon *dbus_daemon,
    GAsyncReadyCallback callback,
    gpointer user_data);
_TP_AVAILABLE_IN_0_18
GList *tp_list_connection_managers_finish (GAsyncResult *result,
    GError **error);

const gchar *tp_connection_manager_get_name (TpConnectionManager *self);
gboolean tp_connection_manager_is_running (TpConnectionManager *self);
TpCMInfoSource tp_connection_manager_get_info_source (
    TpConnectionManager *self);

gboolean tp_connection_manager_check_valid_name (const gchar *name,
    GError **error);

gboolean tp_connection_manager_check_valid_protocol_name (const gchar *name,
    GError **error);

gchar **tp_connection_manager_dup_protocol_names (TpConnectionManager *self)
  G_GNUC_WARN_UNUSED_RESULT;
gboolean tp_connection_manager_has_protocol (TpConnectionManager *self,
    const gchar *protocol);
TpProtocol *tp_connection_manager_get_protocol (
    TpConnectionManager *self, const gchar *protocol);
_TP_AVAILABLE_IN_0_18
GList *tp_connection_manager_dup_protocols (TpConnectionManager *self)
  G_GNUC_WARN_UNUSED_RESULT;

const gchar *tp_connection_manager_param_get_name (
    const TpConnectionManagerParam *param);
const gchar *tp_connection_manager_param_get_dbus_signature (
    const TpConnectionManagerParam *param);
gboolean tp_connection_manager_param_is_required (
    const TpConnectionManagerParam *param);
gboolean tp_connection_manager_param_is_required_for_registration (
    const TpConnectionManagerParam *param);
gboolean tp_connection_manager_param_is_secret (
    const TpConnectionManagerParam *param);
gboolean tp_connection_manager_param_is_dbus_property (
    const TpConnectionManagerParam *param);
gboolean tp_connection_manager_param_get_default (
    const TpConnectionManagerParam *param, GValue *value);
_TP_AVAILABLE_IN_0_20
GVariant *tp_connection_manager_param_dup_default_variant (
    const TpConnectionManagerParam *param);
_TP_AVAILABLE_IN_0_24
GVariantType *tp_connection_manager_param_dup_variant_type (
    const TpConnectionManagerParam *param);

void tp_connection_manager_init_known_interfaces (void);

#define TP_CONNECTION_MANAGER_FEATURE_CORE \
  (tp_connection_manager_get_feature_quark_core ())

GQuark tp_connection_manager_get_feature_quark_core (void) G_GNUC_CONST;

TpConnectionManagerParam *tp_connection_manager_param_copy (
    const TpConnectionManagerParam *in);
void tp_connection_manager_param_free (TpConnectionManagerParam *param);

G_END_DECLS

#endif
