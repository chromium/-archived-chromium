// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_DRAG_UTILS_H_
#define VIEWS_DRAG_UTILS_H_

#include <string>

namespace gfx {
class Canvas;
}
class GURL;
class OSExchangeData;
class SkBitmap;

namespace drag_utils {

// Sets url and title on data as well as setting a suitable image for dragging.
// The image looks like that of the bookmark buttons.
void SetURLAndDragImage(const GURL& url,
                        const std::wstring& title,
                        const SkBitmap& icon,
                        OSExchangeData* data);

// Creates a dragging image to be displayed when the user drags a file from
// Chrome (via the download manager, for example). The drag image is set into
// the supplied data_object. 'file_name' can be a full path, but the directory
// portion will be truncated in the drag image.
void CreateDragImageForFile(const std::wstring& file_name,
                            SkBitmap* icon,
                            OSExchangeData* data_object);

// Sets the drag image on data_object from the supplied canvas. width/height
// are the size of the image to use, and the offsets give the location of
// the hotspot for the drag image.
void SetDragImageOnDataObject(const gfx::Canvas& canvas,
                              int width,
                              int height,
                              int cursor_x_offset,
                              int cursor_y_offset,
                              OSExchangeData* data_object);
} // namespace drag_utils

#endif  // #ifndef VIEWS_DRAG_UTILS_H_
