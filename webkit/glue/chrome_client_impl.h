// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_CHROME_CLIENT_IMPL_H__
#define WEBKIT_GLUE_CHROME_CLIENT_IMPL_H__

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "ChromeClientChromium.h"
MSVC_POP_WARNING();

class WebCursor;
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

  WebViewImpl* webview() { return webview_; }

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
  virtual bool runJavaScriptPrompt(WebCore::Frame*,
                                   const WebCore::String& message,
                                   const WebCore::String& defaultValue,
                                   WebCore::String& result);

  virtual void setStatusbarText(const WebCore::String&);
  virtual bool shouldInterruptJavaScript();

  // Returns true if anchors should accept keyboard focus with the tab key.
  // This method is used in a convoluted fashion by EventHandler::tabsToLinks.
  // It's a twisted path (self-evident, but more complicated than seems
  // necessary), but the net result is that returning true from here, on a
  // platform other than MAC or QT, lets anchors get keyboard focus.
  virtual bool tabsToLinks() const;

  virtual WebCore::IntRect windowResizerRect() const;

  virtual void repaint(const WebCore::IntRect&, bool contentChanged,
                       bool immediate = false, bool repaintContentOnly = false);
  virtual void scroll(const WebCore::IntSize& scrollDelta,
                      const WebCore::IntRect& rectToScroll,
                      const WebCore::IntRect& clipRect);
  virtual WebCore::IntPoint screenToWindow(const WebCore::IntPoint&) const;
  virtual WebCore::IntRect windowToScreen(const WebCore::IntRect&) const;
  virtual PlatformWidget platformWindow() const;
  virtual void contentsSizeChanged(WebCore::Frame*, const WebCore::IntSize&) const {}

  virtual void mouseDidMoveOverElement(const WebCore::HitTestResult& result,
                                       unsigned modifierFlags);

  virtual void setToolTip(const WebCore::String& tooltip_text);

  virtual void print(WebCore::Frame*);

  virtual void exceededDatabaseQuota(WebCore::Frame*,
                                     const WebCore::String& databaseName);

  virtual void runOpenPanel(WebCore::Frame*,
                            PassRefPtr<WebCore::FileChooser>);
  virtual void popupOpened(WebCore::FramelessScrollView* popup_view,
                           const WebCore::IntRect& bounds,
                           bool focus_on_show);

  void SetCursor(const WebCursor& cursor);

  virtual void enableSuddenTermination();
  virtual void disableSuddenTermination();

  virtual void formStateDidChange(const WebCore::Node*);

private:
  WebViewImpl* webview_;  // weak pointer
  bool toolbars_visible_;
  bool statusbar_visible_;
  bool scrollbars_visible_;
  bool menubar_visible_;
  bool resizable_;
};

#endif // WEBKIT_GLUE_CHROME_CLIENT_IMPL_H__

