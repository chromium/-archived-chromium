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

#include <stdio.h>

#include <gdk/gdk.h>
#include <SkCanvas.h>
#include <SkBitmap.h>
#include <SkDevice.h>

#include "GdkSkia.h"

static GdkGC *gdk_skia_create_gc      (GdkDrawable     *drawable,
                                       GdkGCValues     *values,
                                       GdkGCValuesMask  mask);
static void   gdk_skia_draw_rectangle (GdkDrawable     *drawable,
                                       GdkGC           *gc,
                                       gboolean         filled,
                                       gint             x,
                                       gint             y,
                                       gint             width,
                                       gint             height);
static void   gdk_skia_draw_arc       (GdkDrawable     *drawable,
                                       GdkGC           *gc,
                                       gboolean         filled,
                                       gint             x,
                                       gint             y,
                                       gint             width,
                                       gint             height,
                                       gint             angle1,
                                       gint             angle2);
static void   gdk_skia_draw_polygon   (GdkDrawable     *drawable,
                                       GdkGC           *gc,
                                       gboolean         filled,
                                       GdkPoint        *points,
                                       gint             npoints);
static void   gdk_skia_draw_text      (GdkDrawable     *drawable,
                                       GdkFont         *font,
                                       GdkGC           *gc,
                                       gint             x,
                                       gint             y,
                                       const gchar     *text,
                                       gint             text_length);
static void   gdk_skia_draw_text_wc   (GdkDrawable     *drawable,
                                       GdkFont         *font,
                                       GdkGC           *gc,
                                       gint             x,
                                       gint             y,
                                       const GdkWChar  *text,
                                       gint             text_length);
static void   gdk_skia_draw_drawable  (GdkDrawable     *drawable,
                                       GdkGC           *gc,
                                       GdkPixmap       *src,
                                       gint             xsrc,
                                       gint             ysrc,
                                       gint             xdest,
                                       gint             ydest,
                                       gint             width,
                                       gint             height);
static void   gdk_skia_draw_points    (GdkDrawable     *drawable,
                                       GdkGC           *gc,
                                       GdkPoint        *points,
                                       gint             npoints);
static void   gdk_skia_draw_segments  (GdkDrawable     *drawable,
                                       GdkGC           *gc,
                                       GdkSegment      *segs,
                                       gint             nsegs);
static void   gdk_skia_draw_lines     (GdkDrawable     *drawable,
                                       GdkGC           *gc,
                                       GdkPoint        *points,
                                       gint             npoints);

static void gdk_skia_draw_glyphs             (GdkDrawable      *drawable,
                                              GdkGC            *gc,
                                              PangoFont        *font,
                                              gint              x,
                                              gint              y,
                                              PangoGlyphString *glyphs);
static void gdk_skia_draw_glyphs_transformed (GdkDrawable      *drawable,
                                              GdkGC            *gc,
                                              PangoMatrix      *matrix,
                                              PangoFont        *font,
                                              gint              x,
                                              gint              y,
                                              PangoGlyphString *glyphs);

static void   gdk_skia_draw_image     (GdkDrawable     *drawable,
                                       GdkGC           *gc,
                                       GdkImage        *image,
                                       gint             xsrc,
                                       gint             ysrc,
                                       gint             xdest,
                                       gint             ydest,
                                       gint             width,
                                       gint             height);
static void   gdk_skia_draw_pixbuf    (GdkDrawable     *drawable,
                                       GdkGC           *gc,
                                       GdkPixbuf       *pixbuf,
                                       gint             src_x,
                                       gint             src_y,
                                       gint             dest_x,
                                       gint             dest_y,
                                       gint             width,
                                       gint             height,
                                       GdkRgbDither     dither,
                                       gint             x_dither,
                                       gint             y_dither);
static void  gdk_skia_draw_trapezoids (GdkDrawable     *drawable,
                                       GdkGC	         *gc,
                                       GdkTrapezoid    *trapezoids,
                                       gint             n_trapezoids);

static void   gdk_skia_real_get_size  (GdkDrawable     *drawable,
                                       gint            *width,
                                       gint            *height);

static GdkImage* gdk_skia_copy_to_image (GdkDrawable *drawable,
                                         GdkImage    *image,
                                         gint         src_x,
                                         gint         src_y,
                                         gint         dest_x,
                                         gint         dest_y,
                                         gint         width,
                                         gint         height);

static cairo_surface_t *gdk_skia_ref_cairo_surface (GdkDrawable *drawable);

static GdkVisual*   gdk_skia_real_get_visual   (GdkDrawable *drawable);
static gint         gdk_skia_real_get_depth    (GdkDrawable *drawable);
static void         gdk_skia_real_set_colormap (GdkDrawable *drawable,
                                                  GdkColormap *cmap);
static GdkColormap* gdk_skia_real_get_colormap (GdkDrawable *drawable);
static GdkScreen*   gdk_skia_real_get_screen   (GdkDrawable *drawable);
static void gdk_skia_init       (GdkSkiaObject      *skia);
static void gdk_skia_class_init (GdkSkiaObjectClass *klass);
static void gdk_skia_finalize   (GObject              *object);

static gpointer parent_class = NULL;

// -----------------------------------------------------------------------------
// Usually GDK code is C code. However, since we are interfacing to a C++
// library, we must compile in C++ mode to parse its headers etc. Thankfully,
// these are the only non-static symbol in the file so we can just wrap them in
// an extern decl to disable name mangling and everything should be happy.
// -----------------------------------------------------------------------------
extern "C" {
GType
gdk_skia_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    object_type = g_type_register_static_simple (GDK_TYPE_DRAWABLE,
						 "GdkSkia",
						 sizeof (GdkSkiaObjectClass),
						 (GClassInitFunc) gdk_skia_class_init,
						 sizeof (GdkSkiaObject),
						 (GInstanceInitFunc) gdk_skia_init,
						 (GTypeFlags) 0);

  return object_type;
}

GdkSkia*
gdk_skia_new(SkCanvas *canvas)
{
  GdkSkia *skia = GDK_SKIA(g_object_new (GDK_TYPE_SKIA, NULL));
  reinterpret_cast<GdkSkiaObject*>(skia)->canvas = canvas;
  return skia;
}

}  // extern "C"

static void
gdk_skia_init (GdkSkiaObject *skia)
{
  /* 0 initialization is fine for us */
}

static void
gdk_skia_class_init (GdkSkiaObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_skia_finalize;

  drawable_class->create_gc = gdk_skia_create_gc;
  drawable_class->draw_rectangle = gdk_skia_draw_rectangle;
  drawable_class->draw_arc = gdk_skia_draw_arc;
  drawable_class->draw_polygon = gdk_skia_draw_polygon;
  drawable_class->draw_text = gdk_skia_draw_text;
  drawable_class->draw_text_wc = gdk_skia_draw_text_wc;
  drawable_class->draw_drawable = gdk_skia_draw_drawable;
  drawable_class->draw_points = gdk_skia_draw_points;
  drawable_class->draw_segments = gdk_skia_draw_segments;
  drawable_class->draw_lines = gdk_skia_draw_lines;
  drawable_class->draw_glyphs = gdk_skia_draw_glyphs;
  drawable_class->draw_glyphs_transformed = gdk_skia_draw_glyphs_transformed;
  drawable_class->draw_image = gdk_skia_draw_image;
  drawable_class->draw_pixbuf = gdk_skia_draw_pixbuf;
  drawable_class->draw_trapezoids = gdk_skia_draw_trapezoids;
  drawable_class->get_depth = gdk_skia_real_get_depth;
  drawable_class->get_screen = gdk_skia_real_get_screen;
  drawable_class->get_size = gdk_skia_real_get_size;
  drawable_class->set_colormap = gdk_skia_real_set_colormap;
  drawable_class->get_colormap = gdk_skia_real_get_colormap;
  drawable_class->get_visual = gdk_skia_real_get_visual;
  drawable_class->_copy_to_image = gdk_skia_copy_to_image;
  drawable_class->ref_cairo_surface = gdk_skia_ref_cairo_surface;
}

static void
gdk_skia_finalize (GObject *object)
{
  GdkSkiaObject *const skia = (GdkSkiaObject *) object;
  if (skia->surface)
    cairo_surface_destroy(skia->surface);
  G_OBJECT_CLASS (parent_class)->finalize(object);
}

#define NOTIMPLEMENTED fprintf(stderr, "GDK Skia not implemented: %s\n", __PRETTY_FUNCTION__)

static GdkGC *
gdk_skia_create_gc(GdkDrawable     *drawable,
                   GdkGCValues     *values,
                   GdkGCValuesMask  mask) {
  NOTIMPLEMENTED;
  return NULL;
}

static void
gc_set_paint(GdkGC *gc, SkPaint *paint) {
  GdkGCValues values;
  gdk_gc_get_values(gc, &values);

  paint->setARGB(255,
                 values.foreground.pixel >> 16,
                 values.foreground.pixel >> 8,
                 values.foreground.pixel);
  paint->setStrokeWidth(values.line_width);
}

static void
gdk_skia_draw_rectangle(GdkDrawable     *drawable,
                        GdkGC           *gc,
                        gboolean         filled,
                        gint             x,
                        gint             y,
                        gint             width,
                        gint             height) {
  GdkSkiaObject *skia = (GdkSkiaObject *) drawable;
  SkPaint paint;
  gc_set_paint(gc, &paint);

  if (filled) {
    paint.setStyle(SkPaint::kFill_Style);
  } else {
    paint.setStyle(SkPaint::kStroke_Style);
  }

  SkRect rect;
  rect.set(x, y, x + width, y + height);

  skia->canvas->drawRect(rect, paint);
}

static void
gdk_skia_draw_arc(GdkDrawable     *drawable,
                  GdkGC           *gc,
                  gboolean         filled,
                  gint             x,
                  gint             y,
                  gint             width,
                  gint             height,
                  gint             angle1,
                  gint             angle2) {
  NOTIMPLEMENTED;
}

static void
gdk_skia_draw_polygon(GdkDrawable     *drawable,
                      GdkGC           *gc,
                      gboolean         filled,
                      GdkPoint        *points,
                      gint             npoints) {
  NOTIMPLEMENTED;
}

static void
gdk_skia_draw_text(GdkDrawable     *drawable,
                   GdkFont         *font,
                   GdkGC           *gc,
                   gint             x,
                   gint             y,
                   const gchar     *text,
                   gint             text_length) {
  NOTIMPLEMENTED;
}

static void
gdk_skia_draw_text_wc(GdkDrawable     *drawable,
                      GdkFont         *font,
                      GdkGC           *gc,
                      gint             x,
                      gint             y,
                      const GdkWChar  *text,
                      gint             text_length) {
  NOTIMPLEMENTED;
}

static void
gdk_skia_draw_drawable(GdkDrawable     *drawable,
                       GdkGC           *gc,
                       GdkPixmap       *src,
                       gint             xsrc,
                       gint             ysrc,
                       gint             xdest,
                       gint             ydest,
                       gint             width,
                       gint             height) {
  NOTIMPLEMENTED;
}

static void
gdk_skia_draw_points(GdkDrawable     *drawable,
                     GdkGC           *gc,
                     GdkPoint        *points,
                     gint             npoints) {
  NOTIMPLEMENTED;
}

static void
gdk_skia_draw_segments(GdkDrawable     *drawable,
                       GdkGC           *gc,
                       GdkSegment      *segs,
                       gint             nsegs) {
  NOTIMPLEMENTED;
}

static void
gdk_skia_draw_lines(GdkDrawable     *drawable,
                    GdkGC           *gc,
                    GdkPoint        *points,
                    gint             npoints) {
  NOTIMPLEMENTED;
}

static void
gdk_skia_draw_glyphs(GdkDrawable      *drawable,
                     GdkGC            *gc,
                     PangoFont        *font,
                     gint              x,
                     gint              y,
                     PangoGlyphString *glyphs) {
  NOTIMPLEMENTED;
}

static void
gdk_skia_draw_glyphs_transformed(GdkDrawable      *drawable,
                                 GdkGC            *gc,
                                 PangoMatrix      *matrix,
                                 PangoFont        *font,
                                 gint              x,
                                 gint              y,
                                 PangoGlyphString *glyphs) {
  NOTIMPLEMENTED;
}

static void
gdk_skia_draw_image(GdkDrawable     *drawable,
                    GdkGC           *gc,
                    GdkImage        *image,
                    gint             xsrc,
                    gint             ysrc,
                    gint             xdest,
                    gint             ydest,
                    gint             width,
                    gint             height) {
  NOTIMPLEMENTED;
}

static void
gdk_skia_draw_pixbuf(GdkDrawable     *drawable,
                     GdkGC           *gc,
                     GdkPixbuf       *pixbuf,
                     gint             src_x,
                     gint             src_y,
                     gint             dest_x,
                     gint             dest_y,
                     gint             width,
                     gint             height,
                     GdkRgbDither     dither,
                     gint             x_dither,
                     gint             y_dither) {
  NOTIMPLEMENTED;
}

static void
gdk_skia_draw_trapezoids(GdkDrawable     *drawable,
                         GdkGC	         *gc,
                         GdkTrapezoid    *trapezoids,
                         gint             n_trapezoids) {
  NOTIMPLEMENTED;
}

static void
gdk_skia_real_get_size(GdkDrawable     *drawable,
                       gint            *width,
                       gint            *height) {
  GdkSkiaObject *const skia = (GdkSkiaObject *) drawable;
  SkDevice *const dev = skia->canvas->getDevice();
  *width = dev->width();
  *height = dev->height();
}

static GdkImage*
gdk_skia_copy_to_image(GdkDrawable *drawable,
                       GdkImage    *image,
                       gint         src_x,
                       gint         src_y,
                       gint         dest_x,
                       gint         dest_y,
                       gint         width,
                       gint         height) {
  NOTIMPLEMENTED;
  return NULL;
}

static cairo_surface_t *
gdk_skia_ref_cairo_surface(GdkDrawable *drawable) {
  GdkSkiaObject *const skia = (GdkSkiaObject *) drawable;

  if (!skia->surface) {
    SkDevice *const dev = skia->canvas->getDevice();
    const SkBitmap *const bm = &dev->accessBitmap(true);

    skia->surface = cairo_image_surface_create_for_data
      ((unsigned char *) bm->getPixels(),
       CAIRO_FORMAT_ARGB32, dev->width(), dev->height(), bm->rowBytes());
  }

  SkMatrix matrix = skia->canvas->getTotalMatrix();
  int x_shift = SkScalarRound(matrix.getTranslateX());
  int y_shift = SkScalarRound(matrix.getTranslateY());
  cairo_surface_set_device_offset(skia->surface, x_shift, y_shift);

  return cairo_surface_reference(skia->surface);
}

static GdkVisual*
gdk_skia_real_get_visual(GdkDrawable *drawable) {
  NOTIMPLEMENTED;
  return NULL;
}

static gint
gdk_skia_real_get_depth(GdkDrawable *drawable) {
  GdkSkiaObject *skia = (GdkSkiaObject *) drawable;
  const SkBitmap::Config config = skia->canvas->getDevice()->config();

  switch (config) {
    case SkBitmap::kARGB_8888_Config:
      return 24;
    default:
      // NOTREACHED
      *reinterpret_cast<char*>(NULL) = 0;
      return 0;
  }
}

static void
gdk_skia_real_set_colormap(GdkDrawable *drawable,
                           GdkColormap *cmap) {
  NOTIMPLEMENTED;
}

static GdkColormap*
gdk_skia_real_get_colormap(GdkDrawable *drawable) {
  NOTIMPLEMENTED;
  return NULL;
}

static GdkScreen*
gdk_skia_real_get_screen(GdkDrawable *drawable) {
  NOTIMPLEMENTED;
  return NULL;
}
