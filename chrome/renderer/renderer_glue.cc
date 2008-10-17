// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides the embedder's side of random webkit glue functions.

#include <windows.h>
#include <wininet.h>

#include "chrome/renderer/net/render_dns_master.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/visitedlink_slave.h"
#include "googleurl/src/url_util.h"
#include "net/base/mime_util.h"
#include "webkit/glue/webframe.h"
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

void PrefetchDns(const std::string& hostname) {
  if (!hostname.empty())
    DnsPrefetchCString(hostname.c_str(), hostname.length());
}

void PrecacheUrl(const wchar_t* url, int url_length) {
  // TBD: jar: Need implementation that loads the targetted URL into our cache.
  // For now, at least prefetch DNS lookup
  GURL parsed_url(std::wstring(url, url_length));
  PrefetchDns(parsed_url.host());
}

void webkit_glue::AppendToLog(const char* file, int line, const char* msg) {
  logging::LogMessage(file, line).stream() << msg;
}

bool webkit_glue::GetMimeTypeFromExtension(const std::wstring &ext,
                                           std::string *mime_type) {
  if (IsPluginProcess())
    return net::GetMimeTypeFromExtension(ext, mime_type);

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
    return net::GetMimeTypeFromFile(file_path, mime_type);

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
    return net::GetPreferredExtensionForMimeType(mime_type, ext);

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

std::string webkit_glue::GetDataResource(int resource_id) {
  return ResourceBundle::GetSharedInstance().GetDataResource(resource_id);
}

SkBitmap* webkit_glue::GetBitmapResource(int resource_id) {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(resource_id);
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

GURL webkit_glue::GetInspectorURL() {
  return GURL("chrome-resource://inspector/inspector.html");
}

std::string webkit_glue::GetUIResourceProtocol() {
  return "chrome-resource";
}

bool webkit_glue::GetPlugins(bool refresh,
                             std::vector<WebPluginInfo>* plugins) {
  return RenderThread::current()->Send(
      new ViewHostMsg_GetPlugins(refresh, plugins));
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

