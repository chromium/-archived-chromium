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

#ifndef CHROME_VIEWS_BORDER_H__
#define CHROME_VIEWS_BORDER_H__

#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/insets.h"
#include "chrome/views/view.h"
#include "SkColor.h"

namespace ChromeViews {


class View;

////////////////////////////////////////////////////////////////////////////////
//
// Border class.
//
// The border class is used to display a border around a view.
// To set a border on a view, just call SetBorder on the view, for example:
// view->SetBorder(Border::CreateSolidBorder(1, SkColorSetRGB(25, 25, 112));
// Once set on a view, the border is owned by the view.
//
// IMPORTANT NOTE: not all views support borders at this point. In order to
// support the border, views should make sure to use bounds excluding the
// border (by calling View::GetLocalBoundsExcludingBorder) when doing layout and
// painting.
//
////////////////////////////////////////////////////////////////////////////////

class Border {
 public:
  Border();
  virtual ~Border();

  // Creates a border that is a simple line of the specified thickness and
  // color.
  static Border* CreateSolidBorder(int thickness, SkColor color);

  // Creates a border for reserving space. The returned border does not
  // paint anything.
  static Border* CreateEmptyBorder(int top, int left, int bottom, int right);

  // Renders the border for the specified view.
  virtual void Paint(const View& view, ChromeCanvas* canvas) const = 0;

  // Sets the specified insets to the the border insets.
  virtual void GetInsets(gfx::Insets* insets) const = 0;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(Border);
};

}

#endif // CHROME_VIEWS_BORDER_H__
