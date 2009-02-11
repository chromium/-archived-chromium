// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides the embedder's side of random webkit glue functions.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <wininet.h>
#endif

#include "base/clipboard.h"
#include "base/scoped_clipboard_writer.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/renderer/net/render_dns_master.h"
#include "chrome/renderer/render_process.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/visitedlink_slave.h"
#include "googleurl/src/url_util.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "webkit/glue/scoped_clipboard_writer_glue.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webkit_glue.h"

#include <vector>

#include "SkBitmap.h"

#if defined(OS_WIN)
#include <strsafe.h>  // note: per msdn docs, this must *follow* other includes
#endif

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

#if defined(OS_WIN)
// This definition of WriteBitmap uses shared memory to communicate across
// processes.
void ScopedClipboardWriterGlue::WriteBitmap(const SkBitmap& bitmap) {
  // do not try to write a bitmap more than once
  if (shared_buf_)
    return;

  size_t buf_size = bitmap.getSize();
  gfx::Size size(bitmap.width(), bitmap.height());

  // Allocate a shared memory buffer to hold the bitmap bits
  shared_buf_ = RenderProcess::AllocSharedMemory(buf_size);
  if (!shared_buf_ || !shared_buf_->Map(buf_size)) {
    NOTREACHED();
    return;
  }

  // Copy the bits into shared memory
  SkAutoLockPixels bitmap_lock(bitmap);
  memcpy(shared_buf_->memory(), bitmap.getPixels(), buf_size);
  shared_buf_->Unmap();

  Clipboard::ObjectMapParam param1, param2;
  base::SharedMemoryHandle smh = shared_buf_->handle();

  const char* shared_handle = reinterpret_cast<const char*>(&smh);
  for (size_t i = 0; i < sizeof base::SharedMemoryHandle; i++)
    param1.push_back(shared_handle[i]);

  const char* size_data = reinterpret_cast<const char*>(&size);
  for (size_t i = 0; i < sizeof gfx::Size; i++)
    param2.push_back(size_data[i]);

  Clipboard::ObjectMapParams params;
  params.push_back(param1);
  params.push_back(param2);
  objects_[Clipboard::CBF_SMBITMAP] = params;
}
#endif

// Define a destructor that makes IPCs to flush the contents to the
// system clipboard.
ScopedClipboardWriterGlue::~ScopedClipboardWriterGlue() {
  if (objects_.empty())
    return;

#if defined(OS_WIN)
  if (shared_buf_) {
    g_render_thread->Send(
        new ViewHostMsg_ClipboardWriteObjectsSync(objects_));
    RenderProcess::FreeSharedMemory(shared_buf_);
    return;
  }
#endif

  g_render_thread->Send(
      new ViewHostMsg_ClipboardWriteObjectsAsync(objects_));
}

namespace webkit_glue {

// Global variable set during RenderProcess::GlobalInit if video was enabled
// and our media libraries were successfully loaded.
static bool g_media_player_available = false;

void SetMediaPlayerAvailable(bool value) {
  g_media_player_available = value;
}

bool IsMediaPlayerAvailable() {
  return g_media_player_available;
}

void PrefetchDns(const std::string& hostname) {
  if (!hostname.empty())
    DnsPrefetchCString(hostname.c_str(), hostname.length());
}

void PrecacheUrl(const wchar_t* url, int url_length) {
  // TBD: jar: Need implementation that loads the targetted URL into our cache.
  // For now, at least prefetch DNS lookup
  GURL parsed_url(WideToUTF8(std::wstring(url, url_length)));
  PrefetchDns(parsed_url.host());
}

void AppendToLog(const char* file, int line, const char* msg) {
  logging::LogMessage(file, line).stream() << msg;
}

bool GetMimeTypeFromExtension(const std::wstring &ext,
                              std::string *mime_type) {
  if (IsPluginProcess())
    return net::GetMimeTypeFromExtension(ext, mime_type);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  DCHECK(mime_type->empty());
  g_render_thread->Send(
      new ViewHostMsg_GetMimeTypeFromExtension(ext, mime_type));
  return !mime_type->empty();
}

bool GetMimeTypeFromFile(const std::wstring &file_path,
                         std::string *mime_type) {
  if (IsPluginProcess())
    return net::GetMimeTypeFromFile(file_path, mime_type);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  DCHECK(mime_type->empty());
  g_render_thread->Send(
      new ViewHostMsg_GetMimeTypeFromFile(file_path, mime_type));
  return !mime_type->empty();
}

bool GetPreferredExtensionForMimeType(const std::string& mime_type,
                                      std::wstring* ext) {
  if (IsPluginProcess())
    return net::GetPreferredExtensionForMimeType(mime_type, ext);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  DCHECK(ext->empty());
  g_render_thread->Send(
      new ViewHostMsg_GetPreferredExtensionForMimeType(mime_type, ext));
  return !ext->empty();
}

std::string GetDataResource(int resource_id) {
  return ResourceBundle::GetSharedInstance().GetDataResource(resource_id);
}

SkBitmap* GetBitmapResource(int resource_id) {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(resource_id);
}

#if defined(OS_WIN)
HCURSOR LoadCursor(int cursor_id) {
  return ResourceBundle::GetSharedInstance().LoadCursor(cursor_id);
}
#endif

// Clipboard glue

Clipboard* ClipboardGetClipboard(){
  return NULL;
}

#if defined(OS_LINUX)
// TODO(port): This should replace the method below (the unsigned int is a
// windows type).  We may need to convert the type of format so it can be sent
// over IPC.
bool ClipboardIsFormatAvailable(Clipboard::FormatType format) {
  NOTIMPLEMENTED();
  return false;
}
#endif

bool ClipboardIsFormatAvailable(unsigned int format) {
  bool result;
  g_render_thread->Send(
      new ViewHostMsg_ClipboardIsFormatAvailable(format, &result));
  return result;
}

void ClipboardReadText(std::wstring* result) {
  g_render_thread->Send(new ViewHostMsg_ClipboardReadText(result));
}

void ClipboardReadAsciiText(std::string* result) {
  g_render_thread->Send(new ViewHostMsg_ClipboardReadAsciiText(result));
}

void ClipboardReadHTML(std::wstring* markup, GURL* url) {
  g_render_thread->Send(new ViewHostMsg_ClipboardReadHTML(markup, url));
}

GURL GetInspectorURL() {
  return GURL("chrome-ui://inspector/inspector.html");
}

std::string GetUIResourceProtocol() {
  return "chrome";
}

bool GetPlugins(bool refresh,
                             std::vector<WebPluginInfo>* plugins) {
  return g_render_thread->Send(
      new ViewHostMsg_GetPlugins(refresh, plugins));
}

#if defined(OS_WIN)
bool EnsureFontLoaded(HFONT font) {
  LOGFONT logfont;
  GetObject(font, sizeof(LOGFONT), &logfont);
  return g_render_thread->Send(new ViewHostMsg_LoadFont(logfont));
}
#endif

webkit_glue::ScreenInfo GetScreenInfo(gfx::NativeViewId window) {
  webkit_glue::ScreenInfo results;
  g_render_thread->Send(
      new ViewHostMsg_GetScreenInfo(window, &results));
  return results;
}

uint64 VisitedLinkHash(const char* canonical_url, size_t length) {
  return g_render_thread->visited_link_slave()->ComputeURLFingerprint(
      canonical_url, length);
}

bool IsLinkVisited(uint64 link_hash) {
#if defined(OS_WIN)
  return g_render_thread->visited_link_slave()->IsVisited(link_hash);
#elif defined(OS_POSIX)
  // TODO(port): Currently we don't have a HistoryService. This stops the
  // VisitiedLinkMaster from sucessfully calling Init(). In that case, no
  // message is ever sent to the renderer with the VisitiedLink shared memory
  // region and we end up crashing with SIGFPE as we try to hash by taking a
  // fingerprint mod 0.
  NOTIMPLEMENTED();
  return false;
#endif
}

int ResolveProxyFromRenderThread(const GURL& url, std::string* proxy_result) {
  // Send a synchronous IPC from renderer process to the browser process to
  // resolve the proxy. (includes --single-process case).
  int net_error;
  bool ipc_ok = g_render_thread->Send(
      new ViewHostMsg_ResolveProxy(url, &net_error, proxy_result));
  return ipc_ok ? net_error : net::ERR_UNEXPECTED;
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
  g_render_thread->Send(new ViewHostMsg_SetCookie(url, policy_url, cookie));
}

std::string GetCookies(const GURL& url, const GURL& policy_url) {
  std::string cookies;
  g_render_thread->Send(new ViewHostMsg_GetCookies(url, policy_url, &cookies));
  return cookies;
}

void NotifyCacheStats() {
  // Update the browser about our cache
  // NOTE: Since this can be called from the plugin process, we might not have
  // a RenderThread.  Do nothing in that case.
  if (g_render_thread)
    g_render_thread->InformHostOfCacheStatsLater();
}

#endif  // !USING_SIMPLE_RESOURCE_LOADER_BRIDGE

}  // namespace webkit_glue
