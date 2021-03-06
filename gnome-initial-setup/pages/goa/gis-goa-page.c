/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

/* Online accounts page {{{1 */

#define PAGE_ID "goa"

#include "config.h"
#include "gis-goa-page.h"

#define GOA_API_IS_SUBJECT_TO_CHANGE
#include <goa/goa.h>
#define GOA_BACKEND_API_IS_SUBJECT_TO_CHANGE
#include <goabackend/goabackend.h>

#include "cc-online-accounts-add-account-dialog.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

G_DEFINE_TYPE (GisGoaPage, gis_goa_page, GIS_TYPE_PAGE);

#define GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GIS_TYPE_GOA_PAGE, GisGoaPagePrivate))

struct _GisGoaPagePrivate {
  GoaClient *goa_client;
};

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE(page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

static void
show_online_account_dialog (GtkButton *button,
                            gpointer   user_data)
{
  GisGoaPage *page = GIS_GOA_PAGE (user_data);
  GisGoaPagePrivate *priv = page->priv;
  GtkWindow *parent;
  GtkWidget *dialog;
  gint response;
  GList *providers;
  GList *l;
  GoaObject *object;
  GError *error;

  providers = NULL;

  parent = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (page)));

  dialog = goa_panel_add_account_dialog_new (priv->goa_client);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  providers = goa_provider_get_all ();
  for (l = providers; l != NULL; l = l->next)
    {
      GoaProvider *provider;

      provider = GOA_PROVIDER (l->data);
      goa_panel_add_account_dialog_add_provider (GOA_PANEL_ADD_ACCOUNT_DIALOG (dialog), provider);
    }

  gtk_widget_show_all (dialog);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response != GTK_RESPONSE_OK)
    {
      gtk_widget_destroy (dialog);
      goto out;
    }

  error = NULL;
  object = goa_panel_add_account_dialog_get_account (GOA_PANEL_ADD_ACCOUNT_DIALOG (dialog), &error);

  if (object == NULL)
    {
      gtk_widget_destroy (dialog);
      if (!(error->domain == GOA_ERROR && error->code == GOA_ERROR_DIALOG_DISMISSED))
        {
          dialog = gtk_message_dialog_new (parent,
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           _("Error creating account"));
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                    "%s",
                                                    error->message);
          gtk_widget_show_all (dialog);
          gtk_dialog_run (GTK_DIALOG (dialog));
          gtk_widget_destroy (dialog);
        }
      g_error_free (error);
    }

 out:
  g_list_foreach (providers, (GFunc) g_object_unref, NULL);
  g_list_free (providers);
}

static void
remove_account_cb (GoaAccount   *account,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  GisGoaPage *page = GIS_GOA_PAGE (user_data);
  GError *error;

  error = NULL;
  if (!goa_account_call_remove_finish (account, res, &error))
    {
      GtkWidget *dialog;
      dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (page))),
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("Error removing account"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                "%s",
                                                error->message);
      gtk_widget_show_all (dialog);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_error_free (error);
    }
}


static void
confirm_remove_account (GtkButton *button, gpointer user_data)
{
  GisGoaPage *page = GIS_GOA_PAGE (user_data);
  GtkWidget *dialog;
  GoaObject *object;
  gint response;

  object = g_object_get_data (G_OBJECT (button), "goa-object");

  dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (page))),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_CANCEL,
                                   _("Are you sure you want to remove the account?"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("This will not remove the account on the server."));
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Remove"), GTK_RESPONSE_OK);
  gtk_widget_show_all (dialog);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  if (response == GTK_RESPONSE_OK)
    {
      goa_account_call_remove (goa_object_peek_account (object),
                               NULL, /* GCancellable */
                               (GAsyncReadyCallback) remove_account_cb,
                               page);
    }
}


static void
add_account_to_list (GisGoaPage *page, GoaObject *object)
{
  GtkWidget *list;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *button;
  GoaAccount *account;
  GIcon *icon;
  gchar *markup;

  account = goa_object_peek_account (object);

  icon = g_icon_new_for_string (goa_account_get_provider_icon (account), NULL);
  markup = g_strdup_printf ("<b>%s</b>\n"
                            "<small><span foreground=\"#555555\">%s</span></small>",
                            goa_account_get_provider_name (account),
                            goa_account_get_presentation_identity (account));

  list = WID ("online-accounts-list");

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_hexpand (box, TRUE);

  g_object_set_data (G_OBJECT (box), "account-id",
                     (gpointer)goa_account_get_id (account));

  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
  label = gtk_label_new (markup);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  button = gtk_button_new_with_label (_("Remove"));
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);

  g_object_set_data_full (G_OBJECT (button), "goa-object",
                          g_object_ref (object), g_object_unref);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (confirm_remove_account), page);

  gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);

  gtk_widget_show_all (box);

  gtk_container_add (GTK_CONTAINER (list), box);
}

static void
remove_account_from_list (GisGoaPage *page, GoaObject *object)
{
  GtkWidget *list;
  GList *children, *l;
  GtkWidget *child;
  GoaAccount *account;
  const gchar *account_id, *id;

  account = goa_object_peek_account (object);

  account_id = goa_account_get_id (account);

  list = WID ("online-accounts-list");

  children = gtk_container_get_children (GTK_CONTAINER (list));
  for (l = children; l; l = l->next)
    {
      child = GTK_WIDGET (l->data);

      id = (const gchar *)g_object_get_data (G_OBJECT (child), "account-id");

      if (g_strcmp0 (id, account_id) == 0)
        {
          gtk_widget_destroy (child);
          break;
        }
    }
  g_list_free (children);
}

static void
populate_account_list (GisGoaPage *page)
{
  GisGoaPagePrivate *priv = page->priv;
  GList *accounts, *l;
  GoaObject *object;

  accounts = goa_client_get_accounts (priv->goa_client);
  for (l = accounts; l; l = l->next)
    {
      object = GOA_OBJECT (l->data);
      add_account_to_list (page, object);
    }

  g_list_free_full (accounts, (GDestroyNotify) g_object_unref);
}

static void
goa_account_added (GoaClient *client, GoaObject *object, gpointer user_data)
{
  GisGoaPage *page = GIS_GOA_PAGE (user_data);

  g_debug ("Online account added");

  add_account_to_list (page, object);
}

static void
goa_account_removed (GoaClient *client, GoaObject *object, gpointer user_data)
{
  GisGoaPage *page = GIS_GOA_PAGE (user_data);

  g_debug ("Online account removed");

  remove_account_from_list (page, object);
}

static void
gis_goa_page_constructed (GObject *object)
{
  GisGoaPage *page = GIS_GOA_PAGE (object);
  GisGoaPagePrivate *priv = page->priv;
  GtkWidget *button;
  GError *error = NULL;

  G_OBJECT_CLASS (gis_goa_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID ("goa-page"));

  priv->goa_client = goa_client_new_sync (NULL, &error);

  if (priv->goa_client == NULL)
    {
       g_error ("Failed to get a GoaClient: %s", error->message);
       g_error_free (error);
       return;
    }

  populate_account_list (page);

  button = WID("online-add-button");
  g_signal_connect (button, "clicked",
                    G_CALLBACK (show_online_account_dialog), page);

  g_signal_connect (priv->goa_client, "account-added",
                    G_CALLBACK (goa_account_added), page);
  g_signal_connect (priv->goa_client, "account-removed",
                    G_CALLBACK (goa_account_removed), page);

  gis_page_set_title (GIS_PAGE (page), _("Online Accounts"));
  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_goa_page_dispose (GObject *object)
{
  GisGoaPage *page = GIS_GOA_PAGE (object);
  GisGoaPagePrivate *priv = page->priv;

  g_clear_object (&priv->goa_client);

  G_OBJECT_CLASS (gis_goa_page_parent_class)->dispose (object);
}

static void
gis_goa_page_class_init (GisGoaPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  object_class->constructed = gis_goa_page_constructed;
  object_class->dispose = gis_goa_page_dispose;
  
  g_type_class_add_private (object_class, sizeof(GisGoaPagePrivate));
}

static void
gis_goa_page_init (GisGoaPage *page)
{
  page->priv = GET_PRIVATE (page);
}

void
gis_prepare_goa_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_GOA_PAGE,
                                     "driver", driver,
                                     NULL));
}
