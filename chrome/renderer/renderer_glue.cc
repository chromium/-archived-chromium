// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides the embedder's side of random webkit glue functions.

#include "build/build_config.h"

#include <vector>

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
#include "googleurl/src/url_util.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKitClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "webkit/glue/scoped_clipboard_writer_glue.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webkit_glue.h"

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
// This definition of WriteBitmapFromPixels uses shared memory to communicate
// across processes.
void ScopedClipboardWriterGlue::WriteBitmapFromPixels(const void* pixels,
                                                      const gfx::Size& size) {
  // Do not try to write a bitmap more than once
  if (shared_buf_)
    return;

  size_t buf_size = 4 * size.width() * size.height();

  // Allocate a shared memory buffer to hold the bitmap bits
  shared_buf_ = new base::SharedMemory;
  const bool created = shared_buf_ && shared_buf_->Create(
      L"", false /* read write */, true /* open existing */, buf_size);
  if (!shared_buf_ || !created || !shared_buf_->Map(buf_size)) {
    NOTREACHED();
    return;
  }

  // Copy the bits into shared memory
  memcpy(shared_buf_->memory(), pixels, buf_size);
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
    RenderThread::current()->Send(
        new ViewHostMsg_ClipboardWriteObjectsSync(objects_));
    delete shared_buf_;
    return;
  }
#endif

  RenderThread::current()->Send(
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

void PrecacheUrl(const wchar_t* url, int url_length) {
  // TBD: jar: Need implementation that loads the targetted URL into our cache.
  // For now, at least prefetch DNS lookup
  std::string url_string;
  WideToUTF8(url, url_length, &url_string);
  const std::string host = GURL(url_string).host();
  if (!host.empty())
    DnsPrefetchCString(host.data(), host.length());
}

void AppendToLog(const char* file, int line, const char* msg) {
  logging::LogMessage(file, line).stream() << msg;
}

std::string GetDataResource(int resource_id) {
  return ResourceBundle::GetSharedInstance().GetDataResource(resource_id);
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

bool ClipboardIsFormatAvailable(const Clipboard::FormatType& format) {
  bool result;
  RenderThread::current()->Send(
      new ViewHostMsg_ClipboardIsFormatAvailable(format, &result));
  return result;
}

void ClipboardReadText(string16* result) {
  RenderThread::current()->Send(new ViewHostMsg_ClipboardReadText(result));
}

void ClipboardReadAsciiText(std::string* result) {
  RenderThread::current()->Send(new ViewHostMsg_ClipboardReadAsciiText(result));
}

void ClipboardReadHTML(string16* markup, GURL* url) {
  RenderThread::current()->Send(new ViewHostMsg_ClipboardReadHTML(markup, url));
}

GURL GetInspectorURL() {
  return GURL("chrome-ui://inspector/inspector.html");
}

std::string GetUIResourceProtocol() {
  return "chrome-ui";
}

bool GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins) {
  return RenderThread::current()->Send(
      new ViewHostMsg_GetPlugins(refresh, plugins));
}

// static factory function
ResourceLoaderBridge* ResourceLoaderBridge::Create(
    const std::string& method,
    const GURL& url,
    const GURL& policy_url,
    const GURL& referrer,
    const std::string& frame_origin,
    const std::string& main_frame_origin,
    const std::string& headers,
    const std::string& default_mime_type,
    int load_flags,
    int origin_pid,
    ResourceType::Type resource_type,
    int routing_id) {
  ResourceDispatcher* dispatch = RenderThread::current()->resource_dispatcher();
  return dispatch->CreateBridge(method, url, policy_url, referrer,
                                frame_origin, main_frame_origin, headers,
                                default_mime_type, load_flags, origin_pid,
                                resource_type, 0, routing_id);
}

void NotifyCacheStats() {
  // Update the browser about our cache
  // NOTE: Since this can be called from the plugin process, we might not have
  // a RenderThread.  Do nothing in that case.
  if (!IsPluginProcess())
    RenderThread::current()->InformHostOfCacheStatsLater();
}


}  // namespace webkit_glue
