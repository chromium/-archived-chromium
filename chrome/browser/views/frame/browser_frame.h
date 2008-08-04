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

#ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_FRAME_H_
#define CHROME_BROWSER_VIEWS_FRAME_BROWSER_FRAME_H_

class BrowserView2;
namespace ChromeViews {
class Window;
}
namespace gfx {
class Rect;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame
//
//  BrowserFrame is an interface that represents a top level browser window
//  frame. Implementations of this interface exist to supply the browser window
//  for specific environments, e.g. Vista with Aero Glass enabled.
//
class BrowserFrame {
 public:
  // Returns the ChromeViews::Window associated with this frame.
  virtual ChromeViews::Window* GetWindow() = 0;

  enum FrameType {
    FRAMETYPE_OPAQUE,
    FRAMETYPE_AERO_GLASS
  };

  // Returns the FrameType that should be constructed given the current system
  // settings.
  static FrameType GetActiveFrameType();

  // Creates a BrowserFrame instance for the specified FrameType and
  // BrowserView.
  static BrowserFrame* CreateForBrowserView(FrameType type,
                                            BrowserView2* browser_view,
                                            const gfx::Rect& bounds,
                                            int show_command);

};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_FRAME_H_
