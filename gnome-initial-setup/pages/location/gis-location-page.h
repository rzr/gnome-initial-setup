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

#ifndef __GIS_LOCATION_PAGE_H__
#define __GIS_LOCATION_PAGE_H__

#include <glib-object.h>

#include "gnome-initial-setup.h"

G_BEGIN_DECLS

#define GIS_TYPE_LOCATION_PAGE               (gis_location_page_get_type ())
#define GIS_LOCATION_PAGE(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_LOCATION_PAGE, GisLocationPage))
#define GIS_LOCATION_PAGE_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_LOCATION_PAGE, GisLocationPageClass))
#define GIS_IS_LOCATION_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_LOCATION_PAGE))
#define GIS_IS_LOCATION_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_LOCATION_PAGE))
#define GIS_LOCATION_PAGE_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_LOCATION_PAGE, GisLocationPageClass))

typedef struct _GisLocationPage        GisLocationPage;
typedef struct _GisLocationPageClass   GisLocationPageClass;
typedef struct _GisLocationPagePrivate GisLocationPagePrivate;

struct _GisLocationPage
{
  GisPage parent;

  GisLocationPagePrivate *priv;
};

struct _GisLocationPageClass
{
  GisPageClass parent_class;
};

GType gis_location_page_get_type (void);

const gchar *gis_location_page_get_id ();

void gis_prepare_location_page (GisDriver *driver);

G_END_DECLS

#endif /* __GIS_LOCATION_PAGE_H__ */
