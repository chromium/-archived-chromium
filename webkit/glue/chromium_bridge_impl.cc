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
#include "base/trace_event.h"
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
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin_impl.h"

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
  webkit_glue::NotifyJSOutOfMemory(frame);
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


// Resources ------------------------------------------------------------------

PassRefPtr<Image> ChromiumBridge::loadPlatformImageResource(const char* name) {

  // The rest get converted to a resource ID that we can pass to the glue.
  int resource_id = 0;
  if (!strcmp(name, "textAreaResizeCorner")) {
    resource_id = IDR_TEXTAREA_RESIZER;
  } else if (!strcmp(name, "missingImage")) {
    resource_id = IDR_BROKENIMAGE;
  } else if (!strcmp(name, "tickmarkDash")) {
    resource_id = IDR_TICKMARK_DASH;
  } else if (!strcmp(name, "panIcon")) {
    resource_id = IDR_PAN_SCROLL_ICON;
  } else if (!strcmp(name, "linuxCheckboxOff")) {
    resource_id = IDR_LINUX_CHECKBOX_OFF;
  } else if (!strcmp(name, "linuxCheckboxOn")) {
    resource_id = IDR_LINUX_CHECKBOX_ON;
  } else if (!strcmp(name, "linuxRadioOff")) {
    resource_id = IDR_LINUX_RADIO_OFF;
  } else if (!strcmp(name, "linuxRadioOn")) {
    resource_id = IDR_LINUX_RADIO_ON;
  } else if (!strcmp(name, "deleteButton")) {
    if (WebKit::layoutTestMode()) {
      RefPtr<Image> image = BitmapImage::create();
      // Create a red 30x30 square used only in layout tests.
      const char red_square[] =
          "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52"
          "\x00\x00\x00\x1e\x00\x00\x00\x1e\x04\x03\x00\x00\x00\xc9\x1e\xb3"
          "\x91\x00\x00\x00\x30\x50\x4c\x54\x45\x00\x00\x00\x80\x00\x00\x00"
          "\x80\x00\x80\x80\x00\x00\x00\x80\x80\x00\x80\x00\x80\x80\x80\x80"
          "\x80\xc0\xc0\xc0\xff\x00\x00\x00\xff\x00\xff\xff\x00\x00\x00\xff"
          "\xff\x00\xff\x00\xff\xff\xff\xff\xff\x7b\x1f\xb1\xc4\x00\x00\x00"
          "\x09\x70\x48\x59\x73\x00\x00\x0b\x13\x00\x00\x0b\x13\x01\x00\x9a"
          "\x9c\x18\x00\x00\x00\x17\x49\x44\x41\x54\x78\x01\x63\x98\x89\x0a"
          "\x18\x50\xb9\x33\x47\xf9\xa8\x01\x32\xd4\xc2\x03\x00\x33\x84\x0d"
          "\x02\x3a\x91\xeb\xa5\x00\x00\x00\x00\x49\x45\x4e\x44\xae\x42\x60"
          "\x82";
      const int size = arraysize(red_square);
      RefPtr<SharedBuffer> buffer = SharedBuffer::create(red_square, size);
      image->setData(buffer, true);
      return image;
    }
  } else {
    NOTREACHED() << "Unknown image resource " << name;
    return Image::nullImage();
  }

  std::string data = webkit_glue::GetDataResource(resource_id);
  RefPtr<SharedBuffer> buffer(
      SharedBuffer::create(data.empty() ? "" : data.data(),
                           data.length()));
  RefPtr<Image> image = BitmapImage::create();
  image->setData(buffer, true);
  return image;
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

// StatsCounters --------------------------------------------------------------

void ChromiumBridge::decrementStatsCounter(const char* name) {
  StatsCounter(name).Decrement();
}

void ChromiumBridge::incrementStatsCounter(const char* name) {
  StatsCounter(name).Increment();
}

#if USE(V8)
// TODO(evanm): remove this conversion thunk once v8 supports plain char*
// counter functions.
void ChromiumBridge::initV8CounterFunction() {
  v8::V8::SetCounterFunction(StatsTable::FindLocation);
}
#endif

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

// Trace Event ----------------------------------------------------------------

void ChromiumBridge::traceEventBegin(const char* name,
                                     void* id,
                                     const char* extra) {
  TRACE_EVENT_BEGIN(name, id, extra);
}

void ChromiumBridge::traceEventEnd(const char* name,
                                   void* id,
                                   const char* extra) {
  TRACE_EVENT_END(name, id, extra);
}

// URL ------------------------------------------------------------------------

KURL ChromiumBridge::inspectorURL() {
  return webkit_glue::GURLToKURL(webkit_glue::GetInspectorURL());
}

// Visited links --------------------------------------------------------------

WebCore::LinkHash ChromiumBridge::visitedLinkHash(const UChar* url,
                                                  unsigned length) {
  url_canon::RawCanonOutput<2048> buffer;
  url_parse::Parsed parsed;
  if (!url_util::Canonicalize(url, length, NULL, &buffer, &parsed))
    return false;  // Invalid URLs are unvisited.
  return webkit_glue::VisitedLinkHash(buffer.data(), buffer.length());
}

WebCore::LinkHash ChromiumBridge::visitedLinkHash(
    const WebCore::KURL& base,
    const WebCore::AtomicString& attributeURL) {
  // Resolve the relative URL using googleurl and pass the absolute URL up to
  // the embedder. We could create a GURL object from the base and resolve the
  // relative URL that way, but calling the lower-level functions directly
  // saves us the std::string allocation in most cases.
  url_canon::RawCanonOutput<2048> buffer;
  url_parse::Parsed parsed;

#if USE(GOOGLEURL)
  const WebCore::CString& cstr = base.utf8String();
  const char* data = cstr.data();
  int length = cstr.length();
  const url_parse::Parsed& src_parsed = base.parsed();
#else
  // When we're not using GoogleURL, first canonicalize it so we can resolve it
  // below.
  url_canon::RawCanonOutput<2048> src_canon;
  url_parse::Parsed src_parsed;
  WebCore::String str = base.string();
  if (!url_util::Canonicalize(str.characters(), str.length(), NULL,
                              &src_canon, &src_parsed))
    return false;
  const char* data = src_canon.data();
  int length = src_canon.length();
#endif

  if (!url_util::ResolveRelative(data, length, src_parsed,
                                 attributeURL.characters(),
                                 attributeURL.length(), NULL,
                                 &buffer, &parsed))
    return false;  // Invalid resolved URL.
  return webkit_glue::VisitedLinkHash(buffer.data(), buffer.length());
}

bool ChromiumBridge::isLinkVisited(WebCore::LinkHash visitedLinkHash) {
  return webkit_glue::IsLinkVisited(visitedLinkHash);
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
