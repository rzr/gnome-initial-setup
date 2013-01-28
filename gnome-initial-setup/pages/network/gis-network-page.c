/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2013 Intel, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Written by:
 *      Alok Barsode <alok.barsode@intel.com>
 *      Michael Wood <michael.g.wood@intel.com>
 */

#define PAGE_ID "network"

#include <config.h>

#include <stdlib.h>
#include <glib/gi18n.h>

#include "gis-network-page.h"
#include "gis-network-resources.h"

#include "manager.h"
#include "technology.h"
#include "service.h"

G_DEFINE_TYPE (GisNetworkPage, gis_network_page, GIS_TYPE_PAGE);

#define WID(b, w) (GtkWidget *) gtk_builder_get_object (b, w)

#define NETWORK_PAGE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), GIS_TYPE_NETWORK_PAGE, GisNetworkPagePrivate))

enum {
  COLUMN_ICON,
  COLUMN_NAME,
  COLUMN_STATE,
  COLUMN_SECURITY_ICON,
  COLUMN_SECURITY,
  COLUMN_TYPE,
  COLUMN_STRENGTH_ICON,
  COLUMN_STRENGTH,
  COLUMN_FAVORITE,
  COLUMN_FAVORITE_ICON,
  COLUMN_GDBUSPROXY,
  COLUMN_ERROR,
  COLUMN_IMMUTABLE,
  COLUMN_AUTOCONNECT,
  COLUMN_ROAMING,
  COLUMN_PROXY,
  COLUMN_DOMAINS,
  COLUMN_IPv4,
  COLUMN_IPv6,
  COLUMN_NAMESERVERS,
  COLUMN_LAST
};

static void
gis_setup_settings_proxy (GisNetworkPage *page);

static void
gis_setup_settings_domains (GisNetworkPage *page);

static void
gis_setup_settings_ipv4 (GisNetworkPage *page);

static void
gis_setup_settings_ipv6 (GisNetworkPage *page);

static void
gis_setup_settings_nameservers (GisNetworkPage *page);

static void
manager_get_technologies (GObject        *source,
                          GAsyncResult     *res,
                          gpointer         user_data);

struct _GisNetworkPagePrivate
{
  GCancellable  *cancellable;
  GtkBuilder    *builder;
  Manager       *manager;
  GtkWidget     *offline_switch;

  Technology    *ethernet;
  GtkWidget     *ethernet_switch;

  Technology    *wifi;
  GtkWidget     *wifi_switch;

  Technology    *bluetooth;
  GtkWidget     *bluetooth_switch;

  Technology    *cellular;
  GtkWidget     *cellular_switch;

  gboolean      tech_update;
  gint          tech_added_id;
  gint          tech_removed_id;

  gint          tether_id;

  GHashTable    *services;
  gboolean      popup;
  gboolean      label_set;
  gint          available_nw;

  gint          combo_changed_id;
  gint          services_changed_id;
  gint          refresh_id;

  gint          connect_id;
  gint          forget_id;
  gint          configure_id;

  gint          settings_autoconnect_id;
  gint          settings_proxy_save_id;
  gint          settings_proxy_method_id;
  gint          settings_domains_save_id;
  gint          settings_ipv4_save_id;
  gint          settings_ipv4_method_id;
  gint          settings_ipv6_save_id;
  gint          settings_ipv6_method_id;
  gint          settings_nameservers_save_id;
  gint          tether_enable_id;
  gint          tether_cancel_id;
  gint          tether_ssid_id;
  gint          tether_psk_id;

  gboolean      tethering_wifi;
  gchar         *tethering_ssid;
  gchar         *tethering_psk;

  gboolean      tethering_bt;
};

static void
gis_network_page_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gis_network_page_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gis_show_error (GisNetworkPage *page, gchar *header, gchar *message)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (page))),
                                   GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_OK,
                                   header);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Connection Manager"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), message);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
gis_show_sugisess (GisNetworkPage *page, gchar *header, gchar *message)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (page))),
                                   GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_CLOSE,
                                   header);

  gtk_window_set_title (GTK_WINDOW (dialog), _("Connection Manager"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), message);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

}

static void
gis_set_combo_sensitivity (GisNetworkPage *page, gboolean sensitive)
{
  GisNetworkPagePrivate *priv = page->priv;
  GtkComboBox *combo;
  gboolean value;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));

  value = gtk_widget_get_sensitive (GTK_WIDGET (combo));
  if (value == sensitive)
    return;

  gtk_widget_set_sensitive (GTK_WIDGET (combo), sensitive);
}

static const gchar *gis_service_state_to_icon(const gchar *state)
{
  if ((g_strcmp0 (state, "ready") == 0) || (g_strcmp0 (state, "online") == 0))
    return "gtk-yes";
  else
    return "gtk-no";
}

static const gchar *gis_service_security_to_icon (const gchar **security)
{
  if (security == NULL)
    return NULL;

  if (g_strcmp0 (security[0], "none") == 0)
    return NULL;
  else
    return "network-wireless-encrypted-symbolic";
}

static const gchar *gis_service_security_to_string (const gchar **security)
{
  if (security == NULL || g_strcmp0 (security[0], "none") == 0)
    return "None";
  else
    return g_strjoinv (" / ", (gchar **) security);
}

static const gchar *gis_service_strength_to_string (const gchar *type, const gchar strength)
{
  if (g_strcmp0 (type, "ethernet") == 0)
    return "Excellent";

    if (strength > 80)
        return "Excellent";
    if (strength > 55)
        return "Good";
    if (strength > 30)
        return "ok";
    if (strength > 5)
        return "weak";

    return "n/a";
}

static const gchar *gis_service_type_to_icon (const gchar *type, gchar strength)
{
  if (g_strcmp0 (type, "ethernet") == 0)
    return "network-wired-symbolic";
  else if (g_strcmp0 (type, "bluetooth") == 0)
    return "bluetooth-active-symbolic";
  else if (g_strcmp0 (type, "wifi") == 0) {
    if (strength > 80)
        return "network-wireless-signal-excellent-symbolic";
    if (strength > 55)
        return "network-wireless-signal-good-symbolic";
    if (strength > 30)
        return "network-wireless-signal-ok-symbolic";
    if (strength > 5)
        return "network-wireless-signal-weak-symbolic";
    return NULL;
  } else
    return NULL;
}

static void
service_property_changed (Service *service,
                          const gchar *property,
                          GVariant *value,
                          GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  GtkListStore *liststore_services;
  GtkComboBox *combo;

  GtkTreeRowReference *row;
  GtkTreePath *tree_path;
  GtkTreeIter iter;
  gint active;

  const gchar *path;
  gchar *type = NULL;
  gchar strength = 0;
  const gchar *state;
  gchar *str;

  const gchar *error_str;
  gboolean favorite;
  gboolean autoconnect;
  gboolean roaming;
  gboolean immutable;
  GVariant *proxy, *domains, *ipv4, *nameservers;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));
  active = gtk_combo_box_get_active (combo);

  path = g_dbus_proxy_get_object_path ((GDBusProxy *) service);

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  row = g_hash_table_lookup (priv->services, path);

  if (row == NULL)
      return;

  tree_path = gtk_tree_row_reference_get_path (row);

  gtk_tree_model_get_iter ((GtkTreeModel *) liststore_services, &iter, tree_path);

  if (!g_strcmp0 (property, "Strength")) {
    strength = (gchar ) g_variant_get_byte (g_variant_get_variant (value));

    gtk_tree_model_get (GTK_TREE_MODEL (liststore_services), &iter, COLUMN_TYPE, &type, -1);

    gtk_list_store_set (liststore_services,
                        &iter,
                        COLUMN_STRENGTH, gis_service_strength_to_string (type, strength),
                        -1);

    gtk_list_store_set (liststore_services,
                        &iter,
                        COLUMN_STRENGTH_ICON, gis_service_type_to_icon (type, strength),
                        -1);
    if (gtk_tree_path_compare (tree_path, gtk_tree_path_new_from_indices (active, -1)) == 0)
      gtk_label_set_label (GTK_LABEL (WID (priv->builder, "label_strength")), gis_service_strength_to_string (type, strength));
  }

  if (!g_strcmp0 (property, "State")) {
    state = g_variant_get_string (g_variant_get_variant (value), NULL);

    gtk_tree_model_get (GTK_TREE_MODEL (liststore_services), &iter, COLUMN_STATE, &str, -1);

    g_free (str);

    gtk_list_store_set (liststore_services,
                        &iter,
                        COLUMN_STATE, g_strdup (state),
                        -1);

    gtk_list_store_set (liststore_services,
                        &iter,
                        COLUMN_ICON, gis_service_state_to_icon (state),
                        -1);

    if ((g_strcmp0 (state, "ready") == 0) || (g_strcmp0 (state, "online") == 0))
      gtk_button_set_label (GTK_BUTTON (WID (priv->builder, "button_connect")), "Disconnect");
    else
      gtk_button_set_label (GTK_BUTTON (WID (priv->builder, "button_connect")), "Connect");

  }

  if (!g_strcmp0 (property, "Favorite")) {
    favorite = g_variant_get_boolean (g_variant_get_variant (value));

    gtk_list_store_set (liststore_services,
                        &iter,
                        COLUMN_FAVORITE, favorite,
                        -1);

    gtk_list_store_set (liststore_services,
                        &iter,
                        COLUMN_FAVORITE_ICON, favorite ? "starred-symbolic" : NULL,
                        -1);

    if (favorite)
      gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "button_forget")), TRUE);
    else
      gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "button_forget")), FALSE);

  }

  if (!g_strcmp0 (property, "AutoConnect")) {
    autoconnect = g_variant_get_boolean (g_variant_get_variant (value));

    gtk_list_store_set (liststore_services,
                        &iter,
                        COLUMN_AUTOCONNECT, autoconnect,
                        -1);

    if (gtk_tree_path_compare (tree_path, gtk_tree_path_new_from_indices (active, -1)) == 0)
      gtk_switch_set_active (GTK_SWITCH (WID (priv->builder, "switch_autoconnect")), autoconnect);
  }

  if (!g_strcmp0 (property, "Immutable")) {
    immutable = g_variant_get_boolean (g_variant_get_variant (value));

    gtk_list_store_set (liststore_services,
                        &iter,
                        COLUMN_IMMUTABLE, immutable,
                        -1);

    if (gtk_tree_path_compare (tree_path, gtk_tree_path_new_from_indices (active, -1)) == 0)
      gtk_label_set_label (GTK_LABEL (WID (priv->builder, "label_immutable")), immutable ? "Yes" : "No");
  }

  if (!g_strcmp0 (property, "Roaming")) {
    roaming = g_variant_get_boolean (g_variant_get_variant (value));

    gtk_list_store_set (liststore_services,
                        &iter,
                        COLUMN_ROAMING, roaming,
                        -1);

    if (gtk_tree_path_compare (tree_path, gtk_tree_path_new_from_indices (active, -1)) == 0)
      gtk_label_set_label (GTK_LABEL (WID (priv->builder, "label_roaming")), roaming ? "Yes" : "No");
  }

  if (!g_strcmp0 (property, "Error")) {
    error_str = g_variant_get_string (g_variant_get_variant (value), NULL);

    gtk_list_store_set (liststore_services,
                        &iter,
                        COLUMN_ERROR, error_str,
                        -1);
    if (gtk_tree_path_compare (tree_path, gtk_tree_path_new_from_indices (active, -1)) == 0)
      gtk_label_set_label (GTK_LABEL (WID (priv->builder, "label_error")), error_str);
  }

  if (!g_strcmp0 (property, "Proxy")) {

    proxy = g_variant_get_variant (value);

    gtk_list_store_set (liststore_services,
                        &iter,
                        COLUMN_PROXY, g_variant_ref (proxy),
                        -1);

    if (gtk_tree_path_compare (tree_path, gtk_tree_path_new_from_indices (active, -1)) == 0)
      gis_setup_settings_proxy (page);
  }

  if (!g_strcmp0 (property, "Domains")) {

    domains = g_variant_get_variant (value);

    gtk_list_store_set (liststore_services,
                        &iter,
                        COLUMN_DOMAINS, g_variant_ref (domains),
                        -1);

    if (gtk_tree_path_compare (tree_path, gtk_tree_path_new_from_indices (active, -1)) == 0)
      gis_setup_settings_domains (page);
  }

  if (!g_strcmp0 (property, "IPv4")) {

    ipv4 = g_variant_get_variant (value);

    gtk_list_store_set (liststore_services,
                        &iter,
                        COLUMN_IPv4, g_variant_ref (ipv4),
                        -1);

    if (gtk_tree_path_compare (tree_path, gtk_tree_path_new_from_indices (active, -1)) == 0)
      gis_setup_settings_ipv4 (page);
  }

  if (!g_strcmp0 (property, "IPv6")) {

    ipv4 = g_variant_get_variant (value);

    gtk_list_store_set (liststore_services,
                        &iter,
                        COLUMN_IPv6, g_variant_ref (ipv4),
                        -1);

    if (gtk_tree_path_compare (tree_path, gtk_tree_path_new_from_indices (active, -1)) == 0)
      gis_setup_settings_ipv6 (page);
  }

  if (!g_strcmp0 (property, "Nameservers")) {

    nameservers = g_variant_get_variant (value);

    gtk_list_store_set (liststore_services,
                        &iter,
                        COLUMN_NAMESERVERS, g_variant_ref (nameservers),
                        -1);

    if (gtk_tree_path_compare (tree_path, gtk_tree_path_new_from_indices (active, -1)) == 0)
      gis_setup_settings_nameservers (page);
  }
}

static void
gis_add_service (const gchar         *path,
                GVariant            *properties,
                GisNetworkPage      *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  GError *error = NULL;
  gboolean ret;

  GVariant *value = NULL;
  GtkListStore *liststore_services;
  GtkTreeIter iter;
  GtkTreePath *tree_path;
  GtkTreeRowReference *row;

  Service *service;
  const gchar *name;
  const gchar *state;
  const gchar **security = NULL;
  const gchar *type;
  const gchar *serv_error;

  gchar strength = 0;
  gboolean favorite = FALSE;
  gboolean roaming = FALSE;
  gboolean autoconnect = FALSE;
  gboolean immutable = FALSE;
  GVariant *proxy, *domains, *ipv4, *ipv6, *nameservers;

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  gtk_list_store_append (liststore_services, &iter);

  value = g_variant_lookup_value (properties, "Name", G_VARIANT_TYPE_STRING);
  if (value == NULL)
    name = g_strdup ("Connect to a Hidden Network");
  else
    name = g_variant_get_string (value, NULL);

  value = g_variant_lookup_value (properties, "State", G_VARIANT_TYPE_STRING);
  state = g_variant_get_string (value, NULL);

  value = g_variant_lookup_value (properties, "Type", G_VARIANT_TYPE_STRING);
  type = g_variant_get_string (value, NULL);

  ret = g_variant_lookup (properties, "Favorite", "b", &favorite);
  ret = g_variant_lookup (properties, "Immutable", "b", &immutable);
  ret = g_variant_lookup (properties, "AutoConnect", "b", &autoconnect);
  ret = g_variant_lookup (properties, "Roaming", "b", &roaming);

  if (!g_strcmp0 (type, "wifi")) {
    value = g_variant_lookup_value (properties, "Security", G_VARIANT_TYPE_STRING_ARRAY);
    security = g_variant_get_strv (value, NULL);

    value = g_variant_lookup_value (properties, "Strength", G_VARIANT_TYPE_BYTE);
    strength = (gchar ) g_variant_get_byte (value);
  }

  ret = g_variant_lookup (properties, "Error", "s", &serv_error);
  if (!ret)
    serv_error = "None";

  service = service_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                            G_DBUS_PROXY_FLAGS_NONE,
                                            "net.connman",
                                            path,
                                            priv->cancellable,
                                            &error);

  if (error != NULL) {
    g_warning ("could not get proxy for service %s: %s", name,  error->message);
    g_error_free (error);
  }

  proxy = g_variant_lookup_value (properties, "Proxy", G_VARIANT_TYPE_DICTIONARY);

  domains = g_variant_lookup_value (properties, "Domains", G_VARIANT_TYPE_STRING_ARRAY);

  ipv4 = g_variant_lookup_value (properties, "IPv4", G_VARIANT_TYPE_DICTIONARY);

  ipv6 = g_variant_lookup_value (properties, "IPv6", G_VARIANT_TYPE_DICTIONARY);

  nameservers = g_variant_lookup_value (properties, "Nameservers", G_VARIANT_TYPE_STRING_ARRAY);

  gtk_list_store_set (liststore_services,
                      &iter,
                      COLUMN_ICON, gis_service_state_to_icon (state),
                      COLUMN_NAME, g_strdup (name),
                      COLUMN_STATE, g_strdup (state),
                      COLUMN_SECURITY_ICON, gis_service_security_to_icon (security),
                      COLUMN_SECURITY, gis_service_security_to_string (security),
                      COLUMN_TYPE, g_strdup (type),
                      COLUMN_STRENGTH_ICON, gis_service_type_to_icon (type, strength),
                      COLUMN_STRENGTH, gis_service_strength_to_string (type, strength),
                      COLUMN_FAVORITE, favorite,
                      COLUMN_FAVORITE_ICON, favorite ? "starred-symbolic" : NULL,
                      COLUMN_GDBUSPROXY, service,
                      COLUMN_ERROR, g_strdup (serv_error),
                      COLUMN_IMMUTABLE, immutable,
                      COLUMN_AUTOCONNECT, autoconnect,
                      COLUMN_ROAMING, roaming,
                      COLUMN_PROXY, g_variant_ref (proxy),
                      COLUMN_DOMAINS, g_variant_ref (domains),
                      COLUMN_IPv4, g_variant_ref (ipv4),
                      COLUMN_IPv6, g_variant_ref (ipv6),
                      COLUMN_NAMESERVERS, g_variant_ref (nameservers),
                      -1);

  tree_path = gtk_tree_model_get_path ((GtkTreeModel *) liststore_services, &iter);

  row = gtk_tree_row_reference_new ((GtkTreeModel *) liststore_services, tree_path);

  g_hash_table_insert (priv->services,
                       g_strdup (path),
                       gtk_tree_row_reference_copy (row));

  g_signal_connect (service,
                    "property_changed",
                    G_CALLBACK (service_property_changed),
                    page);

  gtk_tree_path_free (tree_path);
  gtk_tree_row_reference_free (row);
}

static void
manager_services_changed (Manager *manager,
                          GVariant *added,
                          const gchar *const *removed,
                          GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  GtkListStore *liststore_services;
  gint i;

  GtkTreeRowReference *row;
  GtkTreePath *tree_path;
  GtkTreeIter iter;

  GVariant *array_value, *tuple_value, *properties;
  GVariantIter array_iter, tuple_iter;
  gchar *path;
  gchar *label;

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  for (i=0; removed != NULL && removed[i] != NULL; i++) {
    row = g_hash_table_lookup (priv->services, (gconstpointer *)removed[i]);

    if (row == NULL)
      continue;

    tree_path = gtk_tree_row_reference_get_path (row);

    gtk_tree_model_get_iter ((GtkTreeModel *) liststore_services, &iter, tree_path);

    gtk_list_store_remove (liststore_services, &iter);

    g_hash_table_remove (priv->services, removed[i]);

    gtk_tree_path_free (tree_path);

    priv->available_nw--;
  }

  g_variant_iter_init (&array_iter, added);

  /* Added Services*/
  while ((array_value = g_variant_iter_next_value (&array_iter)) != NULL) {
    /* tuple_iter is oa{sv} */
    g_variant_iter_init (&tuple_iter, array_value);

    /* get the object path */
    tuple_value = g_variant_iter_next_value (&tuple_iter);
    g_variant_get (tuple_value, "o", &path);

    /* Found a new item, so add it */
    if (g_hash_table_lookup (priv->services, (gconstpointer *)path) == NULL) {
      /* get the Properties */
      properties = g_variant_iter_next_value (&tuple_iter);

      gis_add_service (path, properties, page);
      g_variant_unref (properties);
      priv->available_nw++;
    }

    g_free (path);
    g_variant_unref (array_value);
    g_variant_unref (tuple_value);
  }

  if (!priv->label_set)
    return;

  tree_path = gtk_tree_path_new_from_string ("0");

  gtk_tree_model_get_iter (GTK_TREE_MODEL (liststore_services),
                           &iter,
                           tree_path);

  gtk_tree_path_free (tree_path);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services), &iter, 1, &label, -1);

  g_free (label);

  label = g_strdup_printf (" %d Networks Available", priv->available_nw);

  gtk_list_store_set (liststore_services,
                      &iter,
                      COLUMN_NAME, label,
                      -1);

  if (priv->available_nw == 0)
    gis_set_combo_sensitivity(page, FALSE);
  else
    gis_set_combo_sensitivity(page, TRUE);

}

static void
manager_get_services (GObject        *source,
                      GAsyncResult   *res,
                      gpointer       user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;

  GtkListStore *liststore_services;
  GtkTreeIter iter;
  GtkComboBox *combo;
  gchar *label;

  GError *error;
  GVariant *result, *array_value, *tuple_value, *properties;
  GVariantIter array_iter, tuple_iter;
  gchar *path;
  gsize size;

  error = NULL;
  if (!manager_call_get_services_finish (priv->manager, &result,
                                           res, &error))
    {
      /* TODO: display any error in a user friendly way */
      g_warning ("Could not get services: %s", error->message);
      g_error_free (error);
      return;
    }

  /* Result is  (a(oa{sv}))*/

  g_variant_iter_init (&array_iter, result);

  /* 1st item in the ComboBox is the available Networks */
  size = g_variant_iter_n_children (&array_iter);
  priv->available_nw = (gint) size;
  label = g_strdup_printf (" %d Networks Available", (gint) size);

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));
  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));
  gtk_list_store_append (liststore_services, &iter);

  gtk_list_store_set (liststore_services,
                      &iter,
                      COLUMN_NAME, label,
                      -1);

  gtk_combo_box_set_active (combo, 0);
  priv->label_set = TRUE;

  if (priv->available_nw == 0)
    goto out;

  gis_set_combo_sensitivity(page, TRUE);
  /* Update the service list */

  while ((array_value = g_variant_iter_next_value (&array_iter)) != NULL) {
    /* tuple_iter is oa{sv} */
    g_variant_iter_init (&tuple_iter, array_value);

    /* get the object path */
    tuple_value = g_variant_iter_next_value (&tuple_iter);
    g_variant_get (tuple_value, "o", &path);

    /* get the Properties */
    properties = g_variant_iter_next_value (&tuple_iter);

    gis_add_service (path, properties, page);

    g_free (path);

    g_variant_unref (array_value);
    g_variant_unref (tuple_value);
    g_variant_unref (properties);
  }

out:
  priv->services_changed_id = g_signal_connect (priv->manager,
                                                "services_changed",
                                                G_CALLBACK (manager_services_changed),
                                                page);

  gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "button_refresh")), FALSE);

  gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "button_connect")), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "button_forget")), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "togglebutton_configure")), FALSE);
}

static gint
gis_check_activeitem_set (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  GtkComboBox *combo;
  gint active;
  GtkListStore *liststore_services;

  if (priv->popup == TRUE)
    return FALSE;

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));
  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));

  active = gtk_combo_box_get_active (combo);

  if (active == -1) {
    g_signal_handler_disconnect (combo, priv->combo_changed_id);

    gtk_list_store_clear (liststore_services);

    manager_call_get_services (priv->manager, priv->cancellable, manager_get_services, page);
  }

  return FALSE;
}

static void
gis_button_refresh_pressed (GtkButton *button, gpointer user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  GtkListStore *liststore_services;

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  gtk_list_store_clear (liststore_services);

  manager_call_get_services (priv->manager, priv->cancellable, manager_get_services, page);
}

static void
gis_button_connect_pressed (GtkButton *button, gpointer user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  gint active;
  GtkComboBox *combo;
  GError *error = NULL;
  const gchar *state;

  GtkListStore *liststore_services;
  GtkTreePath *tree_path;
  GtkTreeIter iter;

  Service *service;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));
  active = gtk_combo_box_get_active (combo);

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  tree_path = gtk_tree_path_new_from_indices (active, -1);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (liststore_services),
                           &iter,
                           tree_path);

  gtk_tree_path_free (tree_path);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services), &iter, COLUMN_GDBUSPROXY, &service, -1);
  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services), &iter, COLUMN_STATE, &state, -1);

  if (service != NULL) {
    if ((g_strcmp0 (state, "ready") == 0) || (g_strcmp0 (state, "online") == 0))
      service_call_disconnect_sync (service, priv->cancellable , &error);
    else
      service_call_connect_sync (service, priv->cancellable , &error);

    if (error != NULL) {
      g_warning ("Could not connect to service: %s", error->message);
      g_error_free (error);
      return;
    }
  }
}

static void
gis_button_forget_pressed (GtkButton *button, gpointer user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  gint active;
  GtkComboBox *combo;
  GError *error = NULL;

  GtkListStore *liststore_services;
  GtkTreePath *tree_path;
  GtkTreeIter iter;

  Service *service;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));
  active = gtk_combo_box_get_active (combo);

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  tree_path = gtk_tree_path_new_from_indices (active, -1);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (liststore_services),
                           &iter,
                           tree_path);

  gtk_tree_path_free (tree_path);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services), &iter, COLUMN_GDBUSPROXY, &service, -1);

  if (service != NULL) {
    service_call_remove_sync (service, priv->cancellable , &error);

    if (error != NULL) {
      g_warning ("Could not remove service: %s", error->message);
      g_error_free (error);
      return;
    }
  }
}

static void
service_set_autoconnect (GObject      *source,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  gint active;
  GtkComboBox *combo;

  GtkListStore *liststore_services;
  GtkTreePath *tree_path;
  GtkTreeIter iter;

  Service *service;
  gboolean autoconnect;

  GError *error = NULL;
  gint err_code;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));
  active = gtk_combo_box_get_active (combo);

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  tree_path = gtk_tree_path_new_from_indices (active, -1);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (liststore_services),
                           &iter,
                           tree_path);

  gtk_tree_path_free (tree_path);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services),
                                        &iter,
                                        COLUMN_GDBUSPROXY, &service,
                                        -1);


  if (!service_call_set_property_finish (service, res, &error)) {
    err_code = error->code;
    /* Reset the switch if error is not AlreadyEnabled/AlreadyDisabled */
    if (err_code != 36) { /* Not AlreadyEnabled */
      autoconnect = gtk_switch_get_active (GTK_SWITCH (WID (priv->builder, "switch_autoconnect")));
      gtk_switch_set_active (GTK_SWITCH (WID (priv->builder, "switch_autoconnect")), !autoconnect);
    } else
      g_warning ("Could not set service autoconnect property: %s", error->message);

    g_error_free (error);
    return;
  }
}

static void
gis_autoconnect_switch_toggle (GtkSwitch *sw,
                              GParamSpec *pspec,
                              GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  gint active;
  GtkComboBox *combo;

  GtkListStore *liststore_services;
  GtkTreePath *tree_path;
  GtkTreeIter iter;

  Service *service;
  gboolean autoconnect;
  GVariant *value;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));
  active = gtk_combo_box_get_active (combo);

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  tree_path = gtk_tree_path_new_from_indices (active, -1);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (liststore_services),
                           &iter,
                           tree_path);

  gtk_tree_path_free (tree_path);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services),
                                        &iter,
                                        COLUMN_GDBUSPROXY, &service,
                                        -1);

  autoconnect = gtk_switch_get_active (sw);
  value = g_variant_new_boolean (autoconnect);

  service_call_set_property (service,
                             "AutoConnect",
                             g_variant_new_variant (value),
                             priv->cancellable,
                             service_set_autoconnect,
                             page);
}

static void
gis_setup_settings_details (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  gint active;
  GtkComboBox *combo;

  GtkListStore *liststore_services;
  GtkTreePath *tree_path;
  GtkTreeIter iter;

  gboolean autoconnect;
  gboolean roaming;
  gboolean immutable;
  const gchar *error_str;
  const gchar *signal_strength;
  const gchar *security;
  const gchar *type;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));
  active = gtk_combo_box_get_active (combo);

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  tree_path = gtk_tree_path_new_from_indices (active, -1);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (liststore_services),
                           &iter,
                           tree_path);

  gtk_tree_path_free (tree_path);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services),
                                        &iter,
                                        COLUMN_TYPE, &type,
                                        COLUMN_AUTOCONNECT, &autoconnect,
                                        COLUMN_IMMUTABLE, &immutable,
                                        COLUMN_ROAMING, &roaming,
                                        COLUMN_ERROR, &error_str,
                                        COLUMN_SECURITY, &security,
                                        COLUMN_STRENGTH, &signal_strength,
                                        -1);

  gtk_switch_set_active (GTK_SWITCH (WID (priv->builder, "switch_autoconnect")), autoconnect);

  if (!priv->settings_autoconnect_id) {
    priv->settings_autoconnect_id = g_signal_connect (GTK_SWITCH (WID (priv->builder, "switch_autoconnect")),
                                                      "notify::active",
                                                      G_CALLBACK (gis_autoconnect_switch_toggle),
                                                      page);
  }

  gtk_label_set_label (GTK_LABEL (WID (priv->builder, "label_strength")), signal_strength);
  gtk_label_set_label (GTK_LABEL (WID (priv->builder, "label_security")), security);
  gtk_label_set_label (GTK_LABEL (WID (priv->builder, "label_error")), error_str);
  gtk_label_set_label (GTK_LABEL (WID (priv->builder, "label_roaming")), roaming ? "Yes" : "No");
  gtk_label_set_label (GTK_LABEL (WID (priv->builder, "label_immutable")), immutable ? "Yes" : "No");
}

static void
gis_setup_settings_proxy_widgets (GisNetworkPage *page, gchar *method)
{
  GisNetworkPagePrivate *priv = page->priv;

  if (!g_strcmp0 (method, "direct")) {
    gtk_widget_hide (GTK_WIDGET (WID (priv->builder, "entry_proxy_url_box")));
    gtk_widget_hide (GTK_WIDGET (WID (priv->builder, "entry_proxy_servers_box")));
    gtk_widget_hide (GTK_WIDGET (WID (priv->builder, "entry_proxy_excludes_box")));
  } else  if (!g_strcmp0 (method, "auto")) {
    gtk_widget_show (GTK_WIDGET (WID (priv->builder, "entry_proxy_url_box")));
    gtk_widget_hide (GTK_WIDGET (WID (priv->builder, "entry_proxy_servers_box")));
    gtk_widget_hide (GTK_WIDGET (WID (priv->builder, "entry_proxy_excludes_box")));
  }else {
    gtk_widget_hide (GTK_WIDGET (WID (priv->builder, "entry_proxy_url_box")));
    gtk_widget_show (GTK_WIDGET (WID (priv->builder, "entry_proxy_servers_box")));
    gtk_widget_show (GTK_WIDGET (WID (priv->builder, "entry_proxy_excludes_box")));
  }
}

static void
gis_proxy_method_changed (GtkComboBox *combo, gpointer user_data)
{
  GisNetworkPage *page = user_data;
  gint active;

  active = gtk_combo_box_get_active (combo);
  if (active == 0)
    gis_setup_settings_proxy_widgets (page, "direct");
  else if (active == 1)
    gis_setup_settings_proxy_widgets (page, "auto");
  else
    gis_setup_settings_proxy_widgets (page, "manual");
}

static void
gis_button_proxy_save_cb (GtkButton *button, gpointer user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  gint active;
  GtkComboBox *combo;
  GError *error = NULL;

  GtkListStore *liststore_services;
  GtkTreePath *tree_path;
  GtkTreeIter iter;

  Service *service;
  GVariantBuilder *proxyconf = g_variant_builder_new(G_VARIANT_TYPE_DICTIONARY);
  GVariant *value;
  gchar *str;
  gchar **servers = NULL;
  gchar **excludes = NULL;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));
  active = gtk_combo_box_get_active (combo);

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  tree_path = gtk_tree_path_new_from_indices (active, -1);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (liststore_services),
                           &iter,
                           tree_path);

  gtk_tree_path_free (tree_path);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services), &iter, COLUMN_GDBUSPROXY, &service, -1);

  active = gtk_combo_box_get_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_proxy_method")));

  if (active == 0) { /* Direct */
    g_variant_builder_add(proxyconf,"{sv}", "Method", g_variant_new_string ("direct"));
  }

  if (active == 1) { /* Automatic */
    g_variant_builder_add (proxyconf,"{sv}", "Method", g_variant_new_string ("auto"));
    if (gtk_entry_get_text_length (GTK_ENTRY (WID (priv->builder, "entry_proxy_url"))) > 0)
      g_variant_builder_add (proxyconf,"{sv}", "URL", g_variant_new_string (gtk_entry_get_text (GTK_ENTRY (WID (priv->builder, "entry_proxy_url")))));
  }

  if (active == 2) { /* Manual */
    g_variant_builder_add (proxyconf,"{sv}", "Method", g_variant_new_string ("manual"));

    str = (gchar *) gtk_entry_get_text (GTK_ENTRY (WID (priv->builder, "entry_proxy_servers")));
    if (str) {
      servers = g_strsplit (str, ",", -1);
      g_variant_builder_add (proxyconf,"{sv}", "Servers", g_variant_new_strv ((const gchar * const *) servers, -1));
    }

    str = (gchar *) gtk_entry_get_text (GTK_ENTRY (WID (priv->builder, "entry_proxy_excludes")));
    if (str) {
      excludes = g_strsplit (str, ",", -1);
      g_variant_builder_add (proxyconf,"{sv}", "Excludes", g_variant_new_strv ((const gchar * const *) excludes, -1));
    }

  }

  value = g_variant_builder_end(proxyconf);

  service_call_set_property_sync (service,
                                  "Proxy.Configuration",
                                  g_variant_new_variant (value),
                                  priv->cancellable,
                                  &error);
  if (error) {
    gis_show_error (page, _("Unable to save Proxy"), error->message);
    g_error_free (error);
  } else {
    gis_show_sugisess (page, _("Proxy Settings"), _("Proxy saved sugisesfully"));
  }

}

static void
gis_setup_settings_proxy (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  gint active;
  GtkComboBox *combo;

  GtkListStore *liststore_services;
  GtkTreePath *tree_path;
  GtkTreeIter iter;

  gboolean ret;
  GVariant *proxy;
  GVariant *value;
  gchar *method;
  gchar *url;
  const gchar **servers;
  const gchar **excludes;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));
  active = gtk_combo_box_get_active (combo);

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  tree_path = gtk_tree_path_new_from_indices (active, -1);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (liststore_services),
                           &iter,
                           tree_path);

  gtk_tree_path_free (tree_path);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services),
                      &iter,
                      COLUMN_PROXY, &proxy,
                      -1);

  ret = g_variant_lookup (proxy, "Method", "s", &method);
  if (!ret)
    method = "direct";

  gis_setup_settings_proxy_widgets (page, method);

  /* Set the method combobox */

  if (!g_strcmp0 (method, "direct"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_proxy_method")), 0);
  else  if (!g_strcmp0 (method, "auto")) {
    gtk_combo_box_set_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_proxy_method")), 1);
    g_variant_lookup (proxy, "URL", "s", &url);
    gtk_entry_set_text (GTK_ENTRY (WID (priv->builder, "entry_proxy_url")), url);
  } else {
    gtk_combo_box_set_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_proxy_method")), 2);

    value = g_variant_lookup_value (proxy, "Servers", G_VARIANT_TYPE_STRING_ARRAY);
    servers = g_variant_get_strv (value, NULL);

    value = g_variant_lookup_value (proxy, "Excludes", G_VARIANT_TYPE_STRING_ARRAY);
    excludes = g_variant_get_strv (value, NULL);

    if (servers != NULL)
      gtk_entry_set_text (GTK_ENTRY (WID (priv->builder, "entry_proxy_servers")), g_strjoinv (",", (gchar **) servers));

    if (excludes != NULL)
      gtk_entry_set_text (GTK_ENTRY (WID (priv->builder, "entry_proxy_excludes")), g_strjoinv (",", (gchar **) excludes));
  }

  if (!priv->settings_proxy_save_id) {
    priv->settings_proxy_save_id = g_signal_connect (GTK_BUTTON (WID (priv->builder, "button_proxy_save")),
                                                     "clicked",
                                                     G_CALLBACK (gis_button_proxy_save_cb),
                                                     page);
  }

  if (!priv->settings_proxy_method_id) {
    priv->settings_proxy_method_id = g_signal_connect (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_proxy_method")),
                                                       "changed",
                                                       G_CALLBACK (gis_proxy_method_changed),
                                                       page);
  }
}

static void
gis_button_domains_save_cb (GtkButton *button, gpointer user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  gint active;
  GtkComboBox *combo;
  GError *error = NULL;

  GtkListStore *liststore_services;
  GtkTreePath *tree_path;
  GtkTreeIter iter;

  Service *service;
  GVariant *value;
  gchar *str = NULL;
  gchar **dns = NULL;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));
  active = gtk_combo_box_get_active (combo);

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  tree_path = gtk_tree_path_new_from_indices (active, -1);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (liststore_services),
                           &iter,
                           tree_path);

  gtk_tree_path_free (tree_path);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services), &iter, COLUMN_GDBUSPROXY, &service, -1);

  str = (gchar *) gtk_entry_get_text (GTK_ENTRY (WID (priv->builder, "entry_domains")));
  if (str) {
    dns = g_strsplit (str, ",", -1);

    value = g_variant_new_strv ((const gchar * const *) dns, -1);

    service_call_set_property_sync (service,
                                    "Domains.Configuration",
                                    g_variant_new_variant (value),
                                    priv->cancellable,
                                    &error);

    if (error) {
      gis_show_error (page, _("Unable to save DNS"), error->message);
      g_error_free (error);
    } else {
      gis_show_sugisess (page, _("Domains Settings"), _("DNS saved sugisesfully"));
    }
  }
}

static void
gis_setup_settings_domains (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  gint active;
  GtkComboBox *combo;

  GtkListStore *liststore_services;
  GtkTreePath *tree_path;
  GtkTreeIter iter;

  GVariant *domains;
  const gchar **dns;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));
  active = gtk_combo_box_get_active (combo);

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  tree_path = gtk_tree_path_new_from_indices (active, -1);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (liststore_services),
                           &iter,
                           tree_path);

  gtk_tree_path_free (tree_path);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services),
                      &iter,
                      COLUMN_DOMAINS, &domains,
                      -1);

  dns = g_variant_get_strv (domains, NULL);

  if (dns != NULL)
    gtk_entry_set_text (GTK_ENTRY (WID (priv->builder, "entry_domains")), g_strjoinv (",", (gchar **) dns));

  if (!priv->settings_domains_save_id) {
    priv->settings_domains_save_id = g_signal_connect (GTK_BUTTON (WID (priv->builder, "button_domains_save")),
                                                      "clicked",
                                                      G_CALLBACK (gis_button_domains_save_cb),
                                                      page);
  }
}

static void
gis_setup_settings_ipv4_widgets (GisNetworkPage *page, gchar *method)
{
  GisNetworkPagePrivate *priv = page->priv;

  if (!g_strcmp0 (method, "off")) {
    gtk_widget_hide (GTK_WIDGET (WID (priv->builder, "entry_ipv4_address_box")));
    gtk_widget_hide (GTK_WIDGET (WID (priv->builder, "entry_ipv4_netmask_box")));
    gtk_widget_hide (GTK_WIDGET (WID (priv->builder, "entry_ipv4_gateway_box")));
  } else {
    gtk_widget_show (GTK_WIDGET (WID (priv->builder, "entry_ipv4_address_box")));
    gtk_widget_show (GTK_WIDGET (WID (priv->builder, "entry_ipv4_netmask_box")));
    gtk_widget_show (GTK_WIDGET (WID (priv->builder, "entry_ipv4_gateway_box")));
  }

  if (!g_strcmp0 (method, "manual")) {
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "entry_ipv4_address")), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "entry_ipv4_netmask")), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "entry_ipv4_gateway")), TRUE);
  } else {
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "entry_ipv4_address")), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "entry_ipv4_netmask")), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "entry_ipv4_gateway")), FALSE);
  }
}

static void
gis_ipv4_method_changed (GtkComboBox *combo, gpointer user_data)
{
  GisNetworkPage *page = user_data;
  gint active;

  active = gtk_combo_box_get_active (combo);
  if (active == 0)
    gis_setup_settings_ipv4_widgets (page, "off");
  else if (active == 1)
    gis_setup_settings_ipv4_widgets (page, "dhcp");
  else if (active == 2)
    gis_setup_settings_ipv4_widgets (page, "manual");
  else
    gis_setup_settings_ipv4_widgets (page, "fixed");
}

static void
gis_button_ipv4_save_cb (GtkButton *button, gpointer user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  gint active;
  GtkComboBox *combo;
  GError *error = NULL;

  GtkListStore *liststore_services;
  GtkTreePath *tree_path;
  GtkTreeIter iter;

  Service *service;
  GVariantBuilder *ipv4conf = g_variant_builder_new (G_VARIANT_TYPE_DICTIONARY);
  GVariant *value;
  gchar *str;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));
  active = gtk_combo_box_get_active (combo);

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  tree_path = gtk_tree_path_new_from_indices (active, -1);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (liststore_services),
                           &iter,
                           tree_path);

  gtk_tree_path_free (tree_path);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services), &iter, COLUMN_GDBUSPROXY, &service, -1);

  active = gtk_combo_box_get_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_ipv4_method")));

  if (active == 3) { /* fixed */
    gis_show_error (page, _("Invalid Method"), _("User cannot set this method"));
    return;
  }

  if (active == 0) /* Off */
    g_variant_builder_add(ipv4conf,"{sv}", "Method", g_variant_new_string ("off"));
  else if (active == 1) /* DHCP/Automatic */
    g_variant_builder_add (ipv4conf,"{sv}", "Method", g_variant_new_string ("dhcp"));
  else if (active == 2) { /* Manual */
    g_variant_builder_add (ipv4conf,"{sv}", "Method", g_variant_new_string ("manual"));

    str = (gchar *) gtk_entry_get_text (GTK_ENTRY (WID (priv->builder, "entry_ipv4_address")));
    if (str) {
      g_variant_builder_add (ipv4conf,"{sv}", "Address", g_variant_new_string (str));
    }

    str = (gchar *) gtk_entry_get_text (GTK_ENTRY (WID (priv->builder, "entry_ipv4_netmask")));
    if (str) {
      g_variant_builder_add (ipv4conf,"{sv}", "Netmask", g_variant_new_string (str));
    }

    str = (gchar *) gtk_entry_get_text (GTK_ENTRY (WID (priv->builder, "entry_ipv4_gateway")));
    if (str) {
      g_variant_builder_add (ipv4conf,"{sv}", "Gateway", g_variant_new_string (str));
    }
  }

  value = g_variant_builder_end(ipv4conf);

  service_call_set_property_sync (service,
                                  "IPv4.Configuration",
                                  g_variant_new_variant (value),
                                  priv->cancellable,
                                  &error);

  if (error) {
    gis_show_error (page, _("Unable to save IPv4 Configuration"), error->message);
    g_error_free (error);
  } else
    gis_show_sugisess (page, _("IPv4 Settings"), _("IPv4 settings saved sugisesfully"));
}

static void
gis_setup_settings_ipv4 (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  gint active;
  GtkComboBox *combo;

  GtkListStore *liststore_services;
  GtkTreePath *tree_path;
  GtkTreeIter iter;

  gboolean ret;
  GVariant *ipv4;
  gchar *method;
  gchar *address;
  gchar *netmask;
  gchar *gateway;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));
  active = gtk_combo_box_get_active (combo);

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  tree_path = gtk_tree_path_new_from_indices (active, -1);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (liststore_services),
                           &iter,
                           tree_path);

  gtk_tree_path_free (tree_path);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services),
                      &iter,
                      COLUMN_IPv4, &ipv4,
                      -1);

  ret = g_variant_lookup (ipv4, "Method", "s", &method);
  if (!ret)
    method = "off";

  /* Set the method combobox */

  if (!g_strcmp0 (method, "off"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_ipv4_method")), 0);
  else  if (!g_strcmp0 (method, "dhcp"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_ipv4_method")), 1);
  else  if (!g_strcmp0 (method, "manual"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_ipv4_method")), 2);
  else
    gtk_combo_box_set_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_ipv4_method")), 3);

  ret = g_variant_lookup (ipv4, "Address", "s", &address);
  if (ret)
    gtk_entry_set_text (GTK_ENTRY (WID (priv->builder, "entry_ipv4_address")), address);

  ret = g_variant_lookup (ipv4, "Netmask", "s", &netmask);
  if (ret)
    gtk_entry_set_text (GTK_ENTRY (WID (priv->builder, "entry_ipv4_netmask")), netmask);

  ret = g_variant_lookup (ipv4, "Gateway", "s", &gateway);
  if (ret)
    gtk_entry_set_text (GTK_ENTRY (WID (priv->builder, "entry_ipv4_gateway")), gateway);

  gis_setup_settings_ipv4_widgets (page, method);

  if (!priv->settings_ipv4_save_id) {
    priv->settings_ipv4_save_id = g_signal_connect (GTK_BUTTON (WID (priv->builder, "button_ipv4_save")),
                                                    "clicked",
                                                    G_CALLBACK (gis_button_ipv4_save_cb),
                                                    page);
  }

  if (!priv->settings_ipv4_method_id) {
    priv->settings_ipv4_method_id = g_signal_connect (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_ipv4_method")),
                                                      "changed",
                                                      G_CALLBACK (gis_ipv4_method_changed),
                                                      page);
  }
}

static void
gis_setup_settings_ipv6_widgets (GisNetworkPage *page, gchar *method)
{
  GisNetworkPagePrivate *priv = page->priv;

  if (!g_strcmp0 (method, "off")) {
    gtk_widget_hide (GTK_WIDGET (WID (priv->builder, "entry_ipv6_address_box")));
    gtk_widget_hide (GTK_WIDGET (WID (priv->builder, "entry_ipv6_privacy_box")));
    gtk_widget_hide (GTK_WIDGET (WID (priv->builder, "entry_ipv6_gateway_box")));
  } else {
    gtk_widget_show (GTK_WIDGET (WID (priv->builder, "entry_ipv6_address_box")));
    gtk_widget_show (GTK_WIDGET (WID (priv->builder, "entry_ipv6_privacy_box")));
    gtk_widget_show (GTK_WIDGET (WID (priv->builder, "entry_ipv6_gateway_box")));
  }

  if (!g_strcmp0 (method, "manual")) {
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "entry_ipv6_address")), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "entry_ipv6_prefix")), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "entry_ipv6_gateway")), TRUE);
  } else {
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "entry_ipv6_address")), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "entry_ipv6_prefix")), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "entry_ipv6_gateway")), FALSE);
  }

  if (!g_strcmp0 (method, "auto"))
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "comboboxtext_ipv6_privacy")), TRUE);
  else
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "comboboxtext_ipv6_privacy")), FALSE);
}

static void
gis_ipv6_method_changed (GtkComboBox *combo, gpointer user_data)
{
  GisNetworkPage *page = user_data;
  gint active;

  active = gtk_combo_box_get_active (combo);

  if (active == 0)
    gis_setup_settings_ipv6_widgets (page, "off");
  else if (active == 1)
    gis_setup_settings_ipv6_widgets (page, "auto");
  else if (active == 2)
    gis_setup_settings_ipv6_widgets (page, "manual");
  else if (active == 3)
    gis_setup_settings_ipv6_widgets (page, "fixed");
  else
    gis_setup_settings_ipv6_widgets (page, "6to4");
}

static void
gis_button_ipv6_save_cb (GtkButton *button, gpointer user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  gint active, active2;
  GtkComboBox *combo;
  GError *error = NULL;

  GtkListStore *liststore_services;
  GtkTreePath *tree_path;
  GtkTreeIter iter;

  Service *service;
  GVariantBuilder *ipv6conf = g_variant_builder_new (G_VARIANT_TYPE_DICTIONARY);
  GVariant *value;
  gchar *str;
  guint8 prefix_length;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));
  active = gtk_combo_box_get_active (combo);

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  tree_path = gtk_tree_path_new_from_indices (active, -1);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (liststore_services),
                           &iter,
                           tree_path);

  gtk_tree_path_free (tree_path);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services), &iter, COLUMN_GDBUSPROXY, &service, -1);

  active = gtk_combo_box_get_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_ipv6_method")));

  if (active == 3 ||active == 4) { /* fixed or 6to4*/
    gis_show_error (page, _("Invalid Method"), _("User cannot set this method"));
    return;
  }

  if (active == 0) /* Off */
    g_variant_builder_add(ipv6conf,"{sv}", "Method", g_variant_new_string ("off"));

  else if (active == 1) { /* Automatic */
    g_variant_builder_add (ipv6conf,"{sv}", "Method", g_variant_new_string ("auto"));

    active2 = gtk_combo_box_get_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_ipv6_privacy")));
    if (active2 == 1)
      g_variant_builder_add (ipv6conf,"{sv}", "Privacy", g_variant_new_string ("enabled"));
    else if (active2 == 2)
      g_variant_builder_add (ipv6conf,"{sv}", "Privacy", g_variant_new_string ("prefered"));
    else
      g_variant_builder_add (ipv6conf,"{sv}", "Privacy", g_variant_new_string ("disabled"));

  } else if (active == 2) { /* Manual */
    g_variant_builder_add (ipv6conf,"{sv}", "Method", g_variant_new_string ("manual"));

    str = (gchar *) gtk_entry_get_text (GTK_ENTRY (WID (priv->builder, "entry_ipv6_address")));
    if (str) {
      g_variant_builder_add (ipv6conf,"{sv}", "Address", g_variant_new_string (str));
    }

    str = (gchar *) gtk_entry_get_text (GTK_ENTRY (WID (priv->builder, "entry_ipv6_prefix")));
    if (str) {
      prefix_length = (guint8) atoi (str);
      g_variant_builder_add (ipv6conf,"{sv}", "PrefixLength", g_variant_new_byte (prefix_length));
    }

    str = (gchar *) gtk_entry_get_text (GTK_ENTRY (WID (priv->builder, "entry_ipv6_gateway")));
    if (str) {
      g_variant_builder_add (ipv6conf,"{sv}", "Gateway", g_variant_new_string (str));
    }
  }
  
  value = g_variant_builder_end(ipv6conf);

  service_call_set_property_sync (service,
                                  "IPv6.Configuration",
                                  g_variant_new_variant (value),
                                  priv->cancellable,
                                  &error);

  if (error) {
    gis_show_error (page, _("Unable to save IPv6 Configuration"), error->message);
    g_error_free (error);
  } else {
    gis_show_sugisess (page, _("IPv6 Settings"), _("IPv6 settings saved sugisesfully"));
  }
}

static void
gis_setup_settings_ipv6 (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  gint active;
  GtkComboBox *combo;

  GtkListStore *liststore_services;
  GtkTreePath *tree_path;
  GtkTreeIter iter;

  gboolean ret;
  GVariant *ipv6;
  gchar *method;
  gchar *privacy;
  gchar *address;
  guint8 prefix;
  gchar *gateway;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));
  active = gtk_combo_box_get_active (combo);

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  tree_path = gtk_tree_path_new_from_indices (active, -1);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (liststore_services),
                           &iter,
                           tree_path);

  gtk_tree_path_free (tree_path);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services),
                      &iter,
                      COLUMN_IPv6, &ipv6,
                      -1);

  ret = g_variant_lookup (ipv6, "Method", "s", &method);
  if (!ret)
    method = "off";

  gis_setup_settings_ipv6_widgets (page, method);

  /* Set the method combobox */

  if (!g_strcmp0 (method, "off"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_ipv6_method")), 0);
  else  if (!g_strcmp0 (method, "auto")) {
    gtk_combo_box_set_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_ipv6_method")), 1);

    ret = g_variant_lookup (ipv6, "Privacy", "s", &privacy);
    if (!ret)
      privacy = "disabled";

    if (!g_strcmp0 (privacy , "enabled"))
      gtk_combo_box_set_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_ipv6_privacy")), 0);
    else if (!g_strcmp0 (privacy , "prefered"))
      gtk_combo_box_set_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_ipv6_privacy")), 2);
    else
      gtk_combo_box_set_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_ipv6_privacy")), 1);

  } else if (!g_strcmp0 (method, "manual"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_ipv6_method")), 2);
  else if (!g_strcmp0 (method, "fixed"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_ipv6_method")), 3);
  else
    gtk_combo_box_set_active (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_ipv6_method")), 4);

  ret = g_variant_lookup (ipv6, "Address", "s", &address);
  if (ret)
    gtk_entry_set_text (GTK_ENTRY (WID (priv->builder, "entry_ipv6_address")), address);

  ret = g_variant_lookup (ipv6, "PrefixLength", "y", &prefix);
  if (ret)
    gtk_entry_set_text (GTK_ENTRY (WID (priv->builder, "entry_ipv6_prefix")), g_strdup_printf ("%i", prefix));

  ret = g_variant_lookup (ipv6, "Gateway", "s", &gateway);
  if (ret)
    gtk_entry_set_text (GTK_ENTRY (WID (priv->builder, "entry_ipv6_gateway")), gateway);

  if (!priv->settings_ipv6_save_id) {
    priv->settings_ipv6_save_id = g_signal_connect (GTK_BUTTON (WID (priv->builder, "button_ipv6_save")),
                                                    "clicked",
                                                    G_CALLBACK (gis_button_ipv6_save_cb),
                                                    page);
  }

  if (!priv->settings_ipv6_method_id) {
    priv->settings_ipv6_method_id = g_signal_connect (GTK_COMBO_BOX (WID (priv->builder, "comboboxtext_ipv6_method")),
                                                      "changed",
                                                      G_CALLBACK (gis_ipv6_method_changed),
                                                      page);
  }
}

static void
gis_button_nameservers_save_cb (GtkButton *button, gpointer user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  gint active;
  GtkComboBox *combo;
  GError *error = NULL;

  GtkListStore *liststore_services;
  GtkTreePath *tree_path;
  GtkTreeIter iter;

  Service *service;
  GVariant *value;
  gchar *str;
  gchar **nameservers = NULL;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));
  active = gtk_combo_box_get_active (combo);

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  tree_path = gtk_tree_path_new_from_indices (active, -1);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (liststore_services),
                           &iter,
                           tree_path);

  gtk_tree_path_free (tree_path);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services), &iter, COLUMN_GDBUSPROXY, &service, -1);

  str = (gchar *) gtk_entry_get_text (GTK_ENTRY (WID (priv->builder, "entry_nameservers")));
  if (str) {
    nameservers = g_strsplit (str, ",", -1);

    value = g_variant_new_strv ((const gchar * const *) nameservers, -1);

    service_call_set_property_sync (service,
                                    "Nameservers.Configuration",
                                    g_variant_new_variant (value),
                                    priv->cancellable,
                                    &error);
    if (error) {
      gis_show_error (page, _("Unable to save Nameserver Configuration"), error->message);
      g_error_free (error);
    } else {
      gis_show_sugisess (page, _("Nameserver Settings"), _("Nameservers saved sugisesfully"));
    }
}
}

static void
gis_setup_settings_nameservers (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  gint active;
  GtkComboBox *combo;

  GtkListStore *liststore_services;
  GtkTreePath *tree_path;
  GtkTreeIter iter;

  GVariant *nameservers;
  const gchar **ns;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));
  active = gtk_combo_box_get_active (combo);

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  tree_path = gtk_tree_path_new_from_indices (active, -1);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (liststore_services),
                           &iter,
                           tree_path);

  gtk_tree_path_free (tree_path);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services),
                      &iter,
                      COLUMN_NAMESERVERS, &nameservers,
                      -1);

  ns = g_variant_get_strv (nameservers, NULL);

  if (ns != NULL)
    gtk_entry_set_text (GTK_ENTRY (WID (priv->builder, "entry_nameservers")), g_strjoinv (",", (gchar **) ns));

  if (!priv->settings_nameservers_save_id) {
    priv->settings_nameservers_save_id = g_signal_connect (GTK_BUTTON (WID (priv->builder, "button_nameservers_save")),
                                                           "clicked",
                                                           G_CALLBACK (gis_button_nameservers_save_cb),
                                                           page);
  }
}

static void
gis_button_configure_pressed (GtkToggleButton *button, gpointer user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  GtkNotebook *settings;

  gboolean active;

  settings = GTK_NOTEBOOK (WID (priv->builder, "notebook_settings"));

  active = gtk_toggle_button_get_active (button);

  if (active) {
    gis_setup_settings_details (page);
    gis_setup_settings_proxy (page);
    gis_setup_settings_domains (page);
    gis_setup_settings_ipv4 (page);
    gis_setup_settings_ipv6 (page);
    gis_setup_settings_nameservers(page);

    gtk_widget_show_all (GTK_WIDGET (settings));
  } else {
    gtk_widget_hide (GTK_WIDGET (settings));
  }
}

static void
gis_combo_changed_cb (GtkComboBox *combo, gpointer user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  GtkButton *refresh;
  GtkNotebook *settings;
  GtkButton *connect;
  GtkButton *forget;
  GtkToggleButton *configure;

  GtkListStore *liststore_services;
  GtkTreeIter iter;

  gint active;
  const gchar *state;
  gboolean favorite;

  active = gtk_combo_box_get_active (combo);
  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  if (active == -1) {
    return;
  }

  settings = GTK_NOTEBOOK (WID (priv->builder, "notebook_settings"));

  /* The user has selected a service if active is not -1 */

  if (priv->services_changed_id > 0) {
    g_signal_handler_disconnect (priv->manager, priv->services_changed_id);

    priv->services_changed_id = -1;
  }

  refresh = GTK_BUTTON (WID (priv->builder, "button_refresh"));
  gtk_widget_set_sensitive (GTK_WIDGET (refresh), TRUE);

  if (!priv->refresh_id) {
    priv->refresh_id = g_signal_connect (GTK_BUTTON (refresh), "clicked",
                                         G_CALLBACK (gis_button_refresh_pressed),
                                         page);
  }

  connect = GTK_BUTTON (WID (priv->builder, "button_connect"));
  gtk_widget_set_sensitive (GTK_WIDGET (connect), TRUE);

  forget = GTK_BUTTON (WID (priv->builder, "button_forget"));
  gtk_widget_set_sensitive (GTK_WIDGET (forget), FALSE);

  configure = GTK_TOGGLE_BUTTON (WID (priv->builder, "togglebutton_configure"));
  gtk_widget_set_sensitive (GTK_WIDGET (configure), TRUE);
  gtk_toggle_button_set_active (configure, FALSE);
  /* Hide the notebook */
  gtk_widget_hide (GTK_WIDGET (settings));

  if (!priv->connect_id) {
    priv->connect_id = g_signal_connect (GTK_BUTTON (connect), "clicked",
                                         G_CALLBACK (gis_button_connect_pressed),
                                         page);
  }

  if (!priv->forget_id) {
    priv->forget_id = g_signal_connect (GTK_BUTTON (forget), "clicked",
                                         G_CALLBACK (gis_button_forget_pressed),
                                         page);
  }

  if (!priv->configure_id) {
    priv->configure_id = g_signal_connect (GTK_TOGGLE_BUTTON (configure), "toggled",
                                         G_CALLBACK (gis_button_configure_pressed),
                                         page);
  }

  gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (liststore_services), &iter, NULL, active);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services), &iter, COLUMN_STATE, &state, -1);
  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services), &iter, COLUMN_FAVORITE, &favorite, -1);

  if ((g_strcmp0 (state, "ready") == 0) || (g_strcmp0 (state, "online") == 0))
    gtk_button_set_label (connect, "Disconnect");
  else
    gtk_button_set_label (connect, "Connect");

  if (favorite)
    gtk_widget_set_sensitive (GTK_WIDGET (forget), TRUE);
}

static void
gis_combo_popdown_cb (GtkComboBox *combo, gboolean test, gpointer user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  GtkListStore *liststore_services;
  GtkTreeIter iter;

  liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

  if (priv->popup == TRUE) {
    priv->popup = FALSE;

    g_timeout_add (100, (GSourceFunc) gis_check_activeitem_set, page);
    return;
  }

  priv->popup = TRUE;

  if (priv->label_set) {
    gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (liststore_services), &iter, NULL, 0);
    gtk_list_store_remove (liststore_services, &iter);

    priv->label_set = FALSE;

    priv->combo_changed_id = g_signal_connect (combo,
                                               "changed",
                                               G_CALLBACK (gis_combo_changed_cb),
                                               page);
  }
}

static void
gis_setup_service_list(GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  GtkCellRenderer *renderer;
  GtkComboBox *combo;

  combo = GTK_COMBO_BOX (WID (priv->builder, "combobox1"));

  /* Icon */
  renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (renderer, "stock-size", GTK_ICON_SIZE_MENU, NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer, "stock-id", COLUMN_ICON, NULL);

  /* Name */
  renderer = gtk_cell_renderer_text_new();
  gtk_cell_renderer_set_padding (renderer, 4, 4);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer, "text", COLUMN_NAME, NULL);

  /* Strength */
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_renderer_set_padding (renderer, 4, 4);
  gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (combo), renderer, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer, "icon-name", COLUMN_STRENGTH_ICON, NULL);

  /* Security */
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_renderer_set_padding (renderer, 4, 4);
  gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (combo), renderer, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer, "icon-name", COLUMN_SECURITY_ICON, NULL);

  /* Favorite */
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_renderer_set_padding (renderer, 8, 8);
  gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (combo), renderer, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer, "icon-name", COLUMN_FAVORITE_ICON, NULL);

  /* State */
  renderer = gtk_cell_renderer_text_new();
  gtk_cell_renderer_set_alignment (renderer, 0.5, 0.5);
  gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (combo), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer, "text", COLUMN_STATE, NULL);

  g_signal_connect (combo,
                    "notify::popup-shown",
                    G_CALLBACK (gis_combo_popdown_cb),
                    page);

  manager_call_get_services (priv->manager, priv->cancellable, manager_get_services, page);
}

/* Tether Section */

static void
gis_button_tether_enabled_pressed (GtkButton *button, gpointer user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  GError *error = NULL;

  GtkWidget *dialog;
  gboolean tether_bt;
  gchar *ssid = NULL;
  gchar *psk = NULL;

  dialog = GTK_WIDGET (WID (priv->builder, "tether_dialog"));
  gtk_widget_hide (dialog);

  tether_bt = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (WID (priv->builder, "checkbutton_tether_bt")));


  ssid = (gchar *) gtk_entry_get_text (GTK_ENTRY (WID (priv->builder, "entry_ssid")));

  psk = (gchar *) gtk_entry_get_text (GTK_ENTRY (WID (priv->builder, "entry_passphrase")));

  /* Tether BT */
  if (priv->bluetooth && tether_bt) {
    /* If BT is available but turned off, enable it */
    if (!gtk_switch_get_active (GTK_SWITCH (priv->bluetooth_switch))) {
      technology_call_set_property_sync (priv->bluetooth,
                                         "Powered",
                                         g_variant_new_variant (g_variant_new_boolean (TRUE)),
                                         priv->cancellable,
                                         &error);
      if (error) {
        g_printerr ("Unable to turn on Bluetooth:%s", error->message);
        g_error_free (error);
        return;
      }
    }

    technology_call_set_property_sync (priv->bluetooth,
                                       "Tethering",
                                       g_variant_new_variant (g_variant_new_boolean (TRUE)),
                                       priv->cancellable,
                                       &error);
    if (error) {
      g_printerr ("Unable to tether Bluetooth:%s", error->message);
      g_error_free (error);
      return;
    }
  }

  technology_call_set_property_sync (priv->wifi,
                                     "TetheringIdentifier",
                                     g_variant_new_variant (g_variant_new_string (ssid)),
                                     priv->cancellable,
                                     &error);
  if (error) {
    g_printerr ("Unable to set Tether SSID:%s", error->message);
    g_error_free (error);
    return;
  }

  technology_call_set_property_sync (priv->wifi,
                                       "TetheringPassphrase",
                                       g_variant_new_variant (g_variant_new_string (psk)),
                                       priv->cancellable,
                                       &error);

  if (error) {
    g_printerr ("Unable to set Tether Passphrase:%s", error->message);
    g_error_free (error);
    return;
  }


  technology_call_set_property_sync (priv->wifi,
                                     "Tethering",
                                     g_variant_new_variant (g_variant_new_boolean (TRUE)),
                                     priv->cancellable,
                                     &error);
  if (error) {
    g_printerr ("Unable to Tether WiFi:%s", error->message);
    g_error_free (error);
    return;
  }

gtk_button_set_label (GTK_BUTTON (WID (priv->builder, "button_create_hotspot")), "Stop HotSpot");
}

static void
gis_button_tether_cancel_pressed (GtkButton *button, gpointer user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  GtkWidget *dialog;

  dialog = GTK_WIDGET (WID (priv->builder, "tether_dialog"));
  gtk_widget_hide (dialog);
}

static void
gis_tether_text_entered (GtkEntry *entry,
                        gchar    *string,
                        gpointer  user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;

  if (gtk_entry_get_text_length (GTK_ENTRY (WID (priv->builder, "entry_ssid"))) == 0 ||
      gtk_entry_get_text_length (GTK_ENTRY (WID (priv->builder, "entry_passphrase"))) < 8)
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "button_tether")), FALSE);
  else
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "button_tether")), TRUE);
}

static void
gis_button_tether_pressed (GtkButton *button, gpointer user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  GtkWidget *dialog;
  GError *error = NULL;

  if (!g_strcmp0 (gtk_button_get_label (button), "Stop HotSpot")) {

    if (!priv->tethering_wifi)
      return;

    if (priv->tethering_bt) {
      technology_call_set_property_sync (priv->bluetooth,
                                       "Tethering",
                                       g_variant_new_variant (g_variant_new_boolean (FALSE)),
                                       priv->cancellable,
                                       &error);
      if (error) {
        g_printerr ("Unable to Tether off Bluetooth:%s", error->message);
        g_error_free (error);
        return;
      }
    }

    technology_call_set_property_sync (priv->wifi,
                                       "Tethering",
                                       g_variant_new_variant (g_variant_new_boolean (FALSE)),
                                       priv->cancellable,
                                       &error);
    if (error) {
      g_printerr ("Unable to Tether off WiFi:%s", error->message);
      g_error_free (error);
      return;
    }

    gtk_button_set_label (GTK_BUTTON (WID (priv->builder, "button_create_hotspot")), "Create a HotSpot");
    return;
  }

  dialog = GTK_WIDGET (WID (priv->builder, "tether_dialog"));

  if (!priv->tether_enable_id) {
    priv->tether_enable_id = g_signal_connect (GTK_BUTTON ( WID (priv->builder, "button_tether")), "clicked",
                                         G_CALLBACK (gis_button_tether_enabled_pressed),
                                         page);
  }

  if (!priv->tether_cancel_id) {
    priv->tether_cancel_id = g_signal_connect (GTK_BUTTON ( WID (priv->builder, "button_tether_cancel")), "clicked",
                                         G_CALLBACK (gis_button_tether_cancel_pressed),
                                         page);
  }

  if (!priv->tether_ssid_id) {
    priv->tether_ssid_id = g_signal_connect (GTK_ENTRY ( WID (priv->builder, "entry_ssid")), "notify::text-length",
                                         G_CALLBACK (gis_tether_text_entered),
                                         page);
  }

  if (!priv->tether_psk_id) {
    priv->tether_psk_id = g_signal_connect (GTK_ENTRY ( WID (priv->builder, "entry_passphrase")), "notify::text-length",
                                         G_CALLBACK (gis_tether_text_entered),
                                         page);
  }

  /* Setup the details of the dialog */

if (priv->tethering_ssid)
    gtk_entry_set_text (GTK_ENTRY (WID (priv->builder, "entry_ssid")), priv->tethering_ssid);
 if (priv->tethering_psk)
    gtk_entry_set_text (GTK_ENTRY (WID (priv->builder, "entry_passphrase")), priv->tethering_psk);

  if (gtk_entry_get_text_length (GTK_ENTRY (WID (priv->builder, "entry_ssid"))) == 0)
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "button_tether")), FALSE);
  else
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "button_tether")), TRUE);

  if (priv->bluetooth)
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "checkbutton_tether_bt")), TRUE);
  else
    gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "checkbutton_tether_bt")), FALSE);

  gtk_widget_show (dialog);
}

/* Ethernet Switch Section */

static void
ethernet_property_changed (Technology *ethernet,
                          const gchar *property,
                          GVariant *value,
                          GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  gboolean powered;

  if (!g_strcmp0 (property, "Powered")) {
    powered  = g_variant_get_boolean (g_variant_get_variant (value));

    if (priv->ethernet_switch)
      gtk_switch_set_active (GTK_SWITCH (priv->ethernet_switch), powered);
  }
}

static void
ethernet_set_powered (GObject      *source,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  gboolean powered;
  GError *error = NULL;
  gint err_code;

  if (!technology_call_set_property_finish (priv->ethernet, res, &error)) {
    err_code = error->code;
    /* Reset the switch if error is not AlreadyEnabled/AlreadyDisabled */
    if (err_code != 36) { /* Not AlreadyEnabled */
      powered = gtk_switch_get_active (GTK_SWITCH (priv->ethernet_switch));
      gtk_switch_set_active (GTK_SWITCH (priv->ethernet_switch), !powered);
    } else
      g_warning ("Could not set ethernet Powered property: %s", error->message);

    g_error_free (error);
    return;
  }
}

static void
gis_ethernet_switch_toggle (GtkSwitch *sw,
                       GParamSpec *pspec,
                       GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  gboolean powered;
  GVariant *value;

  if (priv->ethernet == NULL)
    return;

  powered = gtk_switch_get_active (sw);
  value = g_variant_new_boolean (powered);

  technology_call_set_property (priv->ethernet,
                             "Powered",
                             g_variant_new_variant (value),
                             priv->cancellable,
                             ethernet_set_powered,
                             page);
}

static void
gis_add_technology_ethernet (const gchar         *path,
                            GVariant            *properties,
                            GisNetworkPage      *page)
{
  GisNetworkPagePrivate *priv = page->priv;

  GError *error = NULL;
  GVariant *value = NULL;
  gboolean powered;

  if (priv->ethernet == NULL) {
    priv->ethernet = technology_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                        G_DBUS_PROXY_FLAGS_NONE,
                                                        "net.connman",
                                                        path,
                                                        priv->cancellable,
                                                        &error);

    if (error != NULL) {
      g_warning ("Could not get proxy for ethernet: %s", error->message);
      g_error_free (error);
      return;
    }

    priv->ethernet_switch = WID(priv->builder, "ethernet_switch");
    gtk_widget_set_sensitive (priv->ethernet_switch, TRUE);
    gtk_widget_set_sensitive (WID(priv->builder, "image_ethernet"), TRUE);
    gtk_widget_set_sensitive (WID(priv->builder, "label_ethernet"), TRUE);

    g_signal_connect (priv->ethernet,
                      "property_changed",
                      G_CALLBACK (ethernet_property_changed),
                      page);

    g_signal_connect (GTK_SWITCH (priv->ethernet_switch), "notify::active",
                      G_CALLBACK (gis_ethernet_switch_toggle),
                      page);

    priv->tech_update = TRUE;
  }

  value = g_variant_lookup_value (properties, "Powered", G_VARIANT_TYPE_BOOLEAN);
  powered = g_variant_get_boolean (value);

  if (priv->ethernet_switch)
    gtk_switch_set_active (GTK_SWITCH (priv->ethernet_switch), powered);
}

static void
gis_remove_technology_ethernet (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;

  if (priv->ethernet == NULL) {
    g_warning ("Unable to remove ethernet technology");
    return;
  }

  g_object_unref (priv->ethernet);
  priv->ethernet = NULL;

  gtk_switch_set_active (GTK_SWITCH (priv->ethernet_switch), FALSE);

  gtk_widget_set_sensitive (priv->ethernet_switch, FALSE);
  gtk_widget_set_sensitive (WID(priv->builder, "image_ethernet"), FALSE);
  gtk_widget_set_sensitive (WID(priv->builder, "label_ethernet"), FALSE);

}

/* Wifi Switch Section */

static void
wifi_property_changed (Technology *wifi,
                          const gchar *property,
                          GVariant *value,
                          GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  gboolean powered;

  if (!g_strcmp0 (property, "Powered")) {
    powered  = g_variant_get_boolean (g_variant_get_variant (value));

    if (priv->wifi_switch)
      gtk_switch_set_active (GTK_SWITCH (priv->wifi_switch), powered);
  }

  if (!g_strcmp0 (property, "Tethering")) {
    priv->tethering_wifi = g_variant_get_boolean (g_variant_get_variant (value));
  }

  if (!g_strcmp0 (property, "TetheringIdentifier")) {
    priv->tethering_ssid = (gchar *) g_variant_get_string (g_variant_get_variant (value), NULL);
  }

  if (!g_strcmp0 (property, "TetheringPassphrase")) {
    priv->tethering_psk = (gchar *) g_variant_get_string (g_variant_get_variant (value), NULL);
  }
}

static void
wifi_set_powered (GObject      *source,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  gboolean powered;
  GError *error = NULL;
  gint err_code;

  if (!technology_call_set_property_finish (priv->wifi, res, &error)) {
    err_code = error->code;
    /* Reset the switch if error is not AlreadyEnabled/AlreadyDisabled */
    if (err_code != 36) { /* Not AlreadyEnabled */
      powered = gtk_switch_get_active (GTK_SWITCH (priv->wifi_switch));
      gtk_switch_set_active (GTK_SWITCH (priv->wifi_switch), !powered);
    } else
      g_warning ("Could not set wifi Powered property: %s", error->message);

    g_error_free (error);

    return;
  }
}

static void
gis_wifi_switch_toggle (GtkSwitch *sw,
                       GParamSpec *pspec,
                       GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  gboolean powered;
  GVariant *value;

  if (priv->wifi == NULL)
    return;

  powered = gtk_switch_get_active (sw);
  value = g_variant_new_boolean (powered);

  technology_call_set_property (priv->wifi,
                             "Powered",
                             g_variant_new_variant (value),
                             priv->cancellable,
                             wifi_set_powered,
                             page);
}

static void
gis_add_technology_wifi (const gchar          *path,
                       GVariant              *properties,
                       GisNetworkPage        *page)
{
  GisNetworkPagePrivate *priv = page->priv;

  GError *error = NULL;
  GVariant *value = NULL;
  gboolean powered;

  if (priv->wifi == NULL) {
    priv->wifi = technology_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                        G_DBUS_PROXY_FLAGS_NONE,
                                                        "net.connman",
                                                        path,
                                                        priv->cancellable,
                                                        &error);

    if (error != NULL) {
      g_warning ("Could not get proxy for wifi: %s", error->message);
      g_error_free (error);
      return;
    }
    priv->wifi_switch = WID(priv->builder, "wifi_switch");
    gtk_widget_set_sensitive (priv->wifi_switch, TRUE);
    gtk_widget_set_sensitive (WID(priv->builder, "image_wifi"), TRUE);
    gtk_widget_set_sensitive (WID(priv->builder, "label_wifi"), TRUE);

    g_signal_connect (priv->wifi,
                      "property_changed",
                      G_CALLBACK (wifi_property_changed),
                      page);

    g_signal_connect (GTK_SWITCH (priv->wifi_switch), "notify::active",
                      G_CALLBACK (gis_wifi_switch_toggle),
                      page);

    priv->tech_update = TRUE;
  }

  value = g_variant_lookup_value (properties, "Powered", G_VARIANT_TYPE_BOOLEAN);
  powered = g_variant_get_boolean (value);

  value = g_variant_lookup_value (properties, "Tethering", G_VARIANT_TYPE_BOOLEAN);
  priv->tethering_wifi = g_variant_get_boolean (value);

  g_variant_lookup (properties, "TetheringIdentifier", "s", &priv->tethering_ssid);
  g_variant_lookup (properties, "TetheringPassphrase", "s", &priv->tethering_psk);

  if (priv->wifi_switch)
    gtk_switch_set_active (GTK_SWITCH (priv->wifi_switch), powered);

  if (!priv->tether_id) {
    priv->tether_id = g_signal_connect (GTK_BUTTON (WID (priv->builder, "button_create_hotspot")), "clicked",
                                         G_CALLBACK (gis_button_tether_pressed),
                                         page);
  }

  gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "button_create_hotspot")), TRUE);
}

static void
gis_remove_technology_wifi (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;

  if (priv->wifi == NULL) {
    g_warning ("Unable to remove wifi technology");
    return;
  }

  g_object_unref (priv->wifi);
  priv->wifi = NULL;

  gtk_switch_set_active (GTK_SWITCH (priv->wifi_switch), FALSE);

  gtk_widget_set_sensitive (priv->wifi_switch, FALSE);
  gtk_widget_set_sensitive (WID(priv->builder, "image_wifi"), FALSE);
  gtk_widget_set_sensitive (WID(priv->builder, "label_wifi"), FALSE);

  gtk_widget_set_sensitive (GTK_WIDGET (WID (priv->builder, "button_create_hotspot")), FALSE);
}

/* Bluetooth Switch Section */

static void
bluetooth_property_changed (Technology *bluetooth,
                          const gchar *property,
                          GVariant *value,
                          GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  gboolean powered;

  if (!g_strcmp0 (property, "Powered")) {
    powered  = g_variant_get_boolean (g_variant_get_variant (value));

    if (priv->bluetooth_switch)
      gtk_switch_set_active (GTK_SWITCH (priv->bluetooth_switch), powered);
  }

  if (!g_strcmp0 (property, "Tethering")) {
    priv->tethering_bt = g_variant_get_boolean (g_variant_get_variant (value));
  }
}

static void
bluetooth_set_powered (GObject      *source,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  gboolean powered;
  GError *error = NULL;
  gint err_code;

  if (priv->bluetooth == NULL)
    return;

  if (!technology_call_set_property_finish (priv->bluetooth, res, &error)) {
    err_code = error->code;
    /* Reset the switch if error is not AlreadyEnabled/AlreadyDisabled */
    if (err_code != 36) { /* Not AlreadyEnabled */
      powered = gtk_switch_get_active (GTK_SWITCH (priv->bluetooth_switch));
      gtk_switch_set_active (GTK_SWITCH (priv->bluetooth_switch), !powered);
    } else
      g_warning ("Could not set bluetooth Powered property: %s", error->message);

    g_error_free (error);

    return;
  }
}

static void
gis_bluetooth_switch_toggle (GtkSwitch *sw,
                       GParamSpec *pspec,
                       GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  gboolean powered;
  GVariant *value;

  if (priv->bluetooth == NULL)
    return;

  powered = gtk_switch_get_active (sw);
  value = g_variant_new_boolean (powered);

  technology_call_set_property (priv->bluetooth,
                             "Powered",
                             g_variant_new_variant (value),
                             priv->cancellable,
                             bluetooth_set_powered,
                             page);
}

static void
gis_add_technology_bluetooth (const gchar        *path,
                           GVariant             *properties,
                           GisNetworkPage       *page)
{
  GisNetworkPagePrivate *priv = page->priv;

  GError *error = NULL;
  GVariant *value = NULL;
  gboolean powered;

  if (priv->bluetooth == NULL) {
    priv->bluetooth = technology_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                        G_DBUS_PROXY_FLAGS_NONE,
                                                        "net.connman",
                                                        path,
                                                        priv->cancellable,
                                                        &error);

    if (error != NULL) {
      g_warning ("Could not get proxy for bluetooth: %s", error->message);
      g_error_free (error);
      return;
    }

    priv->bluetooth_switch = WID(priv->builder, "bluetooth_switch");
    gtk_widget_set_sensitive (priv->bluetooth_switch, TRUE);
    gtk_widget_set_sensitive (WID(priv->builder, "image_bluetooth"), TRUE);
    gtk_widget_set_sensitive (WID(priv->builder, "label_bluetooth"), TRUE);

    g_signal_connect (priv->bluetooth,
                      "property_changed",
                      G_CALLBACK (bluetooth_property_changed),
                      page);

    g_signal_connect (GTK_SWITCH (priv->bluetooth_switch), "notify::active",
                      G_CALLBACK (gis_bluetooth_switch_toggle),
                      page);

    priv->tech_update = TRUE;
  }

  value = g_variant_lookup_value (properties, "Powered", G_VARIANT_TYPE_BOOLEAN);
  powered = g_variant_get_boolean (value);

  value = g_variant_lookup_value (properties, "Tethering", G_VARIANT_TYPE_BOOLEAN);
  priv->tethering_bt = g_variant_get_boolean (value);

  if (priv->bluetooth_switch)
    gtk_switch_set_active (GTK_SWITCH (priv->bluetooth_switch), powered);
}

static void
gis_remove_technology_bluetooth (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;

  if (priv->bluetooth == NULL) {
    g_warning ("Unable to remove bluetooth technology");
    return;
  }

  g_object_unref (priv->bluetooth);
  priv->bluetooth = NULL;

  gtk_switch_set_active (GTK_SWITCH (priv->bluetooth_switch), FALSE);

  gtk_widget_set_sensitive (priv->bluetooth_switch, FALSE);
  gtk_widget_set_sensitive (WID(priv->builder, "image_bluetooth"), FALSE);
  gtk_widget_set_sensitive (WID(priv->builder, "label_bluetooth"), FALSE);
}

/* Cellular Switch Section */

static void
cellular_property_changed (Technology *cellular,
                          const gchar *property,
                          GVariant *value,
                          GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  gboolean powered;

  if (!g_strcmp0 (property, "Powered")) {
    powered  = g_variant_get_boolean (g_variant_get_variant (value));

    if (priv->cellular_switch)
      gtk_switch_set_active (GTK_SWITCH (priv->cellular_switch), powered);
  }
}

static void
cellular_set_powered (GObject      *source,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  gboolean powered;
  GError *error = NULL;
  gint err_code;

  if (!technology_call_set_property_finish (priv->cellular, res, &error)) {
    err_code = error->code;
    /* Reset the switch if error is not AlreadyEnabled/AlreadyDisabled */
    if (err_code != 36) { /* Not AlreadyEnabled */
      powered = gtk_switch_get_active (GTK_SWITCH (priv->cellular_switch));
      gtk_switch_set_active (GTK_SWITCH (priv->cellular_switch), !powered);
    } else
      g_warning ("Could not set cellular Powered property: %s", error->message);

    g_error_free (error);

    return;
  }
}

static void
gis_cellular_switch_toggle (GtkSwitch *sw,
                       GParamSpec *pspec,
                       GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  gboolean powered;
  GVariant *value;

  if (priv->cellular == NULL)
    return;

  powered = gtk_switch_get_active (sw);
  value = g_variant_new_boolean (powered);

  technology_call_set_property (priv->cellular,
                             "Powered",
                             g_variant_new_variant (value),
                             priv->cancellable,
                             cellular_set_powered,
                             page);
}

static void
gis_add_technology_cellular (const gchar          *path,
                           GVariant              *properties,
                           GisNetworkPage        *page)
{
  GisNetworkPagePrivate *priv = page->priv;

  GError *error = NULL;
  GVariant *value = NULL;
  gboolean powered;

  if (priv->cellular == NULL) {
    priv->cellular = technology_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                        G_DBUS_PROXY_FLAGS_NONE,
                                                        "net.connman",
                                                        path,
                                                        priv->cancellable,
                                                        &error);

    if (error != NULL) {
      g_warning ("Could not get proxy for Cellular: %s", error->message);
      g_error_free (error);
      return;
    }

    priv->cellular_switch = WID(priv->builder, "cellular_switch");
    gtk_widget_set_sensitive (priv->bluetooth_switch, TRUE);
    gtk_widget_set_sensitive (WID(priv->builder, "image_cellular"), TRUE);
    gtk_widget_set_sensitive (WID(priv->builder, "label_cellular"), TRUE);

    g_signal_connect (priv->cellular,
                      "property_changed",
                      G_CALLBACK (cellular_property_changed),
                      page);

    g_signal_connect (GTK_SWITCH (priv->cellular_switch), "notify::active",
                      G_CALLBACK (gis_cellular_switch_toggle),
                      page);

    priv->tech_update = TRUE;
  }

  value = g_variant_lookup_value (properties, "Powered", G_VARIANT_TYPE_BOOLEAN);
  powered = g_variant_get_boolean (value);

  if (priv->cellular_switch)
    gtk_switch_set_active (GTK_SWITCH (priv->cellular_switch), powered);
}

static void
gis_remove_technology_cellular (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;

  if (priv->cellular == NULL) {
    g_warning ("Unable to remove cellular technology");
    return;
  }

  g_object_unref (priv->cellular);
  priv->cellular = NULL;

  gtk_switch_set_active (GTK_SWITCH (priv->cellular_switch), FALSE);

  gtk_widget_set_sensitive (priv->cellular_switch, FALSE);
  gtk_widget_set_sensitive (WID(priv->builder, "image_cellular"), FALSE);
  gtk_widget_set_sensitive (WID(priv->builder, "label_cellular"), FALSE);

}

static void
gis_add_technology (const gchar          *path,
                   GVariant             *properties,
                   GisNetworkPage       *page)
{
  GVariant *value = NULL;
  const gchar *type;

  value = g_variant_lookup_value (properties, "Type", G_VARIANT_TYPE_STRING);
  type = g_variant_get_string (value, NULL);

  if (!g_strcmp0 (type, "ethernet")) {
    gis_add_technology_ethernet (path, properties, page);
  } else if (!g_strcmp0 (type, "wifi")) {
    gis_add_technology_wifi (path, properties, page);
  } else if (!g_strcmp0 (type, "bluetooth")) {
    gis_add_technology_bluetooth (path, properties, page);
  } else if (!g_strcmp0 (type, "cellular")) {
    gis_add_technology_cellular (path, properties, page);
  }else {
    g_warning ("Unknown technology type");
    return;
  }
}

static void
gis_remove_technology (const gchar       *path,
                      GisNetworkPage    *page)
{
  if (!g_strcmp0 (path, "/net/connman/technology/ethernet")) {
    gis_remove_technology_ethernet (page);
  } else if (!g_strcmp0 (path, "/net/connman/technology/wifi")) {
    gis_remove_technology_wifi (page);
  } else if (!g_strcmp0 (path, "/net/connman/technology/bluetooth")) {
    gis_remove_technology_bluetooth (page);
  } else if (!g_strcmp0 (path, "/net/connman/technology/cellular")) {
    gis_remove_technology_cellular (page);
  }else {
    g_warning ("Unknown technology type");
    return;
  }
}

static void
manager_technology_added (Manager *manager,
                          const gchar *path,
                          GVariant *properties,
                          GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;

  gis_add_technology (path, properties, page);

  if (priv->tech_update) {
    priv->tech_update = FALSE;
    manager_call_get_technologies(priv->manager, priv->cancellable, manager_get_technologies, page);
  }
}

static void
manager_technology_removed (Manager *manager,
                          const gchar *path,
                          GisNetworkPage *page)
{
    gis_remove_technology (path, page);
}

static void
manager_get_technologies (GObject        *source,
                          GAsyncResult     *res,
                          gpointer         user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;

  GError *error;
  GVariant *result, *array_value, *tuple_value, *properties;
  GVariantIter array_iter, tuple_iter;
  gchar *path;

  error = NULL;
  if (!manager_call_get_technologies_finish (priv->manager, &result,
                                           res, &error))
    {
      /* TODO: display any error in a user friendly way */
      g_warning ("Could not get technologies: %s", error->message);
      g_error_free (error);
      return;
    }

  /* Result is  (a(oa{sv}))*/

  g_variant_iter_init (&array_iter, result);

  while ((array_value = g_variant_iter_next_value (&array_iter)) != NULL) {
    /* tuple_iter is oa{sv} */
    g_variant_iter_init (&tuple_iter, array_value);

    /* get the object path */
    tuple_value = g_variant_iter_next_value (&tuple_iter);
    g_variant_get (tuple_value, "o", &path);

    /* get the Properties */
    properties = g_variant_iter_next_value (&tuple_iter);

    gis_add_technology (path, properties, page);

    g_free (path);
    g_variant_unref (array_value);
    g_variant_unref (tuple_value);
    g_variant_unref (properties);
  }

  if (!priv->tech_added_id) {
    priv->tech_added_id = g_signal_connect (priv->manager,
                                            "technology_added",
                                            G_CALLBACK (manager_technology_added),
                                            page);
  }

  if (!priv->tech_removed_id) {
    priv->tech_removed_id = g_signal_connect (priv->manager,
                                              "technology_removed",
                                              G_CALLBACK (manager_technology_removed),
                                              page);
  }

  if (priv->tech_update) {
    priv->tech_update = FALSE;

    manager_call_get_technologies(priv->manager, priv->cancellable, manager_get_technologies, page);
  }
}

static void
manager_set_offlinemode(GObject      *source,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;
  gboolean offline;
  GError *error = NULL;
  gint err_code;

  if (!manager_call_set_property_finish (priv->manager, res, &error)) {
      g_warning ("Could not set OfflineMode: %s", error->message);
      err_code = error->code;
      g_error_free (error);

      /* Reset the switch */
      if (err_code != 36) { /* if not AlreadyEnabled */
        offline = gtk_switch_get_active (GTK_SWITCH (priv->offline_switch));
        gtk_switch_set_active (GTK_SWITCH (priv->offline_switch), !offline);
      }
      return;
    }
}

static void
gis_offline_switch_toggle (GtkSwitch *sw,
                       GParamSpec *pspec,
                       GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = page->priv;
  gboolean offline;
  GVariant *value;

  offline = gtk_switch_get_active (sw);
  value = g_variant_new_boolean (offline);

  manager_call_set_property (priv->manager,
                             "OfflineMode",
                             g_variant_new_variant (value),
                             priv->cancellable,
                             manager_set_offlinemode,
                             page);
}

static void
gis_setup_airplanemode_switch(GisNetworkPage *page,
                             gboolean offline)
{
  GisNetworkPagePrivate *priv = page->priv;
  GtkWidget *widget;
  GtkWidget *box;

  if (priv->offline_switch == NULL) {
        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);

        widget = gtk_label_new (_("Airplane Mode"));
        gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
        gtk_widget_set_visible (widget, TRUE);

        priv->offline_switch = gtk_switch_new ();
        gtk_box_pack_start (GTK_BOX (box), priv->offline_switch, FALSE, FALSE, 0);
        gtk_widget_set_visible (priv->offline_switch, TRUE);

//        gis_shell_embed_widget_in_header (gis_page_get_shell (GIS_PAGE (page)), box);

        g_signal_connect (GTK_SWITCH (priv->offline_switch), "notify::active",
                          G_CALLBACK (gis_offline_switch_toggle),
                          page);

        gtk_widget_set_visible (box, TRUE);

  }

  gtk_switch_set_active (GTK_SWITCH (priv->offline_switch), offline);
}

static void
gis_setup_status (GisNetworkPage *page,
                const gchar *state)
{
  GisNetworkPagePrivate *priv = page->priv;
  GtkLabel *label;
  GtkImage *image;

  label = GTK_LABEL (WID (priv->builder, "status_label"));
  gtk_label_set_text (label, state);

  image = GTK_IMAGE (WID (priv->builder, "status_image"));
  gtk_image_set_from_stock (image, gis_service_state_to_icon (state), GTK_ICON_SIZE_BUTTON);
}

static void
manager_get_properties(GObject      *source,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GisNetworkPage *page = user_data;
  GisNetworkPagePrivate *priv = page->priv;

  GError *error;
  GVariant *result = NULL;
  GVariant *value = NULL;
  gboolean offlinemode;
  const gchar *state;

  error = NULL;
  if (!manager_call_get_properties_finish (priv->manager, &result,
                                           res, &error))
    {
      /* TODO: display any error in a user friendly way */
      g_warning ("Could not get manager properties: %s", error->message);
      g_error_free (error);
      return;
    }

  value = g_variant_lookup_value (result, "OfflineMode", G_VARIANT_TYPE_BOOLEAN);
  offlinemode = g_variant_get_boolean (value);

  gis_setup_airplanemode_switch (page, offlinemode);

  value = g_variant_lookup_value (result, "State", G_VARIANT_TYPE_STRING);
  state = g_variant_get_string (value, NULL);

  gis_setup_status (page, state);
}

static void
manager_property_changed (Manager *manager,
                          const gchar *property,
                          GVariant *value,
                          GisNetworkPage *page)
{
  if (!g_strcmp0 (property, "OfflineMode")) {
    gis_setup_airplanemode_switch (page, g_variant_get_boolean (g_variant_get_variant (value)));
  }

  if (!g_strcmp0 (property, "State")) {
    gis_setup_status (page, g_variant_get_string (g_variant_get_variant (value), NULL));
  }

}

static void
gis_network_page_constructed (GObject *object)
{
  GisNetworkPage *page = GIS_NETWORK_PAGE (object);
  GisNetworkPagePrivate *priv;
  GError     *error;
  GError     *err;
  GtkWidget  *widget;

  priv = page->priv = NETWORK_PAGE_PRIVATE (page);


  priv->builder = gtk_builder_new ();

  error = NULL;
  err = NULL;
  priv->manager = manager_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                G_DBUS_PROXY_FLAGS_NONE,
                                                "net.connman",
                                                "/",
                                                priv->cancellable,
                                                &error);
  if (priv->manager == NULL) {
        g_warning ("could not get proxy for Manager: %s", error->message);
        g_error_free (error);
  }

  gtk_builder_add_from_resource (priv->builder,
                             "/org/gnome/control-center/connman/network.ui",
                             &err);

  if (err != NULL)
    {
      g_warning ("Could not load interface file: %s", err->message);
      g_error_free (err);
      return;
    }

  priv->cancellable = g_cancellable_new ();

  priv->services = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          (GDestroyNotify) g_free,
                                          (GDestroyNotify) gtk_tree_row_reference_free
                                          );

  widget = WID (priv->builder, "vbox1");
  gtk_widget_reparent (widget, (GtkWidget *) page);

  priv->offline_switch = NULL;
  priv->popup = FALSE;

  /* Set up AirplaneMode  button */
  if (priv->manager != NULL){
    manager_call_get_properties(priv->manager, priv->cancellable, manager_get_properties, page);

    g_signal_connect (priv->manager,
                      "property_changed",
                      G_CALLBACK (manager_property_changed),
                      page);

    manager_call_get_technologies(priv->manager, priv->cancellable, manager_get_technologies, page);

    gis_setup_service_list(page);
  }

  gtk_container_add (GTK_CONTAINER (page), WID (priv->builder,
                                                "network-settings"));

  gis_page_set_title (GIS_PAGE (page), _("Network setup"));
  gtk_widget_show (GTK_WIDGET (page));
}

static GtkBuilder *
gis_network_page_get_builder (GisPage *page)
{
  return GIS_NETWORK_PAGE (page)->priv->builder;
}

static void
gis_network_page_class_init (GisNetworkPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  page_class->get_builder = gis_network_page_get_builder;
  object_class->constructed = gis_network_page_constructed;

  g_type_class_add_private (object_class, sizeof(GisNetworkPagePrivate));
}

static void
gis_network_page_class_finalize (GisNetworkPageClass *klass)
{
}

static void
gis_network_page_init (GisNetworkPage *page)
{
  g_resources_register (gis_network_get_resource ());
}

void
gis_prepare_network_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_NETWORK_PAGE,
                                     "driver", driver,
                                     NULL));
}
