// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "config.h"
#include "ChromiumBridge.h"

#include "BitmapImage.h"
#include "Cursor.h"
#include "Frame.h"
#include "FrameView.h"
#include "HostWindow.h"
#include "KURL.h"
#include "NativeImageSkia.h"
#include "Page.h"
#include "PasteboardPrivate.h"
#include "PlatformContextSkia.h"
#include "PlatformString.h"
#include "PlatformWidget.h"
#include "PluginData.h"
#include "PluginInfoStore.h"
#include "ScrollbarTheme.h"
#include "ScrollView.h"
#include "SystemTime.h"
#include "Widget.h"
#include <wtf/CurrentTime.h>

#undef LOG
#include "base/file_util.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "googleurl/src/url_util.h"
#include "skia/ext/skia_utils_win.h"
#include "webkit/api/public/WebCursorInfo.h"
#include "webkit/api/public/WebScreenInfo.h"
#include "webkit/glue/chrome_client_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin_impl.h"
#include "webkit/glue/webview_impl.h"

#if defined(OS_WIN)
#include <windows.h>
#include <vssym32.h>
#include "base/gfx/native_theme.h"
#endif

using WebKit::WebCursorInfo;

namespace {

gfx::NativeViewId ToNativeId(WebCore::Widget* widget) {
  if (!widget)
    return 0;
  return widget->root()->hostWindow()->platformWindow();
}

ChromeClientImpl* ToChromeClient(WebCore::Widget* widget) {
  WebCore::FrameView* view;
  if (widget->isFrameView()) {
    view = static_cast<WebCore::FrameView*>(widget);
  } else if (widget->parent() && widget->parent()->isFrameView()) {
    view = static_cast<WebCore::FrameView*>(widget->parent());
  } else {
    return NULL;
  }

  WebCore::Page* page = view->frame() ? view->frame()->page() : NULL;
  if (!page)
    return NULL;

  return static_cast<ChromeClientImpl*>(page->chrome()->client());
}

WebViewImpl* ToWebView(WebCore::Widget* widget) {
  ChromeClientImpl* chrome_client = ToChromeClient(widget);
  if (!chrome_client)
    return NULL;
  return chrome_client->webview();
}

WebCore::IntRect ToIntRect(const WebKit::WebRect& input) {
  return WebCore::IntRect(input.x, input.y, input.width, input.height);
}

}  // namespace

namespace WebCore {

// JavaScript -----------------------------------------------------------------

void ChromiumBridge::notifyJSOutOfMemory(Frame* frame) {
  if (!frame)
    return;

  // Dispatch to the delegate of the view that owns the frame.
  WebFrame* webframe = WebFrameImpl::FromFrame(frame);
  WebView* webview = webframe->GetView();
  if (!webview)
    return;
  WebViewDelegate* delegate = webview->GetDelegate();
  if (!delegate)
    return;
  delegate->JSOutOfMemory();
}

// Plugin ---------------------------------------------------------------------

NPObject* ChromiumBridge::pluginScriptableObject(Widget* widget) {
  if (!widget)
    return NULL;

  // NOTE:  We have to trust that the widget passed to us here is a
  // WebPluginImpl.  There isn't a way to dynamically verify it, since the
  // derived class (Widget) has no identifier.
  return static_cast<WebPluginContainer*>(widget)->GetPluginScriptableObject();
}

bool ChromiumBridge::popupsAllowed(NPP npp) {
  bool popups_allowed = false;
  if (npp) {
    NPAPI::PluginInstance* plugin_instance =
        reinterpret_cast<NPAPI::PluginInstance*>(npp->ndata);
    if (plugin_instance)
      popups_allowed = plugin_instance->popups_allowed();
  }
  return popups_allowed;
}

// Protocol -------------------------------------------------------------------

String ChromiumBridge::uiResourceProtocol() {
  return webkit_glue::StdStringToString(webkit_glue::GetUIResourceProtocol());
}


// Screen ---------------------------------------------------------------------

int ChromiumBridge::screenDepth(Widget* widget) {
  WebViewImpl* view = ToWebView(widget);
  if (!view || !view->delegate())
    return NULL;
  return view->delegate()->GetScreenInfo(view).depth;
}

int ChromiumBridge::screenDepthPerComponent(Widget* widget) {
  WebViewImpl* view = ToWebView(widget);
  if (!view || !view->delegate())
    return NULL;
  return view->delegate()->GetScreenInfo(view).depthPerComponent;
}

bool ChromiumBridge::screenIsMonochrome(Widget* widget) {
  WebViewImpl* view = ToWebView(widget);
  if (!view || !view->delegate())
    return NULL;
  return view->delegate()->GetScreenInfo(view).isMonochrome;
}

IntRect ChromiumBridge::screenRect(Widget* widget) {
  WebViewImpl* view = ToWebView(widget);
  if (!view || !view->delegate())
    return IntRect();
  return ToIntRect(view->delegate()->GetScreenInfo(view).rect);
}

IntRect ChromiumBridge::screenAvailableRect(Widget* widget) {
  WebViewImpl* view = ToWebView(widget);
  if (!view || !view->delegate())
    return IntRect();
  return ToIntRect(view->delegate()->GetScreenInfo(view).availableRect);
}

// Widget ---------------------------------------------------------------------

void ChromiumBridge::widgetSetCursor(Widget* widget, const Cursor& cursor) {
  ChromeClientImpl* chrome_client = ToChromeClient(widget);
  if (chrome_client)
    chrome_client->SetCursor(webkit_glue::CursorToWebCursorInfo(cursor));
}

void ChromiumBridge::widgetSetFocus(Widget* widget) {
  ChromeClientImpl* chrome_client = ToChromeClient(widget);
  if (chrome_client)
    chrome_client->focus();
}

}  // namespace WebCore
