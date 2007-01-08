/*
 * handle-repo-static.c - mechanism to store and retrieve handles on a connection
 * (implementation for handle types with a fixed list of possible handles)
 *
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
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

#include <telepathy-glib/handle-repo-static.h>
#include <string.h>

enum
{
  PROP_HANDLE_TYPE = 1,
  PROP_HANDLE_NAMES,
};

struct _TpStaticHandleRepoClass {
  GObjectClass parent_class;
};

struct _TpStaticHandleRepo {
  GObject parent;
  TpHandleType handle_type;
  TpHandle last_handle;
  gchar **handle_names;
};

static void static_repo_iface_init (gpointer g_iface,
    gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (TpStaticHandleRepo, tp_static_handle_repo,
    G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (TP_TYPE_HANDLE_REPO_IFACE,
        static_repo_iface_init));

static void
tp_static_handle_repo_init (TpStaticHandleRepo *self)
{
  self->handle_type = 0;
  self->last_handle = 0;
  self->handle_names = NULL;
}

static void
static_finalize (GObject *object)
{
  TpStaticHandleRepo *self = TP_STATIC_HANDLE_REPO (object);

  g_strfreev (self->handle_names);
  G_OBJECT_CLASS (tp_static_handle_repo_parent_class)->finalize (object);
}

static void
static_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpStaticHandleRepo *self = TP_STATIC_HANDLE_REPO (object);

  switch (property_id)
    {
    case PROP_HANDLE_TYPE:
      g_value_set_uint (value, self->handle_type);
      break;
    case PROP_HANDLE_NAMES:
      g_value_set_boxed (value, g_strdupv(self->handle_names));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
static_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpStaticHandleRepo *self = TP_STATIC_HANDLE_REPO (object);
  TpHandle i;

  switch (property_id)
    {
    case PROP_HANDLE_TYPE:
      self->handle_type = g_value_get_uint (value);
      break;
    case PROP_HANDLE_NAMES:
      g_strfreev (self->handle_names);
      self->handle_names = g_strdupv (g_value_get_boxed (value));
      i = 0;
      while (self->handle_names[i] != NULL)
        {
          i++;
        }
      self->last_handle = i;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_static_handle_repo_class_init (TpStaticHandleRepoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->get_property = static_get_property;
  object_class->set_property = static_set_property;
  object_class->finalize = static_finalize;

  g_object_class_override_property (object_class, PROP_HANDLE_TYPE, "handle-type");
  param_spec = g_param_spec_boxed ("handle-names", "Handle names",
      "The static set of handle names supported by this repo.",
      G_TYPE_STRV,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_HANDLE_NAMES, param_spec);
}

static gboolean
static_handle_is_valid (TpHandleRepoIface *irepo, TpHandle handle, GError **error)
{
  TpStaticHandleRepo *self = TP_STATIC_HANDLE_REPO (irepo);

  if (handle <= 0 || handle > self->last_handle)
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "handle %u is not valid for handle type %u '%s'",
          handle, self->handle_type, "TODO: label handle types");
      return FALSE;
    }
  else
    {
      return TRUE;
    }
}

static gboolean
static_handles_are_valid (TpHandleRepoIface *irepo, const GArray *handles,
    gboolean allow_zero, GError **error)
{
  guint i;

  g_return_val_if_fail (handles != NULL, FALSE);

  for (i = 0; i < handles->len; i++)
    {
      TpHandle handle = g_array_index (handles, TpHandle, i);

      if (handle == 0 && allow_zero)
        continue;

      if (!static_handle_is_valid (irepo, handle, error))
        return FALSE;
    }

  return TRUE;
}

static gboolean
static_ref_or_unref_handle (TpHandleRepoIface *self, TpHandle handle)
{
  /* nothing to do, handles in this repo are permanent */
  return TRUE;
}

static gboolean
static_client_hold_or_release_handle (TpHandleRepoIface *self,
    const gchar *client_name,
    TpHandle handle,
    GError **error)
{
  /* nothing to do, handles in this repo are permanent */
  return TRUE;
}

static const char *
static_inspect_handle (TpHandleRepoIface *irepo, TpHandle handle)
{
  TpStaticHandleRepo *self = TP_STATIC_HANDLE_REPO (irepo);

  g_assert (handle > 0 && handle <= self->last_handle);

  return self->handle_names[handle-1];
}

static TpHandle
static_request_handle (TpHandleRepoIface *irepo, const char *id,
    gboolean may_create)
{
  TpStaticHandleRepo *self = TP_STATIC_HANDLE_REPO (irepo);
  guint i;

  for (i = 0; i < self->last_handle; i++)
    {
      if (!strcmp(self->handle_names[i], id))
        return (TpHandle) i + 1;
    }

  return 0;
}

static gboolean
static_set_qdata (TpHandleRepoIface *repo, TpHandle handle,
    GQuark key_id, gpointer data, GDestroyNotify destroy)
{
  return FALSE;
}

static gpointer
static_get_qdata (TpHandleRepoIface *repo, TpHandle handle,
    GQuark key_id)
{
  return NULL;
}

static void
static_repo_iface_init (gpointer g_iface,
    gpointer iface_data)
{
  TpHandleRepoIfaceClass *klass = (TpHandleRepoIfaceClass *) g_iface;

  klass->handle_is_valid = static_handle_is_valid;
  klass->handles_are_valid = static_handles_are_valid;
  klass->ref_handle = static_ref_or_unref_handle;
  klass->unref_handle = static_ref_or_unref_handle;
  klass->client_hold_handle = static_client_hold_or_release_handle;
  klass->client_release_handle = static_client_hold_or_release_handle;
  klass->inspect_handle = static_inspect_handle;
  klass->request_handle = static_request_handle;
  klass->set_qdata = static_set_qdata;
  klass->get_qdata = static_get_qdata;
}

TpStaticHandleRepo *
tp_static_handle_repo_new (void)
{
  return TP_STATIC_HANDLE_REPO (g_object_new (TP_TYPE_STATIC_HANDLE_REPO,
        NULL));
}
