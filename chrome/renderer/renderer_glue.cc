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

// This file provides the embedder's side of random webkit glue functions.

#include <windows.h>
#include <wininet.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/renderer/net/render_dns_master.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/resource_dispatcher.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/renderer/render_process.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/visitedlink_slave.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_util.h"
#include "net/base/mime_util.h"
#include "webkit/glue/resource_type.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/webkit_glue.h"

#include "SkBitmap.h"

#include <strsafe.h>  // note: per msdn docs, this must *follow* other includes

template <typename T, size_t stack_capacity>
class ResizableStackArray {
 public:
  ResizableStackArray()
      : cur_buffer_(stack_buffer_), cur_capacity_(stack_capacity) {
  }
  ~ResizableStackArray() {
    FreeHeap();
  }

  T* get() const {
    return cur_buffer_;
  }

  T& operator[](size_t i) {
    return cur_buffer_[i];
  }

  size_t capacity() const {
    return cur_capacity_;
  }

  void Resize(size_t new_size) {
    if (new_size < cur_capacity_)
      return;  // already big enough
    FreeHeap();
    cur_capacity_ = new_size;
    cur_buffer_ = new T[new_size];
  }

 private:
  // Resets the heap buffer, if any
  void FreeHeap() {
    if (cur_buffer_ != stack_buffer_) {
      delete[] cur_buffer_;
      cur_buffer_ = stack_buffer_;
      cur_capacity_ = stack_capacity;
    }
  }

  T stack_buffer_[stack_capacity];
  T* cur_buffer_;
  size_t cur_capacity_;
};

namespace webkit_glue {

bool HistoryContains(const wchar_t* url, int url_length,
                     const char* document_host, int document_host_length,
                     bool is_dns_prefetch_enabled) {
  if (url_length == 0)
    return false;  // Empty URLs are not visited.

  // Use big stack buffer to avoid allocation when possible.
  url_parse::Parsed parsed;
  url_canon::RawCanonOutput<2048> canon;

  if (!url_util::Canonicalize(url, url_length, NULL, &canon, &parsed))
    return false;  // Invalid URLs are not visited.

  char* parsed_host = canon.data() + parsed.host.begin;

  // If the hostnames match or is_dns_prefetch_enabled is true, do the prefetch.
  if (parsed.host.is_nonempty()) {
    if (is_dns_prefetch_enabled ||
        (document_host_length > 0 && parsed.host.len == document_host_length &&
         strncmp(parsed_host, document_host, parsed.host.len) == 0))
      DnsPrefetchCString(parsed_host, parsed.host.len);
  }

  return RenderThread::current()->visited_link_slave()->
      IsVisited(canon.data(), canon.length());
}

void DnsPrefetchUrl(const wchar_t* url, int url_length) {
  if (url_length == 0)
    return;  // Empty URLs have no hostnames.

  // Use big stack buffer to avoid allocation when possible.
  url_parse::Parsed parsed;
  url_canon::RawCanonOutput<2048> canon;

  if (!url_util::Canonicalize(url, url_length, NULL, &canon, &parsed))
    return;  // Invalid URLs don't have hostnames.

  // Call for prefetching without creating a std::string().
  if (parsed.host.is_nonempty())
    DnsPrefetchCString(canon.data() + parsed.host.begin, parsed.host.len);
}

void PrecacheUrl(const wchar_t* url, int url_length) {
  // TBD: jar: Need implementation that loads the targetted URL into our cache.
  // For now, at least prefetch DNS lookup
  DnsPrefetchUrl(url, url_length);
}

void webkit_glue::AppendToLog(const char* file, int line, const char* msg) {
  logging::LogMessage(file, line).stream() << msg;
}

bool webkit_glue::GetMimeTypeFromExtension(std::wstring &ext,
                                           std::string *mime_type) {
  if (IsPluginProcess())
    return mime_util::GetMimeTypeFromExtension(ext, mime_type);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  DCHECK(mime_type->empty());
  RenderThread::current()->Send(
      new ViewHostMsg_GetMimeTypeFromExtension(ext, mime_type));
  return !mime_type->empty();
}

bool webkit_glue::GetMimeTypeFromFile(const std::wstring &file_path,
                                      std::string *mime_type) {
  if (IsPluginProcess())
    return mime_util::GetMimeTypeFromFile(file_path, mime_type);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  DCHECK(mime_type->empty());
  RenderThread::current()->Send(
      new ViewHostMsg_GetMimeTypeFromFile(file_path, mime_type));
  return !mime_type->empty();
}

bool webkit_glue::GetPreferredExtensionForMimeType(const std::string& mime_type,
                                                   std::wstring* ext) {
  if (IsPluginProcess())
    return mime_util::GetPreferredExtensionForMimeType(mime_type, ext);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  DCHECK(ext->empty());
  RenderThread::current()->Send(
      new ViewHostMsg_GetPreferredExtensionForMimeType(mime_type, ext));
  return !ext->empty();
}

IMLangFontLink2* webkit_glue::GetLangFontLink() {
  return RenderProcess::GetLangFontLink();
}

std::wstring webkit_glue::GetLocalizedString(int message_id) {
  return ResourceBundle::GetSharedInstance().GetLocalizedString(message_id);
}

std::string webkit_glue::GetDataResource(int resource_id) {
  return ResourceBundle::GetSharedInstance().GetDataResource(resource_id);
}

HCURSOR webkit_glue::LoadCursor(int cursor_id) {
  return ResourceBundle::GetSharedInstance().LoadCursor(cursor_id);
}

// Clipboard glue

void webkit_glue::ClipboardClear() {
  RenderThread::current()->Send(new ViewHostMsg_ClipboardClear());
}

void webkit_glue::ClipboardWriteText(const std::wstring& text) {
  RenderThread::current()->Send(new ViewHostMsg_ClipboardWriteText(text));
}

void webkit_glue::ClipboardWriteHTML(const std::wstring& html,
                                     const GURL& url) {
  RenderThread::current()->Send(new ViewHostMsg_ClipboardWriteHTML(html, url));
}

void webkit_glue::ClipboardWriteBookmark(const std::wstring& title,
                                         const GURL& url) {
  RenderThread::current()->Send(
      new ViewHostMsg_ClipboardWriteBookmark(title, url));
}

// Here we need to do some work to marshal the bitmap through shared memory
void webkit_glue::ClipboardWriteBitmap(const SkBitmap& bitmap) {
  size_t buf_size = bitmap.getSize();
  gfx::Size size(bitmap.width(), bitmap.height());

  // Allocate a shared memory buffer to hold the bitmap bits
  SharedMemory* shared_buf =
      RenderProcess::AllocSharedMemory(buf_size);
  if (!shared_buf) {
    NOTREACHED();
    return;
  }
  if (!shared_buf->Map(buf_size)) {
    NOTREACHED();
    return;
  }

  // Copy the bits into shared memory
  SkAutoLockPixels bitmap_lock(bitmap);
  memcpy(shared_buf->memory(), bitmap.getPixels(), buf_size);
  shared_buf->Unmap();

  // Send the handle over synchronous IPC
  RenderThread::current()->Send(
      new ViewHostMsg_ClipboardWriteBitmap(shared_buf->handle(), size));

  // The browser should be done with the bitmap now.  It's our job to free
  // the shared memory.
  RenderProcess::FreeSharedMemory(shared_buf);
}

void webkit_glue::ClipboardWriteWebSmartPaste() {
  RenderThread::current()->Send(new ViewHostMsg_ClipboardWriteWebSmartPaste());
}

bool webkit_glue::ClipboardIsFormatAvailable(unsigned int format) {
  bool result;
  RenderThread::current()->Send(
      new ViewHostMsg_ClipboardIsFormatAvailable(format, &result));
  return result;
}

void webkit_glue::ClipboardReadText(std::wstring* result) {
  RenderThread::current()->Send(new ViewHostMsg_ClipboardReadText(result));
}

void webkit_glue::ClipboardReadAsciiText(std::string* result) {
  RenderThread::current()->Send(new ViewHostMsg_ClipboardReadAsciiText(result));
}

void webkit_glue::ClipboardReadHTML(std::wstring* markup, GURL* url) {
  RenderThread::current()->Send(new ViewHostMsg_ClipboardReadHTML(markup, url));
}


bool webkit_glue::GetApplicationDirectory(std::wstring *path) {
  return PathService::Get(chrome::DIR_APP, path);
}

GURL webkit_glue::GetInspectorURL() {
  return GURL("chrome-resource://inspector/inspector.html");
}

std::string webkit_glue::GetUIResourceProtocol() {
  return "chrome-resource";
}

bool webkit_glue::GetExeDirectory(std::wstring *path) {
  return PathService::Get(base::DIR_EXE, path);
}

bool webkit_glue::GetPlugins(bool refresh,
                             std::vector<WebPluginInfo>* plugins) {
  return RenderThread::current()->Send(
      new ViewHostMsg_GetPlugins(refresh, plugins));
}

bool webkit_glue::IsPluginRunningInRendererProcess() {
  return !IsPluginProcess();
}

bool webkit_glue::EnsureFontLoaded(HFONT font) {
  LOGFONT logfont;
  GetObject(font, sizeof(LOGFONT), &logfont);
  return RenderThread::current()->Send(new ViewHostMsg_LoadFont(logfont));
}

MONITORINFOEX webkit_glue::GetMonitorInfoForWindow(HWND window) {
  MONITORINFOEX monitor_info;
  RenderThread::current()->Send(
      new ViewHostMsg_GetMonitorInfoForWindow(window, &monitor_info));
  return monitor_info;
}

std::wstring webkit_glue::GetWebKitLocale() {
  // The browser process should have passed the locale to the renderer via the
  // --lang command line flag.
  CommandLine parsed_command_line;
  const std::wstring& lang =
      parsed_command_line.GetSwitchValue(switches::kLang);
  DCHECK(!lang.empty());
  return lang;
}

#ifndef USING_SIMPLE_RESOURCE_LOADER_BRIDGE

// Each RenderView has a ResourceDispatcher.  In unit tests, this function may
// not work properly since there may be a ResourceDispatcher w/o a RenderView.
// The WebView's delegate may be null, which typically happens as a WebView is
// being closed (but it is also possible that it could be null at other times
// since WebView has a SetDelegate method).
static ResourceDispatcher* GetResourceDispatcher(WebFrame* frame) {
  WebViewDelegate* d = frame->GetView()->GetDelegate();
  return d ? static_cast<RenderView*>(d)->resource_dispatcher() : NULL;
}

// static factory function
ResourceLoaderBridge* ResourceLoaderBridge::Create(
    WebFrame* webframe,
    const std::string& method,
    const GURL& url,
    const GURL& policy_url,
    const GURL& referrer,
    const std::string& headers,
    int load_flags,
    int origin_pid,
    ResourceType::Type resource_type,
    bool mixed_content) {
  // TODO(darin): we need to eliminate the webframe parameter because webkit
  // does not always supply it (see ResourceHandle::loadResourceSynchronously).
  // Instead we should add context to ResourceRequest, which will be easy to do
  // once we merge to the latest WebKit (r23806 at least).
  if (!webframe) {
    NOTREACHED() << "no webframe";
    return NULL;
  }
  ResourceDispatcher* dispatcher = GetResourceDispatcher(webframe);
  if (!dispatcher) {
    DLOG(WARNING) << "no resource dispatcher";
    return NULL;
  }
  return dispatcher->CreateBridge(method, url, policy_url, referrer, headers,
                                  load_flags, origin_pid, resource_type,
                                  mixed_content, 0);
}

void SetCookie(const GURL& url, const GURL& policy_url,
               const std::string& cookie) {
  RenderThread::current()->Send(new ViewHostMsg_SetCookie(url, policy_url,
                                                          cookie));
}

std::string GetCookies(const GURL& url, const GURL& policy_url) {
  std::string cookies;
  RenderThread::current()->Send(new ViewHostMsg_GetCookies(url, policy_url,
                                                           &cookies));
  return cookies;
}

void NotifyCacheStats() {
  // Update the browser about our cache
  // NOTE: Since this can be called from the plugin process, we might not have
  // a RenderThread.  Do nothing in that case.
  if (RenderThread::current())
    RenderThread::current()->InformHostOfCacheStatsLater();
}

#endif  // !USING_SIMPLE_RESOURCE_LOADER_BRIDGE

}  // namespace webkit_glue
