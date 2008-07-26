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

#ifndef CHROME_VIEWS_IMAGE_VIEW_H__
#define CHROME_VIEWS_IMAGE_VIEW_H__

#include <windows.h>

#include "chrome/views/view.h"
#include "SkBitmap.h"

class ChromeCanvas;

namespace ChromeViews {

/////////////////////////////////////////////////////////////////////////////
//
// ImageView class.
//
// An ImageView can display an image from an SkBitmap. If a size is provided,
// the ImageView will resize the provided image to fit if it is too big or will
// center the image if smaller. Otherwise, the preferred size matches the
// provided image size.
//
/////////////////////////////////////////////////////////////////////////////
class ImageView : public View {
 public:
  enum Alignment {
    LEADING = 0,
    CENTER,
    TRAILING
  };

  ImageView();
  virtual ~ImageView();

  // Set the bitmap that should be displayed.
  void SetImage(const SkBitmap& bm);

  // Set the bitmap that should be displayed from a pointer. Reset the image
  // if the pointer is NULL. The pointer contents is copied in the receiver's
  // bitmap.
  void SetImage(SkBitmap* bm);

  // Returns the bitmap currently displayed or NULL of none is currently set.
  // The returned bitmap is still owned by the ImageView.
  const SkBitmap& GetImage();

  // Set the desired image size for the receiving ImageView.
  void SetImageSize(const CSize& image_size);

  // Return the preferred size for the receiving view. Returns false if the
  // preferred size is not defined, which means that the view uses the image
  // size.
  bool GetImageSize(CSize* image_size);

  // Reset the image size to the current image dimensions.
  void ResetImageSize();

  // Set / Get the horizontal alignment.
  void SetHorizontalAlignment(Alignment ha);
  Alignment GetHorizontalAlignment();

  // Set / Get the vertical alignment.
  void SetVerticalAlignment(Alignment va);
  Alignment GetVerticalAlignment();

  // Set / Get the tooltip text.
  void SetTooltipText(const std::wstring& tooltip);
  std::wstring GetTooltipText();

  // Return whether the image should be centered inside the view.
  // Overriden from View
  virtual void GetPreferredSize(CSize* out);
  virtual void Paint(ChromeCanvas* canvas);

  // Overriden from View.
  virtual bool GetTooltipText(int x, int y, std::wstring* tooltip);

 private:
  // Compute the image origin given the desired size and the receiver alignment
  // properties.
  void ComputeImageOrigin(int image_width, int image_height, int *x, int *y);

  // Whether the image size is set.
  bool image_size_set_;

  // The actual image size.
  CSize image_size_;

  // The underlying bitmap.
  SkBitmap image_;

  // Horizontal alignment.
  Alignment horiz_alignment_;

  // Vertical alignment.
  Alignment vert_alignment_;

  // The current tooltip text.
  std::wstring tooltip_text_;

  DISALLOW_EVIL_CONSTRUCTORS(ImageView);
};

}

#endif  // CHROME_VIEWS_IMAGE_VIEW_H__
