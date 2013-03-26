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

#include "config.h"

#include "gnome-initial-setup.h"

#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#ifdef HAVE_CLUTTER
#include <clutter-gtk/clutter-gtk.h>
#endif

#ifdef HAVE_CHEESE
#include <cheese-gtk.h>
#endif

#include <egg-list-box.h>

#include "pages/language/gis-language-page.h"
#include "pages/keyboard/gis-keyboard-page.h"
#include "pages/eulas/gis-eula-pages.h"
#include "pages/connman/gis-connman-page.h"
#include "pages/account/gis-account-page.h"
#include "pages/location/gis-location-page.h"
#include "pages/goa/gis-goa-page.h"
#include "pages/summary/gis-summary-page.h"

/* main {{{1 */

static gboolean force_new_user_mode;
static const gchar *system_setup_pages[] = {
    "account",
    "location"
};

typedef void (*PreparePage) (GisDriver *driver);

typedef struct {
    const gchar *page_id;
    PreparePage prepare_page_func;
} PageData;

#define PAGE(name) { #name, gis_prepare_ ## name ## _page }

static PageData page_table[] = {
  PAGE (language),
  PAGE (keyboard),
  PAGE (eula),
  PAGE (connman),
  PAGE (account),
  PAGE (location),
  PAGE (goa),
  PAGE (summary),
  { NULL },
};

#undef PAGE

static gboolean
should_skip_page (GisDriver    *driver,
                  const gchar  *page_id,
                  gchar       **skip_pages)
{
  guint i = 0;
  /* check through our skip pages list for pages we don't want */
  if (skip_pages) {
    while (skip_pages[i]) {
      if (g_strcmp0 (skip_pages[i], page_id) == 0)
        return TRUE;
      i++;
    }
  }

  switch (gis_driver_get_mode (driver)) {
  case GIS_DRIVER_MODE_EXISTING_USER:
    i = 0;
    while (i < G_N_ELEMENTS (system_setup_pages)) {
      if (g_strcmp0 (system_setup_pages[i], page_id) == 0)
        return TRUE;
      i++;
    }
    break;
  case GIS_DRIVER_MODE_NEW_USER:
  default:
    break;
  }

  return FALSE;
}

static gchar **
pages_to_skip_from_file (void)
{
  GKeyFile *skip_pages_file;
  gchar **skip_pages;

  skip_pages_file = g_key_file_new ();
  /* TODO: put the skipfile somewhere sensible */
  if (g_key_file_load_from_file (skip_pages_file, "/tmp/skip_pages_file",
                                 G_KEY_FILE_NONE,
                                 NULL)) {
    skip_pages = g_key_file_get_string_list (skip_pages_file, "pages", "skip",
                                             NULL, NULL);
    g_key_file_free (skip_pages_file);

    return skip_pages;
  }

  return NULL;
}

static void
destroy_pages_after (GisAssistant *assistant,
                     GisPage      *page)
{
  GList *pages, *l, *next;

  pages = gis_assistant_get_all_pages (assistant);

  for (l = pages; l != NULL; l = l->next)
    if (l->data == page)
      break;

  l = l->next;
  for (; l != NULL; l = next) {
    next = l->next;
    gtk_widget_destroy (GTK_WIDGET (l->data));
  }
}

static void
rebuild_pages_cb (GisDriver *driver)
{
  PageData *page_data;
  GisAssistant *assistant;
  GisPage *current_page;
  gchar **skip_pages;

  assistant = gis_driver_get_assistant (driver);
  current_page = gis_assistant_get_current_page (assistant);

  skip_pages = pages_to_skip_from_file ();

  page_data = page_table;

  if (current_page != NULL) {
    destroy_pages_after (assistant, current_page);

    for (page_data = page_table; page_data->page_id != NULL; ++page_data)
      if (g_str_equal (page_data->page_id, GIS_PAGE_GET_CLASS (current_page)->page_id))
        break;

    ++page_data;
  }

  for (; page_data->page_id != NULL; ++page_data)
    if (!should_skip_page (driver, page_data->page_id, skip_pages))
      page_data->prepare_page_func (driver);

  g_strfreev (skip_pages);
}

static gboolean
is_running_as_gnome_initial_setup_user (void)
{
  struct passwd pw, *pwp;
  char buf[4096];

  getpwnam_r ("gnome-initial-setup", &pw, buf, sizeof (buf), &pwp);
  if (pwp == NULL)
    return FALSE;

  return pw.pw_uid == getuid ();
}

static GisDriverMode
get_mode (void)
{
  if (force_new_user_mode)
    return GIS_DRIVER_MODE_NEW_USER;
  else if (is_running_as_gnome_initial_setup_user ())
    return GIS_DRIVER_MODE_NEW_USER;
  else
    return GIS_DRIVER_MODE_EXISTING_USER;
}

int
main (int argc, char *argv[])
{
  GisDriver *driver;
  int status;
  GOptionContext *context;

  GOptionEntry entries[] = {
    { "force-new-user", 0, 0, G_OPTION_ARG_NONE, &force_new_user_mode,
      _("Force new user mode"), NULL },
    { NULL }
  };

  context = g_option_context_new (_("- GNOME initial setup"));
  g_option_context_add_main_entries (context, entries, NULL);

  g_option_context_parse (context, &argc, &argv, NULL);

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

#ifdef HAVE_CHEESE
  cheese_gtk_init (NULL, NULL);
#endif

  gtk_init (&argc, &argv);

#if HAVE_CLUTTER
  if (gtk_clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS) {
    g_critical ("Clutter-GTK init failed");
    exit (1);
  }
#endif

  g_type_ensure (EGG_TYPE_LIST_BOX);

  driver = gis_driver_new (get_mode ());
  g_signal_connect (driver, "rebuild-pages", G_CALLBACK (rebuild_pages_cb), NULL);
  status = g_application_run (G_APPLICATION (driver), argc, argv);

  g_object_unref (driver);
  g_option_context_free (context);
  return status;
}

/* Epilogue {{{1 */
/* vim: set foldmethod=marker: */
