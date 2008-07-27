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

#ifndef WEBKIT_GLUE_CHROME_CLIENT_IMPL_H__
#define WEBKIT_GLUE_CHROME_CLIENT_IMPL_H__

#pragma warning(push, 0)
#include "ChromeClientWin.h"
#pragma warning(pop)

class WebViewImpl;
namespace WebCore {
    class SecurityOrigin;
    struct WindowFeatures;
}

// Handles window-level notifications from WebCore on behalf of a WebView.
class ChromeClientImpl : public WebCore::ChromeClientWin {
public:
  ChromeClientImpl(WebViewImpl* webview);
  virtual ~ChromeClientImpl();

  virtual void chromeDestroyed();

  virtual void setWindowRect(const WebCore::FloatRect&);
  virtual WebCore::FloatRect windowRect();

  virtual WebCore::FloatRect pageRect();

  virtual float scaleFactor();

  virtual void focus();
  virtual void unfocus();

  virtual bool canTakeFocus(WebCore::FocusDirection);
  virtual void takeFocus(WebCore::FocusDirection);

  virtual WebCore::Page* createWindow(WebCore::Frame*,
                                      const WebCore::FrameLoadRequest&,
                                      const WebCore::WindowFeatures&);
  virtual void show();

  virtual bool canRunModal();
  virtual void runModal();

  virtual void setToolbarsVisible(bool);
  virtual bool toolbarsVisible();

  virtual void setStatusbarVisible(bool);
  virtual bool statusbarVisible();

  virtual void setScrollbarsVisible(bool);
  virtual bool scrollbarsVisible();

  virtual void setMenubarVisible(bool);
  virtual bool menubarVisible();

  virtual void setResizable(bool);

  virtual void addMessageToConsole(const WebCore::String& message,
                                   unsigned int lineNumber,
                                   const WebCore::String& sourceID);

  virtual bool canRunBeforeUnloadConfirmPanel();
  virtual bool runBeforeUnloadConfirmPanel(const WebCore::String& message,
                                           WebCore::Frame* frame);

  virtual void closeWindowSoon();

  virtual void runJavaScriptAlert(WebCore::Frame*, const WebCore::String&);
  virtual bool runJavaScriptConfirm(WebCore::Frame*, const WebCore::String&);
  virtual bool runJavaScriptPrompt(WebCore::Frame*, const WebCore::String& message, const WebCore::String& defaultValue, WebCore::String& result);
  
  virtual void setStatusbarText(const WebCore::String&);
  virtual bool shouldInterruptJavaScript();

  // Returns true if anchors should accept keyboard focus with the tab key.
  // This method is used in a convoluted fashion by EventHandler::tabsToLinks.
  // It's a twisted path (self-evident, but more complicated than seems
  // necessary), but the net result is that returning true from here, on a
  // platform other than MAC or QT, lets anchors get keyboard focus.
  virtual bool tabsToLinks() const;

  virtual WebCore::IntRect windowResizerRect() const;
  virtual void addToDirtyRegion(const WebCore::IntRect&);
  virtual void scrollBackingStore(int dx, int dy, const WebCore::IntRect& scrollViewRect, const WebCore::IntRect& clipRect);
  virtual void updateBackingStore();

  virtual void mouseDidMoveOverElement(const WebCore::HitTestResult& result, unsigned modifierFlags);

  virtual void setToolTip(const WebCore::String& tooltip_text);

  virtual void runFileChooser(const WebCore::String&,
                              PassRefPtr<WebCore::FileChooser>);
  virtual WebCore::IntRect windowToScreen(const WebCore::IntRect& rect);

  virtual void print(WebCore::Frame*);

  virtual void exceededDatabaseQuota(WebCore::Frame*,
                                          const WebCore::String& databaseName);

private:
  WebViewImpl* webview_;  // weak pointer
  bool toolbars_visible_;
  bool statusbar_visible_;
  bool scrollbars_visible_;
  bool menubar_visible_;
  bool resizable_;
};

#endif // WEBKIT_GLUE_CHROME_CLIENT_IMPL_H__
