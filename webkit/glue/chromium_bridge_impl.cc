// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "config.h"
#include "ChromiumBridge.h"

#include "Cursor.h"
#include "Frame.h"
#include "FrameView.h"
#include "HostWindow.h"
#include "Page.h"
#include "PlatformWidget.h"
#include "ScrollView.h"
#include "Widget.h"

#undef LOG
#include "webkit/glue/chrome_client_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview_impl.h"
#include "webkit/glue/webview_delegate.h"

namespace WebCore {

static PlatformWidget ToPlatform(Widget* widget) {
  return widget ? widget->root()->hostWindow()->platformWindow() : 0;
}

static ChromeClientImpl* ToChromeClient(Widget* widget) {
  FrameView* view;
  if (widget->isFrameView()) {
    view = static_cast<FrameView*>(widget);
  } else if (widget->parent() && widget->parent()->isFrameView()) {
    view = static_cast<FrameView*>(widget->parent());
  } else {
    return NULL;
  }

  Page* page = view->frame() ? view->frame()->page() : NULL;
  if (!page)
    return NULL;

  return static_cast<ChromeClientImpl*>(page->chrome()->client());
}

// Cookies --------------------------------------------------------------------

void ChromiumBridge::setCookies(
    const KURL& url, const KURL& policy_url, const String& cookie) {
  webkit_glue::SetCookie(
      webkit_glue::KURLToGURL(url),
      webkit_glue::KURLToGURL(policy_url),
      webkit_glue::StringToStdString(cookie));
}

String ChromiumBridge::cookies(const KURL& url, const KURL& policy_url) {
  return webkit_glue::StdStringToString(webkit_glue::GetCookies(
      webkit_glue::KURLToGURL(url),
      webkit_glue::KURLToGURL(policy_url)));
}

// DNS ------------------------------------------------------------------------

void ChromiumBridge::prefetchDNS(const String& hostname) {
  webkit_glue::PrefetchDns(webkit_glue::StringToStdString(hostname));
}

// Screen ---------------------------------------------------------------------

int ChromiumBridge::screenDepth(Widget* widget) {
  return webkit_glue::GetScreenInfo(ToPlatform(widget)).depth;
}

int ChromiumBridge::screenDepthPerComponent(Widget* widget) {
  return webkit_glue::GetScreenInfo(ToPlatform(widget)).depth_per_component;
}

bool ChromiumBridge::screenIsMonochrome(Widget* widget) {
  return webkit_glue::GetScreenInfo(ToPlatform(widget)).is_monochrome;
}

IntRect ChromiumBridge::screenRect(Widget* widget) {
  return webkit_glue::ToIntRect(
      webkit_glue::GetScreenInfo(ToPlatform(widget)).rect);
}

IntRect ChromiumBridge::screenAvailableRect(Widget* widget) {
  return webkit_glue::ToIntRect(
      webkit_glue::GetScreenInfo(ToPlatform(widget)).available_rect);
}

// Widget ---------------------------------------------------------------------

void ChromiumBridge::widgetSetCursor(Widget* widget, const Cursor& cursor) {
  ChromeClientImpl* chrome_client = ToChromeClient(widget);
  if (chrome_client)
    chrome_client->SetCursor(WebCursor(cursor.impl()));
}

void ChromiumBridge::widgetSetFocus(Widget* widget) {
  ChromeClientImpl* chrome_client = ToChromeClient(widget);
  if (chrome_client)
    chrome_client->focus();
}

}  // namespace WebCore
