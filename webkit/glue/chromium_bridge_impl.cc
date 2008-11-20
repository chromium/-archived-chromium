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
#include "net/base/mime_util.h"
#if USE(V8)
#include <v8.h>
#endif
#include "webkit/glue/chrome_client_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/scoped_clipboard_writer_glue.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/webkit_resources.h"
#include "webkit/glue/webview_impl.h"
#include "webkit/glue/webview_delegate.h"

#if defined(OS_WIN)
#include <windows.h>
#include <vssym32.h>

#include "base/gfx/native_theme.h"

// This is only needed on Windows right now.
#include "BitmapImageSingleFrameSkia.h"
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

bool ChromiumBridge::getPlugins(bool refresh, Vector<PluginInfo*>* plugins) {
  std::vector< ::WebPluginInfo> glue_plugins;
  if (!webkit_glue::GetPlugins(refresh, &glue_plugins))
    return false;
  for (size_t i = 0; i < glue_plugins.size(); ++i) {
    WebCore::PluginInfo* rv = new WebCore::PluginInfo;
    const ::WebPluginInfo& plugin = glue_plugins[i];
    rv->name = webkit_glue::StdWStringToString(plugin.name);
    rv->desc = webkit_glue::StdWStringToString(plugin.desc);
    rv->file = webkit_glue::StdWStringToString(
        file_util::GetFilenameFromPath(plugin.file));
    for (size_t j = 0; j < plugin.mime_types.size(); ++ j) {
      WebCore::MimeClassInfo* new_mime = new WebCore::MimeClassInfo();
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
    plugins->append(rv);
  }
  return true;
}

// Protocol -------------------------------------------------------------------

String ChromiumBridge::uiResourceProtocol() {
  return webkit_glue::StdStringToString(webkit_glue::GetUIResourceProtocol());
}


// Resources ------------------------------------------------------------------

#if defined(OS_WIN)
// Creates an Image for the text area resize corner. We do this by drawing the
// theme native control into a memory buffer then converting the memory buffer
// into an image. We don't bother caching this image because the caller holds
// onto a static copy (see WebCore/rendering/RenderLayer.cpp).
static PassRefPtr<Image> GetTextAreaResizeCorner() {
  // Get the size of the resizer.
  const int thickness = ScrollbarTheme::nativeTheme()->scrollbarThickness();

  // Setup a memory buffer.
  gfx::PlatformCanvasWin canvas(thickness, thickness, false);
  gfx::PlatformDeviceWin& device = canvas.getTopPlatformDevice();
  device.prepareForGDI(0, 0, thickness, thickness);
  HDC hdc = device.getBitmapDC();
  RECT widgetRect = { 0, 0, thickness, thickness };

  // Do the drawing.
  gfx::NativeTheme::instance()->PaintStatusGripper(hdc, SP_GRIPPER, 0, 0,
                                                   &widgetRect);
  device.postProcessGDI(0, 0, thickness, thickness);
  return BitmapImageSingleFrameSkia::create(device.accessBitmap(false));
}
#endif

PassRefPtr<Image> ChromiumBridge::loadPlatformImageResource(const char* name) {
  // Some need special handling.
  if (!strcmp(name, "textAreaResizeCorner")) {
#if defined(OS_WIN)
    return GetTextAreaResizeCorner();
#else
    DLOG(WARNING) << "This needs implementing on other platforms.";
    return Image::nullImage();
#endif
  }

  // The rest get converted to a resource ID that we can pass to the glue.
  int resource_id = 0;
  if (!strcmp(name, "missingImage")) {
    resource_id = IDR_BROKENIMAGE;
  } else if (!strcmp(name, "tickmarkDash")) {
    resource_id = IDR_TICKMARK_DASH;
  } else if (!strcmp(name, "panIcon")) {
    resource_id = IDR_PAN_SCROLL_ICON;
  } else if (!strcmp(name, "deleteButton") ||
             !strcmp(name, "deleteButtonPressed")) {
    NOTREACHED() << "Image resource " << name << " does not exist yet.";
    return Image::nullImage();
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

void ChromiumBridge::decrementStatsCounter(const wchar_t* name) {
  StatsCounter(name).Decrement();
}

void ChromiumBridge::incrementStatsCounter(const wchar_t* name) {
  StatsCounter(name).Increment();
}

#if USE(V8)
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
