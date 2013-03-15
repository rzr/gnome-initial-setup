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

#ifndef _GIS_CONNMAN_PAGE_H
#define _GIS_CONNMAN_PAGE_H

#include "gnome-initial-setup.h"

G_BEGIN_DECLS

#define GIS_TYPE_CONNMAN_PAGE gis_connman_page_get_type()

#define GIS_CONNMAN_PAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  GIS_TYPE_CONNMAN_PAGE, GisConnmanPage))

#define GIS_CONNMAN_PAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  GIS_TYPE_CONNMAN_PAGE, GisConnmanPageClass))

#define GIS_IS_CONNMAN_PAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  GIS_TYPE_CONNMAN_PAGE))

#define GIS_IS_CONNMAN_PAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  GIS_TYPE_CONNMAN_PAGE))

#define GIS_CONNMAN_PAGE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  GIS_TYPE_CONNMAN_PAGE, GisConnmanPageClass))

typedef struct _GisConnmanPage GisConnmanPage;
typedef struct _GisConnmanPageClass GisConnmanPageClass;
typedef struct _GisConnmanPagePrivate GisConnmanPagePrivate;

struct _GisConnmanPage
{
  GisPage parent;

  GisConnmanPagePrivate *priv;
};

struct _GisConnmanPageClass
{
  GisPageClass parent_class;
};

GType gis_connman_page_get_type (void) G_GNUC_CONST;

void  gis_prepare_connman_page (GisDriver *driver);

G_END_DECLS

#endif /* _GIS_CONNMAN_PAGE_H */
