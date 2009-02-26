// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// TODO(darin): This include should not be necessary, but for some reason the
// Mac build requires it.
#include "config.h"

#include "webkit/glue/simple_webmimeregistry_impl.h"

#include "WebString.h"

#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "net/base/mime_util.h"
#include "webkit/glue/glue_util.h"

using WebKit::WebString;

namespace webkit_glue {

bool SimpleWebMimeRegistryImpl::supportsImageMIMEType(
    const WebString& mime_type) {
  return net::IsSupportedImageMimeType(UTF16ToASCII(mime_type).c_str());
}

bool SimpleWebMimeRegistryImpl::supportsJavaScriptMIMEType(
    const WebString& mime_type) {
  return net::IsSupportedJavascriptMimeType(UTF16ToASCII(mime_type).c_str());
}

bool SimpleWebMimeRegistryImpl::supportsNonImageMIMEType(
    const WebString& mime_type) {
  return net::IsSupportedNonImageMimeType(UTF16ToASCII(mime_type).c_str());
}

WebString SimpleWebMimeRegistryImpl::mimeTypeForExtension(
    const WebString& file_extension) {
  std::string mime_type;
  net::GetMimeTypeFromExtension(
      WebStringToFilePathString(file_extension), &mime_type);
  return ASCIIToUTF16(mime_type);
}

WebString SimpleWebMimeRegistryImpl::mimeTypeFromFile(
    const WebString& file_path) {
  std::string mime_type;
  net::GetMimeTypeFromFile(
      FilePath(WebStringToFilePathString(file_path)), &mime_type);
  return ASCIIToUTF16(mime_type);
}

WebString SimpleWebMimeRegistryImpl::preferredExtensionForMIMEType(
    const WebString& mime_type) {
  FilePath::StringType file_extension;
  net::GetPreferredExtensionForMimeType(UTF16ToASCII(mime_type),
                                        &file_extension);
  return FilePathStringToWebString(file_extension);
}

}  // namespace webkit_glue
