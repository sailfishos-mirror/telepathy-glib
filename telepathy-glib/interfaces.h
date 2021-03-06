#if !defined (_TP_GLIB_DBUS_H_INSIDE) && !defined (_TP_COMPILATION)
#error "Only <telepathy-glib/telepathy-glib-dbus.h> can be included directly."
#endif

#ifndef __TP_INTERFACES_H__
#define __TP_INTERFACES_H__

#include <glib.h>

G_BEGIN_DECLS

#define TP_CM_BUS_NAME_BASE    "im.telepathy.v1.ConnectionManager."
#define TP_CM_OBJECT_PATH_BASE "/im/telepathy/v1/ConnectionManager/"
#define TP_CONN_BUS_NAME_BASE "im.telepathy.v1.Connection."
#define TP_CONN_OBJECT_PATH_BASE "/im/telepathy/v1/Connection/"
#define TP_ACCOUNT_MANAGER_BUS_NAME "im.telepathy.v1.AccountManager"
#define TP_ACCOUNT_MANAGER_OBJECT_PATH "/im/telepathy/v1/AccountManager"
#define TP_ACCOUNT_OBJECT_PATH_BASE "/im/telepathy/v1/Account/"
#define TP_CHANNEL_DISPATCHER_BUS_NAME "im.telepathy.v1.ChannelDispatcher"
#define TP_CHANNEL_DISPATCHER_OBJECT_PATH "/im/telepathy/v1/ChannelDispatcher"
#define TP_CLIENT_BUS_NAME_BASE "im.telepathy.v1.Client."
#define TP_CLIENT_OBJECT_PATH_BASE "/im/telepathy/v1/Client/"
#define TP_DEBUG_OBJECT_PATH "/im/telepathy/v1/debug"
#define TP_LOGGER_BUS_NAME "im.telepathy.v1.Logger"
#define TP_LOGGER_OBJECT_PATH "/im/telepathy/v1/Logger"

#include <telepathy-glib/_gen/telepathy-interfaces.h>

G_END_DECLS

#endif
