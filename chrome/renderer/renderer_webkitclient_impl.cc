// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/renderer_webkitclient_impl.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/renderer/net/render_dns_master.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/visitedlink_slave.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebString;
using WebKit::WebURL;

//------------------------------------------------------------------------------

WebKit::WebMimeRegistry* RendererWebKitClientImpl::mimeRegistry() {
  return &mime_registry_;
}

WebKit::WebSandboxSupport* RendererWebKitClientImpl::sandboxSupport() {
#if defined(OS_WIN)
  return &sandbox_support_;
#else
  return NULL;
#endif
}

uint64_t RendererWebKitClientImpl::visitedLinkHash(const char* canonical_url,
                                                   size_t length) {
  return RenderThread::current()->visited_link_slave()->ComputeURLFingerprint(
      canonical_url, length);
}

bool RendererWebKitClientImpl::isLinkVisited(uint64_t link_hash) {
  return RenderThread::current()->visited_link_slave()->IsVisited(link_hash);
}

void RendererWebKitClientImpl::setCookies(
    const WebURL& url, const WebURL& policy_url, const WebString& value) {
  std::string value_utf8;
  UTF16ToUTF8(value.data(), value.length(), &value_utf8);
  RenderThread::current()->Send(
      new ViewHostMsg_SetCookie(url, policy_url, value_utf8));
}

WebString RendererWebKitClientImpl::cookies(
    const WebURL& url, const WebURL& policy_url) {
  std::string value_utf8;
  RenderThread::current()->Send(
      new ViewHostMsg_GetCookies(url, policy_url, &value_utf8));
  return WebString::fromUTF8(value_utf8);
}

void RendererWebKitClientImpl::prefetchHostName(const WebString& hostname) {
  if (!hostname.isEmpty()) {
    std::string hostname_utf8;
    UTF16ToUTF8(hostname.data(), hostname.length(), &hostname_utf8);
    DnsPrefetchCString(hostname_utf8.data(), hostname_utf8.length());
  }
}

WebString RendererWebKitClientImpl::defaultLocale() {
  // TODO(darin): Eliminate this webkit_glue call.
  return WideToUTF16(webkit_glue::GetWebKitLocale());
}

//------------------------------------------------------------------------------

WebString RendererWebKitClientImpl::MimeRegistry::mimeTypeForExtension(
    const WebString& file_extension) {
  if (IsPluginProcess())
    return SimpleWebMimeRegistryImpl::mimeTypeForExtension(file_extension);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  std::string mime_type;
  RenderThread::current()->Send(new ViewHostMsg_GetMimeTypeFromExtension(
      webkit_glue::WebStringToFilePathString(file_extension), &mime_type));
  return ASCIIToUTF16(mime_type);

}

WebString RendererWebKitClientImpl::MimeRegistry::mimeTypeFromFile(
    const WebString& file_path) {
  if (IsPluginProcess())
    return SimpleWebMimeRegistryImpl::mimeTypeFromFile(file_path);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  std::string mime_type;
  RenderThread::current()->Send(new ViewHostMsg_GetMimeTypeFromFile(
      FilePath(webkit_glue::WebStringToFilePathString(file_path)),
      &mime_type));
  return ASCIIToUTF16(mime_type);

}

WebString RendererWebKitClientImpl::MimeRegistry::preferredExtensionForMIMEType(
    const WebString& mime_type) {
  if (IsPluginProcess())
    return SimpleWebMimeRegistryImpl::preferredExtensionForMIMEType(mime_type);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  FilePath::StringType file_extension;
  RenderThread::current()->Send(
      new ViewHostMsg_GetPreferredExtensionForMimeType(UTF16ToASCII(mime_type),
          &file_extension));
  return webkit_glue::FilePathStringToWebString(file_extension);
}

//------------------------------------------------------------------------------

#if defined(OS_WIN)

bool RendererWebKitClientImpl::SandboxSupport::ensureFontLoaded(HFONT font) {
  LOGFONT logfont;
  GetObject(font, sizeof(LOGFONT), &logfont);
  return RenderThread::current()->Send(new ViewHostMsg_LoadFont(logfont));
}

#endif
