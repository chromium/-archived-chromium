// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "config.h"
#include "NativeImageSkia.h"
#include "PlatformCursor.h"

#undef LOG
#include "base/gfx/gdi_util.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "grit/webkit_resources.h"
#include "skia/include/SkBitmap.h"
#include "webkit/glue/webcursor.h"

using WebCore::PlatformCursor;

static LPCWSTR ToCursorID(PlatformCursor::Type type) {
  switch (type) {
    case PlatformCursor::TypePointer:
      return IDC_ARROW;
    case PlatformCursor::TypeCross:
      return IDC_CROSS;
    case PlatformCursor::TypeHand:
      return IDC_HAND;
    case PlatformCursor::TypeIBeam:
      return IDC_IBEAM;
    case PlatformCursor::TypeWait:
      return IDC_WAIT;
    case PlatformCursor::TypeHelp:
      return IDC_HELP;
    case PlatformCursor::TypeEastResize:
      return IDC_SIZEWE;
    case PlatformCursor::TypeNorthResize:
      return IDC_SIZENS;
    case PlatformCursor::TypeNorthEastResize:
      return IDC_SIZENESW;
    case PlatformCursor::TypeNorthWestResize:
      return IDC_SIZENWSE;
    case PlatformCursor::TypeSouthResize:
      return IDC_SIZENS;
    case PlatformCursor::TypeSouthEastResize:
      return IDC_SIZENWSE;
    case PlatformCursor::TypeSouthWestResize:
      return IDC_SIZENESW;
    case PlatformCursor::TypeWestResize:
      return IDC_SIZEWE;
    case PlatformCursor::TypeNorthSouthResize:
      return IDC_SIZENS;
    case PlatformCursor::TypeEastWestResize:
      return IDC_SIZEWE;
    case PlatformCursor::TypeNorthEastSouthWestResize:
      return IDC_SIZENESW;
    case PlatformCursor::TypeNorthWestSouthEastResize:
      return IDC_SIZENWSE;
    case PlatformCursor::TypeColumnResize:
      return MAKEINTRESOURCE(IDC_COLRESIZE);
    case PlatformCursor::TypeRowResize:
      return MAKEINTRESOURCE(IDC_ROWRESIZE);
    case PlatformCursor::TypeMiddlePanning:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE);
    case PlatformCursor::TypeEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_EAST);
    case PlatformCursor::TypeNorthPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH);
    case PlatformCursor::TypeNorthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_EAST);
    case PlatformCursor::TypeNorthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_WEST);
    case PlatformCursor::TypeSouthPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH);
    case PlatformCursor::TypeSouthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_EAST);
    case PlatformCursor::TypeSouthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_WEST);
    case PlatformCursor::TypeWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_WEST);
    case PlatformCursor::TypeMove:
      return IDC_SIZEALL;
    case PlatformCursor::TypeVerticalText:
      return MAKEINTRESOURCE(IDC_VERTICALTEXT);
    case PlatformCursor::TypeCell:
      return MAKEINTRESOURCE(IDC_CELL);
    case PlatformCursor::TypeContextMenu:
      return MAKEINTRESOURCE(IDC_ARROW);
    case PlatformCursor::TypeAlias:
      return MAKEINTRESOURCE(IDC_ALIAS);
    case PlatformCursor::TypeProgress:
      return IDC_APPSTARTING;
    case PlatformCursor::TypeNoDrop:
      return IDC_NO;
    case PlatformCursor::TypeCopy:
      return MAKEINTRESOURCE(IDC_COPYCUR);
    case PlatformCursor::TypeNone:
      return IDC_ARROW;
    case PlatformCursor::TypeNotAllowed:
      return IDC_NO;
    case PlatformCursor::TypeZoomIn:
      return MAKEINTRESOURCE(IDC_ZOOMIN);
    case PlatformCursor::TypeZoomOut:
      return MAKEINTRESOURCE(IDC_ZOOMOUT);
  }
  NOTREACHED();
  return NULL;
}

static bool IsSystemCursorID(LPCWSTR cursor_id) {
  return cursor_id >= IDC_ARROW;  // See WinUser.h
}

static PlatformCursor::Type ToPlatformCursorType(HCURSOR cursor) {
  static struct {
    HCURSOR cursor;
    PlatformCursor::Type type;
  } kStandardCursors[] = {
    { LoadCursor(NULL, IDC_ARROW),       PlatformCursor::TypePointer },
    { LoadCursor(NULL, IDC_IBEAM),       PlatformCursor::TypeIBeam },
    { LoadCursor(NULL, IDC_WAIT),        PlatformCursor::TypeWait },
    { LoadCursor(NULL, IDC_CROSS),       PlatformCursor::TypeCross },
    { LoadCursor(NULL, IDC_SIZENWSE),    PlatformCursor::TypeNorthWestResize },
    { LoadCursor(NULL, IDC_SIZENESW),    PlatformCursor::TypeNorthEastResize },
    { LoadCursor(NULL, IDC_SIZEWE),      PlatformCursor::TypeEastWestResize },
    { LoadCursor(NULL, IDC_SIZENS),      PlatformCursor::TypeNorthSouthResize },
    { LoadCursor(NULL, IDC_SIZEALL),     PlatformCursor::TypeMove },
    { LoadCursor(NULL, IDC_NO),          PlatformCursor::TypeNotAllowed },
    { LoadCursor(NULL, IDC_HAND),        PlatformCursor::TypeHand },
    { LoadCursor(NULL, IDC_APPSTARTING), PlatformCursor::TypeProgress },
    { LoadCursor(NULL, IDC_HELP),        PlatformCursor::TypeHelp },
  };
  for (int i = 0; i < arraysize(kStandardCursors); i++) {
    if (cursor == kStandardCursors[i].cursor)
      return kStandardCursors[i].type;
  }
  return PlatformCursor::TypeCustom;
}

HCURSOR WebCursor::GetCursor(HINSTANCE module_handle){
  if (!IsCustom()) {
    const wchar_t* cursor_id =
        ToCursorID(static_cast<PlatformCursor::Type>(type_));

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
  WebCore::PlatformCursor::Type cursor_type = ToPlatformCursorType(cursor);

  *this = WebCursor(cursor_type);

  if (cursor_type == WebCore::PlatformCursor::TypeCustom) {
    external_cursor_ = cursor;
  }
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
