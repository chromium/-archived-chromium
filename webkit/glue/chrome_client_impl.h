// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_CHROME_CLIENT_IMPL_H__
#define WEBKIT_GLUE_CHROME_CLIENT_IMPL_H__

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "ChromeClientChromium.h"
MSVC_POP_WARNING();

class WebViewImpl;
namespace WebCore {
    class SecurityOrigin;
    struct WindowFeatures;
}

// Handles window-level notifications from WebCore on behalf of a WebView.
class ChromeClientImpl : public WebCore::ChromeClientChromium {
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

