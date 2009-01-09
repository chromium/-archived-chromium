// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "webkit/glue/webcursor.h"

#include <gdk/gdk.h>

#include "config.h"
#include "PlatformCursor.h"

#undef LOG
#include "base/logging.h"

using WebCore::PlatformCursor;

namespace {

// webcursor_gtk_data.h is taken directly from WebKit's CursorGtk.h.
#include "webkit/glue/webcursor_gtk_data.h"

// This helper function is taken directly from WebKit's CursorGtk.cpp.
// It attempts to create a custom cursor from the data inlined in
// webcursor_gtk_data.h.
GdkCursor* GetInlineCustomCursor(CustomCursorType type) {
  const CustomCursor& custom = CustomCursors[type];
  GdkCursor* cursor = gdk_cursor_new_from_name(gdk_display_get_default(),
                                               custom.name);
  if (!cursor) {
    const GdkColor fg = { 0, 0, 0, 0 };
    const GdkColor bg = { 65535, 65535, 65535, 65535 };
    GdkPixmap* source = gdk_bitmap_create_from_data(NULL, custom.bits,
                                                    32, 32);
    GdkPixmap* mask = gdk_bitmap_create_from_data(NULL, custom.mask_bits,
                                                  32, 32);
    cursor = gdk_cursor_new_from_pixmap(source, mask, &fg, &bg,
                                        custom.hot_x, custom.hot_y);
    g_object_unref(source);
    g_object_unref(mask);
  }
  return cursor;
}

}  // end anonymous namespace

GdkCursorType WebCursor::GetCursorType() const {
  // http://library.gnome.org/devel/gdk/2.12/gdk-Cursors.html has images
  // of the default X theme, but beware that the user's cursor theme can
  // change everything.
  switch (type_) {
    case PlatformCursor::typePointer:
      return GDK_ARROW;
    case PlatformCursor::typeCross:
      return GDK_CROSS;
    case PlatformCursor::typeHand:
      return GDK_HAND2;
    case PlatformCursor::typeIBeam:
      return GDK_XTERM;
    case PlatformCursor::typeWait:
      return GDK_WATCH;
    case PlatformCursor::typeHelp:
      return GDK_QUESTION_ARROW;
    case PlatformCursor::typeEastResize:
      return GDK_RIGHT_SIDE;
    case PlatformCursor::typeNorthResize:
      return GDK_TOP_SIDE;
    case PlatformCursor::typeNorthEastResize:
      return GDK_TOP_RIGHT_CORNER;
    case PlatformCursor::typeNorthWestResize:
      return GDK_TOP_LEFT_CORNER;
    case PlatformCursor::typeSouthResize:
      return GDK_BOTTOM_SIDE;
    case PlatformCursor::typeSouthEastResize:
      return GDK_BOTTOM_LEFT_CORNER;
    case PlatformCursor::typeSouthWestResize:
      return GDK_BOTTOM_RIGHT_CORNER;
    case PlatformCursor::typeWestResize:
      return GDK_LEFT_SIDE;
    case PlatformCursor::typeNorthSouthResize:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeEastWestResize:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeNorthEastSouthWestResize:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeNorthWestSouthEastResize:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeColumnResize:
      return GDK_SB_H_DOUBLE_ARROW;  // TODO(evanm): is this correct?
    case PlatformCursor::typeRowResize:
      return GDK_SB_V_DOUBLE_ARROW;  // TODO(evanm): is this correct?
    case PlatformCursor::typeMiddlePanning:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeEastPanning:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeNorthPanning:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeNorthEastPanning:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeNorthWestPanning:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeSouthPanning:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeSouthEastPanning:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeSouthWestPanning:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeWestPanning:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeMove:
      return GDK_FLEUR;
    case PlatformCursor::typeVerticalText:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeCell:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeContextMenu:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeAlias:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeProgress:
      return GDK_WATCH;
    case PlatformCursor::typeNoDrop:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeCopy:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeNone:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeNotAllowed:
      NOTIMPLEMENTED(); return GDK_ARROW;
    case PlatformCursor::typeZoomIn:
    case PlatformCursor::typeZoomOut:
    case PlatformCursor::typeCustom:
      return GDK_CURSOR_IS_PIXMAP;
  }
  NOTREACHED();
  return GDK_ARROW;
}

GdkCursor* WebCursor::GetCustomCursor() const {
  switch (type_) {
    case PlatformCursor::typeZoomIn:
      return GetInlineCustomCursor(CustomCursorZoomIn);
    case PlatformCursor::typeZoomOut:
      return GetInlineCustomCursor(CustomCursorZoomOut);
  }

  if (type_ != PlatformCursor::typeCustom) {
    NOTREACHED();
    return NULL;
  }

  const guchar* data = reinterpret_cast<const guchar*>(&custom_data_[0]);
  GdkPixbuf* pixbuf =
      gdk_pixbuf_new_from_data(data,
                               GDK_COLORSPACE_RGB,
                               TRUE,  // has_alpha
                               8,     // bits_per_sample
                               custom_size_.width(),      // width
                               custom_size_.height(),     // height
                               custom_size_.width() * 4,  // row stride
                               NULL,   // data destroy function
                               NULL);  // data destroy function extra data

  GdkCursor* cursor = gdk_cursor_new_from_pixbuf(gdk_display_get_default(),
                                                 pixbuf,
                                                 hotspot_.x(),
                                                 hotspot_.y());

  gdk_pixbuf_unref(pixbuf);

  return cursor;
}

void WebCursor::InitPlatformData() {
  return;
}

bool WebCursor::SerializePlatformData(Pickle* pickle) const {
  return true;
}

bool WebCursor::DeserializePlatformData(const Pickle* pickle, void** iter) {
  return true;
}

bool WebCursor::IsPlatformDataEqual(const WebCursor& other) const {
  return true;
}

void WebCursor::CleanupPlatformData() {
  return;
}

void WebCursor::CopyPlatformData(const WebCursor& other) {
  return;
}
