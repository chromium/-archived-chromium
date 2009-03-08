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

#include "WebKit.h"

#undef LOG
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/time.h"
#include "build/build_config.h"
#include "googleurl/src/url_util.h"
#include "skia/ext/skia_utils_win.h"
#if USE(V8)
#include <v8.h>
#endif
#include "grit/webkit_resources.h"
#include "webkit/glue/chrome_client_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/screen_info.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin_impl.h"
#include "webkit/glue/webview.h"

#if defined(OS_WIN)
#include <windows.h>
#include <vssym32.h>

#include "base/gfx/native_theme.h"
#endif

namespace {

gfx::NativeViewId ToNativeId(WebCore::Widget* widget) {
  if (!widget)
    return 0;
  return widget->root()->hostWindow()->platformWindow();
}

#if PLATFORM(WIN_OS)
static RECT IntRectToRECT(const WebCore::IntRect& r) {
  RECT result;
  result.left = r.x();
  result.top = r.y();
  result.right = r.right();
  result.bottom = r.bottom();
  return result;
}
#endif

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

}  // namespace

namespace WebCore {

// Font -----------------------------------------------------------------------

#if defined(OS_WIN)
bool ChromiumBridge::ensureFontLoaded(HFONT font) {
  return webkit_glue::EnsureFontLoaded(font);
}
#endif

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

bool ChromiumBridge::plugins(bool refresh, Vector<PluginInfo*>* results) {
  std::vector<WebPluginInfo> glue_plugins;
  if (!webkit_glue::GetPlugins(refresh, &glue_plugins))
    return false;
  for (size_t i = 0; i < glue_plugins.size(); ++i) {
    PluginInfo* rv = new PluginInfo;
    const WebPluginInfo& plugin = glue_plugins[i];
    rv->name = webkit_glue::StdWStringToString(plugin.name);
    rv->desc = webkit_glue::StdWStringToString(plugin.desc);
    rv->file =
#if defined(OS_WIN)
      webkit_glue::StdWStringToString(plugin.path.BaseName().value());
#elif defined(OS_POSIX)
      webkit_glue::StdStringToString(plugin.path.BaseName().value());
#endif
    for (size_t j = 0; j < plugin.mime_types.size(); ++ j) {
      MimeClassInfo* new_mime = new MimeClassInfo();
      const WebPluginMimeType& mime_type = plugin.mime_types[j];
      new_mime->desc = webkit_glue::StdWStringToString(mime_type.description);

      for (size_t k = 0; k < mime_type.file_extensions.size(); ++k) {
        if (new_mime->suffixes.length())
          new_mime->suffixes.append(",");

        new_mime->suffixes.append(webkit_glue::StdStringToString(
            mime_type.file_extensions[k]));
      }

      new_mime->type = webkit_glue::StdStringToString(mime_type.mime_type);
      new_mime->plugin = rv;
      rv->mimes.append(new_mime);
    }
    results->append(rv);
  }
  return true;
}

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
  return webkit_glue::GetScreenInfo(ToNativeId(widget)).depth;
}

int ChromiumBridge::screenDepthPerComponent(Widget* widget) {
  return webkit_glue::GetScreenInfo(ToNativeId(widget)).depth_per_component;
}

bool ChromiumBridge::screenIsMonochrome(Widget* widget) {
  return webkit_glue::GetScreenInfo(ToNativeId(widget)).is_monochrome;
}

IntRect ChromiumBridge::screenRect(Widget* widget) {
  return webkit_glue::ToIntRect(
      webkit_glue::GetScreenInfo(ToNativeId(widget)).rect);
}

IntRect ChromiumBridge::screenAvailableRect(Widget* widget) {
  return webkit_glue::ToIntRect(
      webkit_glue::GetScreenInfo(ToNativeId(widget)).available_rect);
}

// Theming --------------------------------------------------------------------

#if PLATFORM(WIN_OS)

void ChromiumBridge::paintButton(
    GraphicsContext* gc, int part, int state, int classic_state,
    const IntRect& rect) {
  skia::PlatformCanvas* canvas = gc->platformContext()->canvas();
  HDC hdc = canvas->beginPlatformPaint();

  RECT native_rect = IntRectToRECT(rect);
  gfx::NativeTheme::instance()->PaintButton(
      hdc, part, state, classic_state, &native_rect);

  canvas->endPlatformPaint();
}

void ChromiumBridge::paintMenuList(
    GraphicsContext* gc, int part, int state, int classic_state,
    const IntRect& rect) {
  skia::PlatformCanvas* canvas = gc->platformContext()->canvas();
  HDC hdc = canvas->beginPlatformPaint();

  RECT native_rect = IntRectToRECT(rect);
  gfx::NativeTheme::instance()->PaintMenuList(
      hdc, part, state, classic_state, &native_rect);

  canvas->endPlatformPaint();
}

void ChromiumBridge::paintScrollbarArrow(
    GraphicsContext* gc, int state, int classic_state, const IntRect& rect) {
  skia::PlatformCanvas* canvas = gc->platformContext()->canvas();
  HDC hdc = canvas->beginPlatformPaint();

  RECT native_rect = IntRectToRECT(rect);
  gfx::NativeTheme::instance()->PaintScrollbarArrow(
      hdc, state, classic_state, &native_rect);

  canvas->endPlatformPaint();
}

void ChromiumBridge::paintScrollbarThumb(
    GraphicsContext* gc, int part, int state, int classic_state,
    const IntRect& rect) {
  skia::PlatformCanvas* canvas = gc->platformContext()->canvas();
  HDC hdc = canvas->beginPlatformPaint();

  RECT native_rect = IntRectToRECT(rect);
  gfx::NativeTheme::instance()->PaintScrollbarThumb(
      hdc, part, state, classic_state, &native_rect);

  canvas->endPlatformPaint();
}

void ChromiumBridge::paintScrollbarTrack(
    GraphicsContext* gc, int part, int state, int classic_state,
    const IntRect& rect, const IntRect& align_rect) {
  skia::PlatformCanvas* canvas = gc->platformContext()->canvas();
  HDC hdc = canvas->beginPlatformPaint();

  RECT native_rect = IntRectToRECT(rect);
  RECT native_align_rect = IntRectToRECT(align_rect);
  gfx::NativeTheme::instance()->PaintScrollbarTrack(
      hdc, part, state, classic_state, &native_rect, &native_align_rect,
      canvas);

  canvas->endPlatformPaint();
}

void ChromiumBridge::paintTextField(
    GraphicsContext* gc, int part, int state, int classic_state,
    const IntRect& rect, const Color& color, bool fill_content_area,
    bool draw_edges) {
  skia::PlatformCanvas* canvas = gc->platformContext()->canvas();
  HDC hdc = canvas->beginPlatformPaint();

  RECT native_rect = IntRectToRECT(rect);
  COLORREF clr = skia::SkColorToCOLORREF(color.rgb());

  gfx::NativeTheme::instance()->PaintTextField(
      hdc, part, state, classic_state, &native_rect, clr, fill_content_area,
      draw_edges);

  canvas->endPlatformPaint();
}

#endif

// URL ------------------------------------------------------------------------

KURL ChromiumBridge::inspectorURL() {
  return webkit_glue::GURLToKURL(webkit_glue::GetInspectorURL());
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
