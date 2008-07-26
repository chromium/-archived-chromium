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

#ifndef CHROME_VIEWS_LAYOUT_MANAGER_H__
#define CHROME_VIEWS_LAYOUT_MANAGER_H__

#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>

namespace ChromeViews {

class View;

/////////////////////////////////////////////////////////////////////////////
//
// LayoutManager interface
//
//   The LayoutManager interface provides methods to handle the sizing of
//   the children of a View according to implementation-specific heuristics.
//
/////////////////////////////////////////////////////////////////////////////
class LayoutManager {
 public:
  virtual ~LayoutManager() {}

  // Notification that this LayoutManager has been installed on a particular
  // host.
  virtual void Installed(View* host) {}

  // Notification that this LayoutManager has been uninstalled on a particular
  // host.
  virtual void Uninstalled(View* host) {}

  // Lay out the children of |host| according to implementation-specific
  // heuristics. The graphics used during painting is provided to allow for
  // string sizing.
  virtual void Layout(View* host) = 0;

  // Return the preferred size which is the size required to give each
  // children their respective preferred size.
  virtual void GetPreferredSize(View* host, CSize* out) = 0;

  // Returns the preferred height for the specified width. The default
  // implementation returns the value from GetPreferredSize.
  virtual int GetPreferredHeightForWidth(View* host, int width);

  // Notification that a view has been added.
  virtual void ViewAdded(View* host, View* view) {}

  // Notification that a view has been removed.
  virtual void ViewRemoved(View* host, View* view) {}
};

}

#endif // CHROME_VIEWS_LAYOUT_MANAGER_H__
