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
#include "skia/include/SkBitmap.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webkit_resources.h"

using WebCore::PlatformCursor;

static LPCWSTR ToCursorID(PlatformCursor::Type type) {
  switch (type) {
    case PlatformCursor::typePointer:
      return IDC_ARROW;
    case PlatformCursor::typeCross:
      return IDC_CROSS;
    case PlatformCursor::typeHand:
      return IDC_HAND;
    case PlatformCursor::typeIBeam:
      return IDC_IBEAM;
    case PlatformCursor::typeWait:
      return IDC_WAIT;
    case PlatformCursor::typeHelp:
      return IDC_HELP;
    case PlatformCursor::typeEastResize:
      return IDC_SIZEWE;
    case PlatformCursor::typeNorthResize:
      return IDC_SIZENS;
    case PlatformCursor::typeNorthEastResize:
      return IDC_SIZENESW;
    case PlatformCursor::typeNorthWestResize:
      return IDC_SIZENWSE;
    case PlatformCursor::typeSouthResize:
      return IDC_SIZENS;
    case PlatformCursor::typeSouthEastResize:
      return IDC_SIZENWSE;
    case PlatformCursor::typeSouthWestResize:
      return IDC_SIZENESW;
    case PlatformCursor::typeWestResize:
      return IDC_SIZEWE;
    case PlatformCursor::typeNorthSouthResize:
      return IDC_SIZENS;
    case PlatformCursor::typeEastWestResize:
      return IDC_SIZEWE;
    case PlatformCursor::typeNorthEastSouthWestResize:
      return IDC_SIZENESW;
    case PlatformCursor::typeNorthWestSouthEastResize:
      return IDC_SIZENWSE;
    case PlatformCursor::typeColumnResize:
      return MAKEINTRESOURCE(IDC_COLRESIZE);
    case PlatformCursor::typeRowResize:
      return MAKEINTRESOURCE(IDC_ROWRESIZE);
    case PlatformCursor::typeMiddlePanning:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE);
    case PlatformCursor::typeEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_EAST);
    case PlatformCursor::typeNorthPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH);
    case PlatformCursor::typeNorthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_EAST);
    case PlatformCursor::typeNorthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_WEST);
    case PlatformCursor::typeSouthPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH);
    case PlatformCursor::typeSouthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_EAST);
    case PlatformCursor::typeSouthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_WEST);
    case PlatformCursor::typeWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_WEST);
    case PlatformCursor::typeMove:
      return IDC_SIZEALL;
    case PlatformCursor::typeVerticalText:
      return MAKEINTRESOURCE(IDC_VERTICALTEXT);
    case PlatformCursor::typeCell:
      return MAKEINTRESOURCE(IDC_CELL);
    case PlatformCursor::typeContextMenu:
      return MAKEINTRESOURCE(IDC_ARROW);
    case PlatformCursor::typeAlias:
      return MAKEINTRESOURCE(IDC_ALIAS);
    case PlatformCursor::typeProgress:
      return IDC_APPSTARTING;
    case PlatformCursor::typeNoDrop:
      return IDC_NO;
    case PlatformCursor::typeCopy:
      return MAKEINTRESOURCE(IDC_COPYCUR);
    case PlatformCursor::typeNone:
      return IDC_ARROW;
    case PlatformCursor::typeNotAllowed:
      return IDC_NO;
    case PlatformCursor::typeZoomIn:
      return MAKEINTRESOURCE(IDC_ZOOMIN);
    case PlatformCursor::typeZoomOut:
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
    { LoadCursor(NULL, IDC_ARROW),       PlatformCursor::typePointer },
    { LoadCursor(NULL, IDC_IBEAM),       PlatformCursor::typeIBeam },
    { LoadCursor(NULL, IDC_WAIT),        PlatformCursor::typeWait },
    { LoadCursor(NULL, IDC_CROSS),       PlatformCursor::typeCross },
    { LoadCursor(NULL, IDC_SIZENWSE),    PlatformCursor::typeNorthWestResize },
    { LoadCursor(NULL, IDC_SIZENESW),    PlatformCursor::typeNorthEastResize },
    { LoadCursor(NULL, IDC_SIZEWE),      PlatformCursor::typeEastWestResize },
    { LoadCursor(NULL, IDC_SIZENS),      PlatformCursor::typeNorthSouthResize },
    { LoadCursor(NULL, IDC_SIZEALL),     PlatformCursor::typeMove },
    { LoadCursor(NULL, IDC_NO),          PlatformCursor::typeNotAllowed },
    { LoadCursor(NULL, IDC_HAND),        PlatformCursor::typeHand },
    { LoadCursor(NULL, IDC_APPSTARTING), PlatformCursor::typeProgress },
    { LoadCursor(NULL, IDC_HELP),        PlatformCursor::typeHelp },
  };
  for (int i = 0; i < arraysize(kStandardCursors); i++) {
    if (cursor == kStandardCursors[i].cursor)
      return kStandardCursors[i].type;
  }
  return PlatformCursor::typeCustom;
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

  if (cursor_type == WebCore::PlatformCursor::typeCustom) {
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
