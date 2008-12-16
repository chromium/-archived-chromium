// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "config.h"
#include "ChromiumBridge.h"

#include "BitmapImage.h"
#include "ClipboardUtilitiesChromium.h"
#include "Cursor.h"
#include "Frame.h"
#include "FrameView.h"
#include "HostWindow.h"
#include "KURL.h"
#include "NativeImageSkia.h"
#include "Page.h"
#include "PasteboardPrivate.h"
#include "PlatformString.h"
#include "PlatformWidget.h"
#include "PluginData.h"
#include "PluginInfoStore.h"
#include "ScrollbarTheme.h"
#include "ScrollView.h"
#include "SystemTime.h"
#include "Widget.h"

#undef LOG
#include "base/clipboard.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/trace_event.h"
#include "build/build_config.h"
#include "googleurl/src/url_util.h"
#include "net/base/mime_util.h"
#if USE(V8)
#include <v8.h>
#endif
#include "webkit/glue/chrome_client_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/scoped_clipboard_writer_glue.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin_impl.h"
#include "webkit/glue/webkit_resources.h"
#include "webkit/glue/webview_impl.h"
#include "webkit/glue/webview_delegate.h"

#if defined(OS_WIN)
#include <windows.h>
#include <vssym32.h>

#include "base/gfx/native_theme.h"
#endif

namespace {

PlatformWidget ToPlatform(WebCore::Widget* widget) {
  return widget ? widget->root()->hostWindow()->platformWindow() : 0;
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

std::wstring UrlToImageMarkup(const WebCore::KURL& url,
                              const WebCore::String& alt_str) {
  std::wstring markup(L"<img src=\"");
  markup.append(webkit_glue::StringToStdWString(url.string()));
  markup.append(L"\"");
  if (!alt_str.isEmpty()) {
    markup.append(L" alt=\"");
    std::wstring alt_stdstr = webkit_glue::StringToStdWString(alt_str);
    ReplaceSubstringsAfterOffset(&alt_stdstr, 0, L"\"", L"&quot;");
    markup.append(alt_stdstr);
    markup.append(L"\"");
  }
  markup.append(L"/>");
  return markup;
}

}  // namespace

namespace WebCore {

bool ChromiumBridge::clipboardIsFormatAvailable(
    PasteboardPrivate::ClipboardFormat format) {
  switch (format) {
    case PasteboardPrivate::HTMLFormat:
      return webkit_glue::ClipboardIsFormatAvailable(
          ::Clipboard::GetHtmlFormatType());

    case PasteboardPrivate::WebSmartPasteFormat:
      return webkit_glue::ClipboardIsFormatAvailable(
          ::Clipboard::GetWebKitSmartPasteFormatType());

    case PasteboardPrivate::BookmarkFormat:
#if defined(OS_WIN) || defined(OS_MACOSX)
      return webkit_glue::ClipboardIsFormatAvailable(
          ::Clipboard::GetUrlWFormatType());
#endif

    default:
      NOTREACHED();
      return false;
  }
}

String ChromiumBridge::clipboardReadPlainText() {
  if (webkit_glue::ClipboardIsFormatAvailable(
      ::Clipboard::GetPlainTextWFormatType())) {
    std::wstring text;
    webkit_glue::ClipboardReadText(&text);
    if (!text.empty())
      return webkit_glue::StdWStringToString(text);
  }

  if (webkit_glue::ClipboardIsFormatAvailable(
      ::Clipboard::GetPlainTextFormatType())) {
    std::string text;
    webkit_glue::ClipboardReadAsciiText(&text);
    if (!text.empty())
      return webkit_glue::StdStringToString(text);
  }

  return String();
}

void ChromiumBridge::clipboardReadHTML(String* html, KURL* url) {
  std::wstring html_stdstr;
  GURL gurl;
  webkit_glue::ClipboardReadHTML(&html_stdstr, &gurl);
  *html = webkit_glue::StdWStringToString(html_stdstr);
  *url = webkit_glue::GURLToKURL(gurl);
}

void ChromiumBridge::clipboardWriteSelection(const String& html,
                                             const KURL& url,
                                             const String& plain_text,
                                             bool can_smart_copy_or_delete) {
  ScopedClipboardWriterGlue scw(webkit_glue::ClipboardGetClipboard());
  scw.WriteHTML(webkit_glue::StringToStdWString(html),
                webkit_glue::CStringToStdString(url.utf8String()));
  scw.WriteText(webkit_glue::StringToStdWString(plain_text));

  if (can_smart_copy_or_delete)
    scw.WriteWebSmartPaste();
}

void ChromiumBridge::clipboardWriteURL(const KURL& url, const String& title) {
  ScopedClipboardWriterGlue scw(webkit_glue::ClipboardGetClipboard());

  GURL gurl = webkit_glue::KURLToGURL(url);
  scw.WriteBookmark(webkit_glue::StringToStdWString(title), gurl.spec());

  std::wstring link(webkit_glue::StringToStdWString(urlToMarkup(url, title)));
  scw.WriteHTML(link, "");

  scw.WriteText(ASCIIToWide(gurl.spec()));
}

void ChromiumBridge::clipboardWriteImage(const NativeImageSkia* bitmap,
    const KURL& url, const String& title) {
  ScopedClipboardWriterGlue scw(webkit_glue::ClipboardGetClipboard());

#if defined(OS_WIN)
  if (bitmap)
    scw.WriteBitmap(*bitmap);
#endif
  if (!url.isEmpty()) {
    GURL gurl = webkit_glue::KURLToGURL(url);
    scw.WriteBookmark(webkit_glue::StringToStdWString(title), gurl.spec());
    scw.WriteHTML(UrlToImageMarkup(url, title), "");
    scw.WriteText(ASCIIToWide(gurl.spec()));
  }
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

// Font -----------------------------------------------------------------------

#if defined(OS_WIN)
bool ChromiumBridge::ensureFontLoaded(HFONT font) {
  return webkit_glue::EnsureFontLoaded(font);
}
#endif

// Forms ----------------------------------------------------------------------

void ChromiumBridge::notifyFormStateChanged(const Document* doc) {
  webkit_glue::NotifyFormStateChanged(doc);
}

// JavaScript -----------------------------------------------------------------

void ChromiumBridge::notifyJSOutOfMemory(Frame* frame) {
  webkit_glue::NotifyJSOutOfMemory(frame);
}

// Language -------------------------------------------------------------------

String ChromiumBridge::computedDefaultLanguage() {
  return webkit_glue::StdWStringToString(webkit_glue::GetWebKitLocale());
}

// LayoutTestMode -------------------------------------------------------------

bool ChromiumBridge::layoutTestMode() {
  return webkit_glue::IsLayoutTestMode();
}

// MimeType -------------------------------------------------------------------

bool ChromiumBridge::isSupportedImageMIMEType(const char* mime_type) {
  return net::IsSupportedImageMimeType(mime_type);
}

bool ChromiumBridge::isSupportedJavascriptMIMEType(const char* mime_type) {
  return net::IsSupportedJavascriptMimeType(mime_type);
}

bool ChromiumBridge::isSupportedNonImageMIMEType(const char* mime_type) {
  return net::IsSupportedNonImageMimeType(mime_type);
}

bool ChromiumBridge::matchesMIMEType(const String& pattern,
                                     const String& type) {
  return net::MatchesMimeType(webkit_glue::StringToStdString(pattern), 
                              webkit_glue::StringToStdString(type));
}

String ChromiumBridge::mimeTypeForExtension(const String& ext) {
  if (ext.isEmpty())
    return String();

  std::string type;
  webkit_glue::GetMimeTypeFromExtension(webkit_glue::StringToStdWString(ext),
                                        &type);
  return webkit_glue::StdStringToString(type);
}

String ChromiumBridge::mimeTypeFromFile(const String& file_path) {
  if (file_path.isEmpty())
    return String();

  std::string type;
  webkit_glue::GetMimeTypeFromFile(webkit_glue::StringToStdWString(file_path),
                                   &type);
  return webkit_glue::StdStringToString(type);
}

String ChromiumBridge::preferredExtensionForMIMEType(const String& mime_type) {
  if (mime_type.isEmpty())
    return String();

  std::wstring stdext;
  webkit_glue::GetPreferredExtensionForMimeType(
      webkit_glue::StringToStdString(mime_type), &stdext);
  return webkit_glue::StdWStringToString(stdext);
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
    rv->file = webkit_glue::StdWStringToString(
        file_util::GetFilenameFromPath(plugin.file));
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
  } else if (!strcmp(name, "deleteButton")) {
    if (webkit_glue::IsLayoutTestMode()) {
      resource_id = IDR_EDITOR_DELETE_BUTTON;
    } else {
      NOTREACHED() << "editor deletion UI should be disabled";
      return Image::nullImage();
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

// SharedTimers ----------------------------------------------------------------
// Called by SharedTimerChromium.cpp

class SharedTimerTask;

// We maintain a single active timer and a single active task for
// setting timers directly on the platform.
static SharedTimerTask* shared_timer_task;
static void (*shared_timer_function)();

// Timer task to run in the chrome message loop.
class SharedTimerTask : public Task {
 public:
  SharedTimerTask(void (*callback)()) : callback_(callback) {}

  virtual void Run() {
    if (!callback_)
      return;
    // Since we only have one task running at a time, verify 'this' is it
    DCHECK(shared_timer_task == this);
    shared_timer_task = NULL;
    callback_();
  }

  void Cancel() {
    callback_ = NULL;
  }

 private:
  void (*callback_)();
  DISALLOW_COPY_AND_ASSIGN(SharedTimerTask);
};

void ChromiumBridge::setSharedTimerFiredFunction(void (*func)()) {
  shared_timer_function = func;
}

void ChromiumBridge::setSharedTimerFireTime(double fire_time) {
  DCHECK(shared_timer_function);
  int interval = static_cast<int>((fire_time - currentTime()) * 1000);
  if (interval < 0)
    interval = 0;

  stopSharedTimer();

  // Verify that we didn't leak the task or timer objects.
  DCHECK(shared_timer_task == NULL);
  shared_timer_task = new SharedTimerTask(shared_timer_function);
  MessageLoop::current()->PostDelayedTask(FROM_HERE, shared_timer_task,
                                          interval);
}

void ChromiumBridge::stopSharedTimer() {
  if (!shared_timer_task)
    return;
  shared_timer_task->Cancel();
  shared_timer_task = NULL;
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

// SystemTime ------------------------------------------------------------------
// Called by SystemTimeChromium.cpp

double ChromiumBridge::currentTime() {
  // Get the current time in seconds since epoch.
  return base::Time::Now().ToDoubleT();
}

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

  const WebCore::CString& cstr = base.utf8String();
  if (!url_util::ResolveRelative(cstr.data(), cstr.length(), base.parsed(),
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
