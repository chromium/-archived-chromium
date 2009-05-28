// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/drag_utils.h"

#include <objidl.h>
#include <shlobj.h>
#include <shobjidl.h>

#include "app/gfx/canvas.h"
#include "app/os_exchange_data.h"
#include "base/gfx/gdi_util.h"

namespace drag_utils {

static void SetDragImageOnDataObject(HBITMAP hbitmap,
                                     int width,
                                     int height,
                                     int cursor_offset_x,
                                     int cursor_offset_y,
                                     IDataObject* data_object) {
  IDragSourceHelper* helper = NULL;
  HRESULT rv = CoCreateInstance(CLSID_DragDropHelper, 0, CLSCTX_INPROC_SERVER,
      IID_IDragSourceHelper, reinterpret_cast<LPVOID*>(&helper));
  if (SUCCEEDED(rv)) {
    SHDRAGIMAGE sdi;
    sdi.sizeDragImage.cx = width;
    sdi.sizeDragImage.cy = height;
    sdi.crColorKey = 0xFFFFFFFF;
    sdi.hbmpDragImage = hbitmap;
    sdi.ptOffset.x = cursor_offset_x;
    sdi.ptOffset.y = cursor_offset_y;
    helper->InitializeFromBitmap(&sdi, data_object);
  }
};

// Blit the contents of the canvas to a new HBITMAP. It is the caller's
// responsibility to release the |bits| buffer.
static HBITMAP CreateBitmapFromCanvas(const gfx::Canvas& canvas,
                                      int width,
                                      int height) {
  HDC screen_dc = GetDC(NULL);
  BITMAPINFOHEADER header;
  gfx::CreateBitmapHeader(width, height, &header);
  void* bits;
  HBITMAP bitmap =
      CreateDIBSection(screen_dc, reinterpret_cast<BITMAPINFO*>(&header),
                       DIB_RGB_COLORS, &bits, NULL, 0);
  HDC compatible_dc = CreateCompatibleDC(screen_dc);
  HGDIOBJ old_object = SelectObject(compatible_dc, bitmap);
  BitBlt(compatible_dc, 0, 0, width, height,
         canvas.getTopPlatformDevice().getBitmapDC(),
         0, 0, SRCCOPY);
  SelectObject(compatible_dc, old_object);
  ReleaseDC(NULL, compatible_dc);
  ReleaseDC(NULL, screen_dc);
  return bitmap;
}

void SetDragImageOnDataObject(const gfx::Canvas& canvas,
                              int width,
                              int height,
                              int cursor_x_offset,
                              int cursor_y_offset,
                              OSExchangeData* data_object) {
  DCHECK(data_object && width > 0 && height > 0);
  // SetDragImageOnDataObject(HBITMAP) takes ownership of the bitmap.
  HBITMAP bitmap = CreateBitmapFromCanvas(canvas, width, height);

  // Attach 'bitmap' to the data_object.
  SetDragImageOnDataObject(bitmap, width, height,
                           cursor_x_offset,
                           cursor_y_offset,
                           data_object);
}

} // namespace drag_utils
