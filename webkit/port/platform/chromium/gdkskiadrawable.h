/* Based on GTK code by the Chromium Authors. The original header for that code
 * continues below */

/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GDK_SKIA_H__
#define __GDK_SKIA_H__

#include <gdk/gdk.h>
#include <cairo/cairo.h>

class SkCanvas;

G_BEGIN_DECLS

typedef struct _GdkSkiaObject GdkSkiaObject;
typedef struct _GdkSkiaObjectClass GdkSkiaObjectClass;

typedef struct _GdkDrawable GdkSkia;

#define GDK_TYPE_SKIA              (gdk_skia_get_type ())
#define GDK_SKIA(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_SKIA, GdkSkia))
#define GDK_SKIA_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_SKIA, GdkSkiaObjectClass))
#define GDK_IS_SKIA(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_SKIA))
#define GDK_IS_SKIA_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_SKIA))
#define GDK_SKIA_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_SKIA, GdkSkiaObjectClass))
#define GDK_SKIA_OBJECT(object)    ((GdkSkiaObject *) GDK_SKIA (object))

struct _GdkSkiaObject
{
  GdkDrawable parent_instance;
  SkCanvas *canvas;
  cairo_surface_t *surface;
};

struct _GdkSkiaObjectClass
{
  GdkDrawableClass parent_class;
};

GType gdk_skia_get_type();

// -----------------------------------------------------------------------------
// Return a new GdkSkia for the given canvas.
// -----------------------------------------------------------------------------
GdkSkia* gdk_skia_new(SkCanvas* canvas);

G_END_DECLS

#endif /* __GDK_SKIA_H__ */
