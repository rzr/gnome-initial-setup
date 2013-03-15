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
 *
 * Based on Connman and Network Manager gnome-control-center panel
 */

#define PAGE_ID "connman"

#include <config.h>

#include <stdlib.h>
#include <glib/gi18n.h>

#include "gis-connman-page.h"
#include "gis-connman-resources.h"

#include "manager.h"
#include "technology.h"
#include "service.h"

G_DEFINE_TYPE (GisConnmanPage, gis_connman_page, GIS_TYPE_PAGE);

#define WID(b, w) (GtkWidget *) gtk_builder_get_object (b, w)

#define CONNMAN_PAGE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), GIS_TYPE_CONNMAN_PAGE, GisConnmanPagePrivate))

enum {
  COLUMN_ICON,
  COLUMN_PULSE,
  COLUMN_PULSE_ID,
  COLUMN_NAME,
  COLUMN_STATE,
  COLUMN_SECURITY_ICON,
  COLUMN_SECURITY,
  COLUMN_TYPE,
  COLUMN_STRENGTH_ICON,
  COLUMN_STRENGTH,
  COLUMN_FAVORITE,
  COLUMN_GDBUSPROXY,
  COLUMN_PROP_ID,
  COLUMN_LAST
};

struct _GisConnmanPagePrivate
{
  GCancellable  *cancellable;
  GtkBuilder    *builder;
  Manager       *manager;
  GHashTable    *services;
  gint          serv_id;
  gboolean      serv_update;

  Technology    *wifi;
};

/*TODO CLEAN THIS MESS */
static gboolean cc_service_state_to_icon (const gchar *state)
{
        if ((g_strcmp0 (state, "association") == 0) || (g_strcmp0 (state, "configuration") == 0))
                return TRUE;
        else
                return FALSE;
}

static const gchar *cc_service_security_to_string (const gchar **security)
{
        if (security == NULL)
                return "none";
        else
                return security[0];
}

static const gchar *cc_service_security_to_icon (const gchar **security)
{
        if (security == NULL)
                return NULL;
        
        if (!g_strcmp0 (security[0], "none"))
                return NULL;
        else if (!g_strcmp0 (security[0], "wep") || !g_strcmp0 (security[0], "wps") || !g_strcmp0 (security[0], "psk"))
                return "network-wireless-encrypted-symbolic";
        else if (!g_strcmp0 (security[0], "ieee8021x"))
                return "connman_corporate";
        else
                return NULL;
}

static const gchar *cc_service_strength_to_string (const gchar *type, const gchar strength)
{
        if (!g_strcmp0 (type, "ethernet") || !g_strcmp0 (type, "bluetooth"))
                return "Excellent";

        if (strength > 80)
                return "Excellent";
        else if (strength > 55)
                return "Good";
        else if (strength > 30)
                return "ok";
        else if (strength > 5)
                return "weak";
        else
                return "n/a";
}

static const gchar *cc_service_type_to_icon (const gchar *type, gchar strength)
{
        if (g_strcmp0 (type, "ethernet") == 0)
                return "network-wired-symbolic";
        else if (g_strcmp0 (type, "bluetooth") == 0)
                return "bluetooth-active-symbolic";
        else if (g_strcmp0 (type, "wifi") == 0) {
                if (strength > 80)
                        return "network-wireless-signal-excellent-symbolic";
                else if (strength > 55)
                        return "network-wireless-signal-good-symbolic";
                else if (strength > 30)
                        return "network-wireless-signal-ok-symbolic";
                else
                        return "network-wireless-signal-excellent-symbolic";
        } else if (g_strcmp0 (type, "cellular") == 0) {
                if (strength > 80)
                        return "network-cellular-signal-excellent-symbolic";
                else if (strength > 55)
                        return "network-cellular-signal-good-symbolic";
                else if (strength > 30)
                        return "network-cellular-signal-ok-symbolic";
                else
                        return "network-cellular-signal-excellent-symbolic";
        } else
                return NULL;
}


static gboolean
spinner_timeout (gpointer page)
{
  GisConnmanPagePrivate *priv = GIS_CONNMAN_PAGE (page)->priv;
  const gchar *path  = g_object_get_data (G_OBJECT (page), "path");

  GtkTreeRowReference *row;
  GtkTreeModel *model;

  GtkTreePath *tree_path;
  GtkTreeIter iter;
  guint pulse;

  row = g_hash_table_lookup (priv->services, path);

  if (row == NULL)
    return FALSE;

  model =  gtk_tree_row_reference_get_model (row);
  tree_path = gtk_tree_row_reference_get_path (row);
  gtk_tree_model_get_iter (model, &iter, tree_path);

  gtk_tree_model_get (model, &iter, COLUMN_PULSE, &pulse, -1);

  if (pulse == G_MAXUINT)
    pulse = 0;
  else
    pulse++;

  gtk_list_store_set (GTK_LIST_STORE (model),
                      &iter,
                      COLUMN_PULSE, pulse,
                      -1);

  gtk_tree_path_free (tree_path);
  return TRUE;
}

static void
service_property_changed (Service *service,
                          const gchar *property,
                          GVariant *value,
                          GisConnmanPage *page)
{
        GisConnmanPagePrivate *priv = page->priv;
        GtkListStore *liststore_services;

        GtkTreeRowReference *row;
        GtkTreePath *tree_path;
        GtkTreeIter iter;

        const gchar *path;
        gchar *type = NULL;
        gchar strength = 0;
        const gchar *state;
        gchar *str;

        gboolean favorite;
        gint id;

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
                                    COLUMN_STRENGTH, cc_service_strength_to_string (type, strength),
                                    -1);

                gtk_list_store_set (liststore_services,
                                    &iter,
                                    COLUMN_STRENGTH_ICON, cc_service_type_to_icon (type, strength),
                                    -1);
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
                                    COLUMN_ICON, cc_service_state_to_icon (state),
                                    -1);

                gtk_tree_model_get (GTK_TREE_MODEL (liststore_services), &iter, COLUMN_PULSE_ID, &id, -1);
                if ((g_strcmp0 (state, "association") == 0) || (g_strcmp0 (state, "configuration") == 0)) {
                    if (id == 0) {
                        g_object_set_data (G_OBJECT (page), "path",
                                           (gpointer)path);

                        id = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                                 80,
                                                 spinner_timeout,
                                                 (gpointer)page,
                                                 NULL);

                        gtk_list_store_set (liststore_services,
                                            &iter,
                                            COLUMN_PULSE_ID, id,
                                            COLUMN_PULSE, 0,
                                            -1);
                    }
                } else {
                    if (id != 0) {
                        g_source_remove (id);
                        gtk_list_store_set (liststore_services,
                                            &iter,
                                            COLUMN_PULSE_ID, 0,
                                            COLUMN_PULSE, 0,
                                            -1);
                    }
                }
        }

        if (!g_strcmp0 (property, "Favorite")) {
                favorite = g_variant_get_boolean (g_variant_get_variant (value));

                gtk_list_store_set (liststore_services,
                                    &iter,
                                    COLUMN_FAVORITE, favorite,
                                    -1);
        }
}

static void
_add_service (const gchar    *path,
              GVariant       *properties,
              GisConnmanPage *page)
{
        GisConnmanPagePrivate *priv = page->priv;
        GError *error = NULL;

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
        gint prop_id;
        gint id;

        gchar strength = 0;
        gboolean favorite = FALSE;

        /* if found in hash, just update the properties */

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

        g_variant_lookup (properties, "Favorite", "b", &favorite);

        if (!g_strcmp0 (type, "wifi")) {
                value = g_variant_lookup_value (properties, "Security", G_VARIANT_TYPE_STRING_ARRAY);
                security = g_variant_get_strv (value, NULL);

                value = g_variant_lookup_value (properties, "Strength", G_VARIANT_TYPE_BYTE);
                strength = (gchar ) g_variant_get_byte (value);
        }

        service = service_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                  G_DBUS_PROXY_FLAGS_NONE,
                                                  "net.connman",
                                                  path,
                                                  priv->cancellable,
                                                  &error);

        if (error != NULL) {
                g_warning ("could not get proxy for %s: %s", name,  error->message);
                g_error_free (error);
                return;
        }


        prop_id = g_signal_connect (service,
                                    "property_changed",
                                    G_CALLBACK (service_property_changed),
                                    page);

        gtk_list_store_set (liststore_services,
                      &iter,
                      COLUMN_ICON, cc_service_state_to_icon (state),
                      COLUMN_PULSE, 0,
                      COLUMN_PULSE_ID, 0,
                      COLUMN_NAME, g_strdup (name),
                      COLUMN_STATE, g_strdup (state),
                      COLUMN_SECURITY_ICON, cc_service_security_to_icon (security),
                      COLUMN_SECURITY, cc_service_security_to_string (security),
                      COLUMN_TYPE, g_strdup (type),
                      COLUMN_STRENGTH_ICON, cc_service_type_to_icon (type, strength),
                      COLUMN_STRENGTH, cc_service_strength_to_string (type, strength),
                      COLUMN_FAVORITE, favorite,
                      COLUMN_GDBUSPROXY, service,
                      COLUMN_PROP_ID, prop_id,
                      -1);

        tree_path = gtk_tree_model_get_path ((GtkTreeModel *) liststore_services, &iter);

        row = gtk_tree_row_reference_new ((GtkTreeModel *) liststore_services, tree_path);

        g_hash_table_insert (priv->services,
                             g_strdup (path),
                             gtk_tree_row_reference_copy (row));

        gtk_tree_model_get (GTK_TREE_MODEL (liststore_services), &iter, COLUMN_PULSE_ID, &id, -1);

        if ((g_strcmp0 (state, "association") == 0) || (g_strcmp0 (state, "configuration") == 0)) {
                if (id == 0) {
                        g_object_set_data (G_OBJECT (page), "path",
                                           (gpointer)path);

                        id = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                                 80,
                                                 spinner_timeout,
                                                 page,
                                                 NULL);

                        gtk_list_store_set (liststore_services,
                                            &iter,
                                            COLUMN_PULSE_ID, id,
                                            COLUMN_PULSE, 0,
                                            -1);
                }
        } else {
                if (id != 0) {
                        g_source_remove (id);
                        gtk_list_store_set (liststore_services,
                                            &iter,
                                            COLUMN_PULSE_ID, 0,
                                            COLUMN_PULSE, 0,
                                            -1);
                }

        }

        gtk_tree_path_free (tree_path);
        gtk_tree_row_reference_free (row);
}


static void
panel_set_scan_cb (GObject      *source,
                   GAsyncResult *res,
                   gpointer      user_data)
{
        GisConnmanPage *panel = user_data;
        GisConnmanPagePrivate *priv = panel->priv;
        GError *error = NULL;
        gint err_code;

        if (!priv->wifi)
                return;

        if (!technology_call_scan_finish (priv->wifi, res, &error)) {
                err_code = error->code;

                /* Reset the switch if error is not AlreadyEnabled/AlreadyDisabled */
                if (err_code != 36)
                  g_warning ("Could not set scan wifi: %s", error->message);

                g_error_free (error);
                return;
        }
}

static void
panel_set_scan (GisConnmanPage *panel)
{
        GisConnmanPagePrivate *priv;
        priv = panel->priv;

        if (!priv->wifi)
                return;

        technology_call_scan (priv->wifi,
                              priv->cancellable,
                              panel_set_scan_cb,
                              panel);
}



static void
manager_services_changed (Manager *manager,
                          GVariant *added,
                          const gchar *const *removed,
                          GisConnmanPage *panel)
{
        GisConnmanPagePrivate *priv = panel->priv;
        GtkListStore *liststore_services;
        gint i;

        GtkTreeRowReference *row;
        GtkTreePath *tree_path;
        GtkTreeIter iter;

        GVariant *array_value, *tuple_value, *properties;
        GVariantIter array_iter, tuple_iter;
        gchar *path;

        gint *new_pos;
        gint elem_size;
        Service *service;
        gint prop_id;

        liststore_services = GTK_LIST_STORE (WID (priv->builder, "liststore_services"));

        for (i=0; removed != NULL && removed[i] != NULL; i++) {
                row = g_hash_table_lookup (priv->services,
                                           (gconstpointer *)removed[i]);

                if (row == NULL)
                        continue;

                tree_path = gtk_tree_row_reference_get_path (row);

                gtk_tree_model_get_iter ((GtkTreeModel *) liststore_services, &iter, tree_path);

                gtk_tree_model_get (GTK_TREE_MODEL (liststore_services),
                                    &iter,
                                    COLUMN_GDBUSPROXY, &service,
                                    COLUMN_PROP_ID, &prop_id,
                                    -1);

                g_signal_handler_disconnect (service, prop_id);

                gtk_list_store_remove (liststore_services, &iter);

                g_hash_table_remove (priv->services, removed[i]);

                gtk_tree_path_free (tree_path);
        }

        g_variant_iter_init (&array_iter, added);

        elem_size = (gint)g_variant_iter_n_children (&array_iter);
        new_pos = (gint *) g_malloc (elem_size * sizeof (gint));
        i = 0;

        /* Added Services */
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

                        _add_service (path, properties, panel);
                        g_variant_unref (properties);
                }

                row = g_hash_table_lookup (priv->services,
                                           (gconstpointer *) path);

                if (row == NULL) {
                        g_printerr ("\nSomething bad bad happened here!");
                        return;
                }

                tree_path = gtk_tree_row_reference_get_path (row);
                gint *old_pos = gtk_tree_path_get_indices (tree_path);
                new_pos[i] = old_pos[0];
                i++;

                gtk_tree_path_free (tree_path);

                g_free (path);
                g_variant_unref (array_value);
                g_variant_unref (tuple_value);

        }

        if (new_pos) {
                gtk_list_store_reorder (liststore_services, new_pos);
                g_free (new_pos);
        }

        /* Since we dont have a scan button on our UI, send a scan command */
        panel_set_scan (panel);
}

static void
manager_get_services (GObject        *source,
                      GAsyncResult   *res,
                      gpointer       user_data)
{
  GisConnmanPage *panel = user_data;
  GisConnmanPagePrivate *priv = panel->priv;

  GError *error;
  GVariant *result, *array_value, *tuple_value, *properties;
  GVariantIter array_iter, tuple_iter;
  gchar *path;
  gint size;

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

  size = (gint)g_variant_iter_n_children (&array_iter);
  if (size == 0)
    panel_set_scan (panel);

  while ((array_value = g_variant_iter_next_value (&array_iter)) != NULL) {
      /* tuple_iter is oa{sv} */
      g_variant_iter_init (&tuple_iter, array_value);

      /* get the object path */
      tuple_value = g_variant_iter_next_value (&tuple_iter);
      g_variant_get (tuple_value, "o", &path);

      /* get the Properties */
      properties = g_variant_iter_next_value (&tuple_iter);

      _add_service (path, properties, panel);

      g_free (path);

      g_variant_unref (array_value);
      g_variant_unref (tuple_value);
      g_variant_unref (properties);
  }

  if (size < 2)
    panel_set_scan (panel);
}

static void
service_connect_cb (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
        GError *error = NULL;
        Service *service = user_data;

        service_call_connect_finish (service, res, &error);
        if (error != NULL) {
                g_warning ("Couldn't Connect to service: %s", error->message);
                g_error_free (error);
                return;
        }
}

static void
service_disconnect_cb (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
        GError *error = NULL;
        Service *service = user_data;

        service_call_disconnect_finish (service, res, &error);
        if (error != NULL) {
                g_warning ("Couldn't Disconnect from service: %s", error->message);
                g_error_free (error);
                return;
        }
}

static void
set_service_name (GtkTreeViewColumn *col,
                  GtkCellRenderer   *renderer,
                  GtkTreeModel      *model,
                  GtkTreeIter       *iter,
                  gpointer           user_data)
{
        gboolean fav;
        const gchar *name, *state;
        gchar *uniname;

        gtk_tree_model_get (model, iter, COLUMN_FAVORITE, &fav, -1);
        gtk_tree_model_get (model, iter, COLUMN_NAME, &name, -1);
        gtk_tree_model_get (model, iter, COLUMN_STATE, &state, -1);

        if ((g_strcmp0 (state, "ready") == 0) || (g_strcmp0 (state, "online") == 0))
                uniname =  g_strdup_printf ("%s \u2713", name);
        else
                uniname = g_strdup (name);

        if (fav) {
                g_object_set (renderer,
                      "weight", PANGO_WEIGHT_BOLD,
                      "weight-set", TRUE,
                      "text", uniname,
                      NULL);
        } else {
                g_object_set (renderer,
                      "weight", PANGO_WEIGHT_ULTRALIGHT,
                      "weight-set", TRUE,
                      "text", uniname,
                      NULL);
        }

        g_free (uniname);
}

static void
activate_service_cb (GtkTreeView       *tree_view,
                     GtkTreePath       *tree_path,
                     GtkTreeViewColumn *column,
                     GisConnmanPage    *page)
{
  GisConnmanPagePrivate *priv;
  GtkListStore *liststore_services;

  GtkTreeIter iter;
  Service *service;
  gchar *state;

  priv = page->priv;

  g_debug ("Trying to active selected service");

  liststore_services = GTK_LIST_STORE (WID (priv->builder,
                                            "liststore_services"));

  gtk_tree_model_get_iter ((GtkTreeModel *) liststore_services,
                           &iter,
                           tree_path);

  gtk_tree_model_get (GTK_TREE_MODEL (liststore_services),
                      &iter, COLUMN_GDBUSPROXY,
                      &service, COLUMN_STATE, &state, -1);


  if (!g_strcmp0 (state, "online") || !g_strcmp0 (state, "ready"))
    service_call_disconnect (service, priv->cancellable, service_disconnect_cb, service);
  else if (!g_strcmp0 (state, "idle") || !g_strcmp0 (state, "failure"))
      service_call_connect (service, priv->cancellable,
                            service_connect_cb, service);

  g_free (state);
}

static void
cursor_changed_cb (GtkTreeView    *tree_view,
                   GisConnmanPage *page)
{
  GtkTreePath *path;
  GtkTreeViewColumn *col;

  gtk_tree_view_get_cursor (tree_view, &path, &col);
  gtk_tree_view_row_activated (tree_view, path, col);

  gtk_tree_path_free (path);
}

static void
_setup_service_columns (GisConnmanPage *page)
{
  GisConnmanPagePrivate *priv;

  GtkCellRenderer *renderer1;
  GtkCellRenderer *renderer2;
  GtkCellRenderer *renderer3;
  GtkCellRenderer *renderer4;
  GtkCellRenderer *renderer5;
  GtkTreeViewColumn *column;
  GtkCellArea *area;
  GtkTreeView *treeview;

  priv = page->priv;

  column = GTK_TREE_VIEW_COLUMN (WID (priv->builder, "treeview_list_column"));
  treeview = GTK_TREE_VIEW (WID (priv->builder, "treeview_services"));

  g_signal_connect (treeview, "row-activated",
                    G_CALLBACK (activate_service_cb), page);
  g_signal_connect (treeview, "cursor-changed",
                    G_CALLBACK (cursor_changed_cb), page);


  area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column));


  /* Column1 : Spinner (Online/Ready) */
  renderer1 = gtk_cell_renderer_spinner_new ();

  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer1, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer1, "active", COLUMN_ICON, "pulse", COLUMN_PULSE, NULL);

  gtk_cell_area_cell_set (area, renderer1, "align", TRUE, NULL);

  /* Column2 : Type(with strength) */
  renderer2 = gtk_cell_renderer_pixbuf_new ();

  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer2, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer2, "icon-name", COLUMN_STRENGTH_ICON, NULL);
  g_object_set (renderer2,
                "follow-state", TRUE,
                "xpad", 1,
                "ypad", 6,
                NULL);

  gtk_cell_area_cell_set (area, renderer2, "align", TRUE, NULL);

  /* Column3 : The Name */
  renderer3 = gtk_cell_renderer_text_new ();
  g_object_set (renderer3,
                "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
                "ellipsize", PANGO_ELLIPSIZE_END,
                NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer3, TRUE);

  gtk_tree_view_column_set_cell_data_func (column, renderer3, set_service_name, NULL, NULL);

  gtk_cell_area_cell_set (area, renderer3,
                          "align", TRUE,
                          "expand", TRUE,
                          NULL);

  /* Column4 : Security */
  renderer4 = gtk_cell_renderer_pixbuf_new ();

  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer4, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer4, "icon-name", COLUMN_SECURITY_ICON, NULL);
  g_object_set (renderer4,
                "follow-state", TRUE,
                "xpad", 6,
                "ypad", 6,
                NULL);

  gtk_cell_area_cell_set (area, renderer4, "align", TRUE, NULL);

  gtk_cell_area_add_focus_sibling (area, renderer3, renderer1);
  gtk_cell_area_add_focus_sibling (area, renderer3, renderer2);
  gtk_cell_area_add_focus_sibling (area, renderer3, renderer4);


  panel_set_scan (page);
}



/* Begin GIS page specfic code */

static void
gis_connman_page_constructed (GObject *object)
{
  GisConnmanPage *page = GIS_CONNMAN_PAGE (object);
  GisConnmanPagePrivate *priv;
  GError     *error = NULL;
  GtkWidget  *outerbox;
  guint ret;

  priv = page->priv = CONNMAN_PAGE_PRIVATE (page);

  G_OBJECT_CLASS (gis_connman_page_parent_class)->constructed (object);

  priv->builder = gtk_builder_new ();

  priv->manager = manager_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                  G_DBUS_PROXY_FLAGS_NONE,
                                                  "net.connman",
                                                  "/",
                                                  priv->cancellable,
                                                  &error);
  priv->serv_update = FALSE;

  priv->serv_id = g_signal_connect (priv->manager, "services_changed",
                                    G_CALLBACK (manager_services_changed),
                                    page);

  manager_call_get_services (priv->manager, priv->cancellable,
                             manager_get_services, page);


  if (priv->manager == NULL) {
        g_warning ("could not get proxy for Manager: %s", error->message);
        g_clear_error (&error);
  }

  ret =
    gtk_builder_add_from_resource (priv->builder,
                                   "/org/gnome/initial-setup/gis-connman-page.ui",
                                   &error);

  if (!ret)
    {
      g_warning ("Could not load interface file: %s", error->message);
      g_clear_error (&error);
      return;
    }

  priv->cancellable = g_cancellable_new ();

  priv->services =
    g_hash_table_new_full (g_str_hash,
                           g_str_equal,
                           (GDestroyNotify) g_free,
                           (GDestroyNotify) gtk_tree_row_reference_free);

  outerbox = WID (priv->builder, "connman-page");
  gtk_container_add (GTK_CONTAINER (page), outerbox);

  gis_page_set_title (GIS_PAGE (page), _("Network setup"));
  gis_page_set_complete (GIS_PAGE (page), TRUE);
  gtk_widget_show (GTK_WIDGET (page));


  _setup_service_columns (page);
}


static GtkBuilder *
gis_connman_page_get_builder (GisPage *page)
{
  return GIS_CONNMAN_PAGE (page)->priv->builder;
}

static void
gis_connman_page_dispose (GObject *page)
{
  GisConnmanPagePrivate *priv;

  priv = GIS_CONNMAN_PAGE (page)->priv;

  g_cancellable_cancel (priv->cancellable);

  g_clear_object (&priv->builder);
  g_clear_object (&priv->manager);
  g_clear_object (&priv->wifi);

  g_hash_table_destroy (priv->services);
  priv->services = NULL;
}

static void
gis_connman_page_class_init (GisConnmanPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  page_class->get_builder = gis_connman_page_get_builder;
  object_class->constructed = gis_connman_page_constructed;
  object_class->dispose = gis_connman_page_dispose;

  g_type_class_add_private (object_class, sizeof(GisConnmanPagePrivate));
}

static void
gis_connman_page_init (GisConnmanPage *page)
{
  g_resources_register (gis_connman_get_resource ());
}

void
gis_prepare_connman_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_CONNMAN_PAGE,
                                     "driver", driver,
                                     NULL));
}
