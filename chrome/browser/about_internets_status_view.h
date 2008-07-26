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

#ifndef CHROME_BROWSER_ABOUT_INTERNETS_STATUS_VIEW_H__
#define CHROME_BROWSER_ABOUT_INTERNETS_STATUS_VIEW_H__

#include "base/scoped_handle.h"
#include "chrome/browser/status_view.h"

// Displays sspipes.scr in the content HWND.
class AboutInternetsStatusView : public StatusView {
 public:
  AboutInternetsStatusView();
  virtual ~AboutInternetsStatusView();

  // TabContents overrides
  virtual const std::wstring GetDefaultTitle() const;
  virtual const std::wstring& GetTitle() const;

  // StatusView implementations

  // Starts sspipes.scr rendering into the contents HWND. (Actually, it looks
  // like this creates a child HWND which is the same size as the contents,
  // and draws into that. Thus, it doesn't resize properly.)
  // TODO(devint): Fix this resizing issue. A few possibilities:
  // 1) Restart the process a few seconds after a resize is completed.
  // 2) Render into an invisible HWND and stretchblt to the current HWND.
  virtual void OnCreate(const CRect& rect);
  // Does nothing, but implementation is required by StatusView.
  virtual void OnSize(const CRect& rect);

 private:
  // Information about the pipes process, used to close the process when this
  // view is destroyed.
  ScopedHandle process_handle_;

  // Title of the page.
  std::wstring title_;

  DISALLOW_EVIL_CONSTRUCTORS(AboutInternetsStatusView);
};

#endif  // CHROME_BROWSER_ABOUT_INTERNETS_STATUS_VIEW_H__
