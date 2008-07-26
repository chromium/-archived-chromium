// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_DRAG_UTILS_H__
#define CHROME_BROWSER_DRAG_UTILS_H__

#include <objidl.h>
#include <string>

class ChromeCanvas;
class GURL;
class OSExchangeData;
class SkBitmap;

namespace drag_utils {

// Creates a dragging image to be displayed when the user drags an item with a
// link. The drag image is set into the supplied data_object.
void CreateDragImageForLink(const GURL& url,
                            const std::wstring& title,
                            IDataObject* data_object);

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
                            IDataObject* data_object);

// Sets the drag image on data_object from the supplied canvas. width/height
// are the size of the image to use, and the offsets give the location of
// the hotspot for the drag image.
void SetDragImageOnDataObject(const ChromeCanvas& canvas,
                              int width,
                              int height,
                              int cursor_x_offset,
                              int cursor_y_offset,
                              IDataObject* data_object);

} // namespace drag_utils

#endif  // #ifndef CHROME_BROWSER_DRAG_UTILS_H__
