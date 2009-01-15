// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ABOUT_INTERNETS_STATUS_VIEW_H__
#define CHROME_BROWSER_ABOUT_INTERNETS_STATUS_VIEW_H__

#include "base/scoped_handle.h"
#include "chrome/browser/tab_contents/status_view.h"

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

