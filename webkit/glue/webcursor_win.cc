// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "base/gfx/gdi_util.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "grit/webkit_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/api/public/WebCursorInfo.h"
#include "webkit/glue/webcursor.h"

using WebKit::WebCursorInfo;

static LPCWSTR ToCursorID(WebCursorInfo::Type type) {
  switch (type) {
    case WebCursorInfo::TypePointer:
      return IDC_ARROW;
    case WebCursorInfo::TypeCross:
      return IDC_CROSS;
    case WebCursorInfo::TypeHand:
      return IDC_HAND;
    case WebCursorInfo::TypeIBeam:
      return IDC_IBEAM;
    case WebCursorInfo::TypeWait:
      return IDC_WAIT;
    case WebCursorInfo::TypeHelp:
      return IDC_HELP;
    case WebCursorInfo::TypeEastResize:
      return IDC_SIZEWE;
    case WebCursorInfo::TypeNorthResize:
      return IDC_SIZENS;
    case WebCursorInfo::TypeNorthEastResize:
      return IDC_SIZENESW;
    case WebCursorInfo::TypeNorthWestResize:
      return IDC_SIZENWSE;
    case WebCursorInfo::TypeSouthResize:
      return IDC_SIZENS;
    case WebCursorInfo::TypeSouthEastResize:
      return IDC_SIZENWSE;
    case WebCursorInfo::TypeSouthWestResize:
      return IDC_SIZENESW;
    case WebCursorInfo::TypeWestResize:
      return IDC_SIZEWE;
    case WebCursorInfo::TypeNorthSouthResize:
      return IDC_SIZENS;
    case WebCursorInfo::TypeEastWestResize:
      return IDC_SIZEWE;
    case WebCursorInfo::TypeNorthEastSouthWestResize:
      return IDC_SIZENESW;
    case WebCursorInfo::TypeNorthWestSouthEastResize:
      return IDC_SIZENWSE;
    case WebCursorInfo::TypeColumnResize:
      return MAKEINTRESOURCE(IDC_COLRESIZE);
    case WebCursorInfo::TypeRowResize:
      return MAKEINTRESOURCE(IDC_ROWRESIZE);
    case WebCursorInfo::TypeMiddlePanning:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE);
    case WebCursorInfo::TypeEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_EAST);
    case WebCursorInfo::TypeNorthPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH);
    case WebCursorInfo::TypeNorthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_EAST);
    case WebCursorInfo::TypeNorthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_WEST);
    case WebCursorInfo::TypeSouthPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH);
    case WebCursorInfo::TypeSouthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_EAST);
    case WebCursorInfo::TypeSouthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_WEST);
    case WebCursorInfo::TypeWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_WEST);
    case WebCursorInfo::TypeMove:
      return IDC_SIZEALL;
    case WebCursorInfo::TypeVerticalText:
      return MAKEINTRESOURCE(IDC_VERTICALTEXT);
    case WebCursorInfo::TypeCell:
      return MAKEINTRESOURCE(IDC_CELL);
    case WebCursorInfo::TypeContextMenu:
      return MAKEINTRESOURCE(IDC_ARROW);
    case WebCursorInfo::TypeAlias:
      return MAKEINTRESOURCE(IDC_ALIAS);
    case WebCursorInfo::TypeProgress:
      return IDC_APPSTARTING;
    case WebCursorInfo::TypeNoDrop:
      return IDC_NO;
    case WebCursorInfo::TypeCopy:
      return MAKEINTRESOURCE(IDC_COPYCUR);
    case WebCursorInfo::TypeNone:
      return IDC_ARROW;
    case WebCursorInfo::TypeNotAllowed:
      return IDC_NO;
    case WebCursorInfo::TypeZoomIn:
      return MAKEINTRESOURCE(IDC_ZOOMIN);
    case WebCursorInfo::TypeZoomOut:
      return MAKEINTRESOURCE(IDC_ZOOMOUT);
  }
  NOTREACHED();
  return NULL;
}

static bool IsSystemCursorID(LPCWSTR cursor_id) {
  return cursor_id >= IDC_ARROW;  // See WinUser.h
}

static WebCursorInfo::Type ToCursorType(HCURSOR cursor) {
  static struct {
    HCURSOR cursor;
    WebCursorInfo::Type type;
  } kStandardCursors[] = {
    { LoadCursor(NULL, IDC_ARROW),       WebCursorInfo::TypePointer },
    { LoadCursor(NULL, IDC_IBEAM),       WebCursorInfo::TypeIBeam },
    { LoadCursor(NULL, IDC_WAIT),        WebCursorInfo::TypeWait },
    { LoadCursor(NULL, IDC_CROSS),       WebCursorInfo::TypeCross },
    { LoadCursor(NULL, IDC_SIZENWSE),    WebCursorInfo::TypeNorthWestResize },
    { LoadCursor(NULL, IDC_SIZENESW),    WebCursorInfo::TypeNorthEastResize },
    { LoadCursor(NULL, IDC_SIZEWE),      WebCursorInfo::TypeEastWestResize },
    { LoadCursor(NULL, IDC_SIZENS),      WebCursorInfo::TypeNorthSouthResize },
    { LoadCursor(NULL, IDC_SIZEALL),     WebCursorInfo::TypeMove },
    { LoadCursor(NULL, IDC_NO),          WebCursorInfo::TypeNotAllowed },
    { LoadCursor(NULL, IDC_HAND),        WebCursorInfo::TypeHand },
    { LoadCursor(NULL, IDC_APPSTARTING), WebCursorInfo::TypeProgress },
    { LoadCursor(NULL, IDC_HELP),        WebCursorInfo::TypeHelp },
  };
  for (int i = 0; i < arraysize(kStandardCursors); i++) {
    if (cursor == kStandardCursors[i].cursor)
      return kStandardCursors[i].type;
  }
  return WebCursorInfo::TypeCustom;
}

HCURSOR WebCursor::GetCursor(HINSTANCE module_handle){
  if (!IsCustom()) {
    const wchar_t* cursor_id =
        ToCursorID(static_cast<WebCursorInfo::Type>(type_));

    if (IsSystemCursorID(cursor_id))
      module_handle = NULL;

    return LoadCursor(module_handle, cursor_id);
  }

  if (custom_cursor_) {
    DCHECK(external_cursor_ == NULL);
    return custom_cursor_;
  }

  if (external_cursor_)
    return external_cursor_;

  BITMAPINFO cursor_bitmap_info = {0};
  gfx::CreateBitmapHeader(
      custom_size_.width(), custom_size_.height(),
      reinterpret_cast<BITMAPINFOHEADER*>(&cursor_bitmap_info));
  HDC dc = GetDC(0);
  HDC workingDC = CreateCompatibleDC(dc);
  HBITMAP bitmap_handle = CreateDIBSection(
      dc, &cursor_bitmap_info, DIB_RGB_COLORS, 0, 0, 0);
  SetDIBits(
      0, bitmap_handle, 0, custom_size_.height(), &custom_data_[0],
      &cursor_bitmap_info, DIB_RGB_COLORS);

  HBITMAP old_bitmap = reinterpret_cast<HBITMAP>(
      SelectObject(workingDC, bitmap_handle));
  SetBkMode(workingDC, TRANSPARENT);
  SelectObject(workingDC, old_bitmap);

  HBITMAP mask = CreateBitmap(
      custom_size_.width(), custom_size_.height(), 1, 1, NULL);
  ICONINFO ii = {0};
  ii.fIcon = FALSE;
  ii.xHotspot = hotspot_.x();
  ii.yHotspot = hotspot_.y();
  ii.hbmMask = mask;
  ii.hbmColor = bitmap_handle;

  custom_cursor_ = CreateIconIndirect(&ii);

  DeleteObject(mask);
  DeleteObject(bitmap_handle);
  DeleteDC(workingDC);
  ReleaseDC(0, dc);
  return custom_cursor_;
}

void WebCursor::InitFromExternalCursor(HCURSOR cursor) {
  WebCursorInfo::Type cursor_type = ToCursorType(cursor);

  InitFromCursorInfo(WebCursorInfo(cursor_type));

  if (cursor_type == WebCursorInfo::TypeCustom)
    external_cursor_ = cursor;
}

void WebCursor::InitPlatformData() {
  external_cursor_ = NULL;
  custom_cursor_ = NULL;
}

bool WebCursor::SerializePlatformData(Pickle* pickle) const {
  // There are some issues with converting certain HCURSORS to bitmaps. The
  // HCURSOR being a user object can be marshaled as is.
  return pickle->WriteIntPtr(reinterpret_cast<intptr_t>(external_cursor_));
}

bool WebCursor::DeserializePlatformData(const Pickle* pickle, void** iter) {
  return pickle->ReadIntPtr(iter,
                            reinterpret_cast<intptr_t*>(&external_cursor_));
}

bool WebCursor::IsPlatformDataEqual(const WebCursor& other) const {
  if (!IsCustom())
    return true;

  return (external_cursor_ == other.external_cursor_);
}

void WebCursor::CopyPlatformData(const WebCursor& other) {
  external_cursor_ = other.external_cursor_;
  // The custom_cursor_ member will be initialized to a HCURSOR the next time
  // the GetCursor member function is invoked on this WebCursor instance. The
  // cursor is created using the data in the custom_data_ vector.
  custom_cursor_ = NULL;
}

void WebCursor::CleanupPlatformData() {
  external_cursor_ = NULL;

  if (custom_cursor_) {
    DestroyIcon(custom_cursor_);
    custom_cursor_ = NULL;
  }
}
