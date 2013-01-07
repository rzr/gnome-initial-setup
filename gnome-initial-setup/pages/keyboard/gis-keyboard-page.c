/*
 * Copyright (C) 2012 Intel, Inc
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
 * Author: Michael Wood <michael.g.wood@intel.com>
 * Based on gnome-control-center region page by:
 *  Sergey Udaltsov <svu@gnome.org>
 *
 */

#define PAGE_ID "keyboard"

#include "config.h"
#include "gis-keyboard-page.h"
#include <gtk/gtk.h>

#include "gnome-region-panel-input.h"

G_DEFINE_TYPE (GisKeyboardPage, gis_keyboard_page, GIS_TYPE_PAGE)

#define KEYBOARD_PAGE_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GIS_TYPE_KEYBOARD_PAGE, GisKeyboardPagePrivate))

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE(page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

struct _GisKeyboardPagePrivate {
};

static GtkBuilder *
gis_keyboard_page_get_builder (GisPage *page)
{
  GtkBuilder *builder = gtk_builder_new ();
  GError *error = NULL;
  const char *resource_path = "/ui/gis-" PAGE_ID "-page.ui";

  gtk_builder_add_from_resource (builder, resource_path, &error);

  if (error != NULL)
    g_error ("Error while loading %s: %s", resource_path, error->message);

  setup_input_tabs (builder, GIS_KEYBOARD_PAGE (page));

  return builder;
}


static void
gis_keyboard_page_constructed (GObject *object)
{
  GisKeyboardPage *page = GIS_KEYBOARD_PAGE (object);

  G_OBJECT_CLASS (gis_keyboard_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID("keyboard-page"));

  gis_page_set_title (GIS_PAGE (page), _("Set keyboard layout"));
  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_keyboard_page_class_init (GisKeyboardPageClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GisPageClass * page_class = GIS_PAGE_CLASS (klass);

  //object_class->set_property = gis_region_page_set_property;
  //object_class->finalize = gis_region_page_finalize;
  page_class->page_id = PAGE_ID;
  page_class->get_builder = gis_keyboard_page_get_builder;
  object_class->constructed = gis_keyboard_page_constructed;

  g_type_class_add_private (klass, sizeof (GisKeyboardPagePrivate));
}

static void
gis_keyboard_page_init (GisKeyboardPage * self)
{
  GisKeyboardPagePrivate *priv;
  priv = self->priv = KEYBOARD_PAGE_PRIVATE (self);
}

void
gis_prepare_keyboard_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_KEYBOARD_PAGE,
                                     "driver", driver,
                                     NULL));
}

const gchar *
gis_keyboard_page_get_id ()
{
    return PAGE_ID;
}
