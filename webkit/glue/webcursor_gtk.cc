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
    case PlatformCursor::TypePointer:
      return GDK_LAST_CURSOR;
    case PlatformCursor::TypeCross:
      return GDK_CROSS;
    case PlatformCursor::TypeHand:
      return GDK_HAND2;
    case PlatformCursor::TypeIBeam:
      return GDK_XTERM;
    case PlatformCursor::TypeWait:
      return GDK_WATCH;
    case PlatformCursor::TypeHelp:
      return GDK_QUESTION_ARROW;
    case PlatformCursor::TypeEastResize:
      return GDK_RIGHT_SIDE;
    case PlatformCursor::TypeNorthResize:
      return GDK_TOP_SIDE;
    case PlatformCursor::TypeNorthEastResize:
      return GDK_TOP_RIGHT_CORNER;
    case PlatformCursor::TypeNorthWestResize:
      return GDK_TOP_LEFT_CORNER;
    case PlatformCursor::TypeSouthResize:
      return GDK_BOTTOM_SIDE;
    case PlatformCursor::TypeSouthEastResize:
      return GDK_BOTTOM_LEFT_CORNER;
    case PlatformCursor::TypeSouthWestResize:
      return GDK_BOTTOM_RIGHT_CORNER;
    case PlatformCursor::TypeWestResize:
      return GDK_LEFT_SIDE;
    case PlatformCursor::TypeNorthSouthResize:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeEastWestResize:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeNorthEastSouthWestResize:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeNorthWestSouthEastResize:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeColumnResize:
      return GDK_SB_H_DOUBLE_ARROW;  // TODO(evanm): is this correct?
    case PlatformCursor::TypeRowResize:
      return GDK_SB_V_DOUBLE_ARROW;  // TODO(evanm): is this correct?
    case PlatformCursor::TypeMiddlePanning:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeEastPanning:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeNorthPanning:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeNorthEastPanning:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeNorthWestPanning:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeSouthPanning:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeSouthEastPanning:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeSouthWestPanning:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeWestPanning:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeMove:
      return GDK_FLEUR;
    case PlatformCursor::TypeVerticalText:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeCell:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeContextMenu:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeAlias:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeProgress:
      return GDK_WATCH;
    case PlatformCursor::TypeNoDrop:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeCopy:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeNone:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeNotAllowed:
      NOTIMPLEMENTED(); return GDK_LAST_CURSOR;
    case PlatformCursor::TypeZoomIn:
    case PlatformCursor::TypeZoomOut:
    case PlatformCursor::TypeCustom:
      return GDK_CURSOR_IS_PIXMAP;
  }
  NOTREACHED();
  return GDK_LAST_CURSOR;
}

GdkCursor* WebCursor::GetCustomCursor() const {
  switch (type_) {
    case PlatformCursor::TypeZoomIn:
      return GetInlineCustomCursor(CustomCursorZoomIn);
    case PlatformCursor::TypeZoomOut:
      return GetInlineCustomCursor(CustomCursorZoomOut);
  }

  if (type_ != PlatformCursor::TypeCustom) {
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
