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

#ifndef CHROME_BROWSER_RENDER_VIEW_CONTEXT_MENU_CONTROLLER_H__
#define CHROME_BROWSER_RENDER_VIEW_CONTEXT_MENU_CONTROLLER_H__

#include "chrome/views/menu.h"
#include "chrome/common/render_messages.h"

class WebContents;

class RenderViewContextMenuController : public Menu::Delegate {
 public:
  RenderViewContextMenuController(WebContents* source_web_contents,
                                  const ViewHostMsg_ContextMenu_Params& params);
  virtual ~RenderViewContextMenuController();

  // Overridden from Menu::Delegate
  virtual std::wstring GetLabel(int id) const;
  virtual bool IsCommandEnabled(int id) const;
  virtual void ExecuteCommand(int id);
  virtual bool GetAcceleratorInfo(int id, ChromeViews::Accelerator* accel);

 private:
  // Opens the specified URL string in a new tab.  If |in_current_window| is
  // false, a new window is created to hold the new tab.
  void OpenURL(const GURL& url,
               WindowOpenDisposition disposition,
               PageTransition::Type transition);

  // Copy to the clipboard an image located at a point in the RenderView
  void CopyImageAt(int x, int y);

  // Launch the inspector targeting a point in the RenderView
  void Inspect(int x, int y);

  // Writes the specified text/url to the system clipboard
  void WriteTextToClipboard(const std::wstring& text);
  void WriteURLToClipboard(const GURL& url);

  bool IsDevCommandEnabled(int id) const;

  DISALLOW_EVIL_CONSTRUCTORS(RenderViewContextMenuController);

 private:
  WebContents* source_web_contents_;
  ViewHostMsg_ContextMenu_Params params_;
};

#endif  // CHROME_BROWSER_RENDER_VIEW_CONTEXT_MENU_CONTROLLER_H__
