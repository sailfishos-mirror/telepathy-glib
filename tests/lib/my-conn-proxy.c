/*
 * my-conn-proxy.c - a simple subclass of TpConnection
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */


#include "my-conn-proxy.h"

#include <telepathy-glib/proxy-internal.h>

G_DEFINE_TYPE  (TpTestsMyConnProxy, tp_tests_my_conn_proxy,
    TP_TYPE_CONNECTION)

static void
tp_tests_my_conn_proxy_init (TpTestsMyConnProxy *self)
{
}

enum {
    FEAT_CORE,
    N_FEAT
};

static void
prepare_core_async (TpProxy *proxy,
    const TpProxyFeature *feature,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;

  result = g_simple_async_result_new ((GObject *) proxy, callback, user_data,
      prepare_core_async);

  g_simple_async_result_complete_in_idle (result);
  g_object_unref (result);
}


static const TpProxyFeature *
list_features (TpProxyClass *cls G_GNUC_UNUSED)
{
  static TpProxyFeature features[N_FEAT + 1] = { { 0 } };

    if (G_LIKELY (features[0].name != 0))
    return features;

  features[FEAT_CORE].name = TP_TESTS_MY_CONN_PROXY_FEATURE_CORE;
  features[FEAT_CORE].core = TRUE;
  features[FEAT_CORE].prepare_async = prepare_core_async;

  return features;
}

static void
tp_tests_my_conn_proxy_class_init (TpTestsMyConnProxyClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;

  proxy_class->list_features = list_features;
}

GQuark
tp_tests_my_conn_proxy_get_feature_quark_core (void)
{
  return g_quark_from_static_string ("tp-my-conn-proxy-feature-core");
}
