// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_protocol.h"

#include "base/string_util.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "googleurl/src/url_util.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_file_job.h"

static const char kExtensionURLScheme[] = "chrome-extension";

FilePath GetPathForExtensionResource(const FilePath& extension_path,
                                     const std::string& url_path) {
  DCHECK(extension_path.IsAbsolute());
  DCHECK(url_path.length() > 0 && url_path[0] == '/');

  // Build up a file:// URL and convert that back to a FilePath.  This avoids
  // URL encoding and path separator issues.

  // Convert the extension's root to a file:// URL.
  GURL extension_url = net::FilePathToFileURL(extension_path);
  if (!extension_url.is_valid())
    return FilePath();

  // Append the requested path.
  GURL::Replacements replacements;
  std::string new_path(extension_url.path());
  new_path += url_path;
  replacements.SetPathStr(new_path);
  GURL file_url = extension_url.ReplaceComponents(replacements);
  if (!file_url.is_valid())
    return FilePath();

  // Convert the result back to a FilePath.
  FilePath ret_val;
  if (!net::FileURLToFilePath(file_url, &ret_val))
    return FilePath();

  if (!file_util::AbsolutePath(&ret_val))
    return FilePath();

  // Double-check that the path we ended up with is actually inside the
  // extension root.
  if (!extension_path.Contains(ret_val))
    return FilePath();

  return ret_val;
}

// Creates a URLRequestJob instance for an extension URL. This is the factory
// function that is registered with URLRequest.
static URLRequestJob* CreateURLRequestJob(URLRequest* request,
                                          const std::string& scheme) {
  ChromeURLRequestContext* context =
      static_cast<ChromeURLRequestContext*>(request->context());

  FilePath extension_path = context->GetPathForExtension(request->url().host());
  if (extension_path.value().empty())
    return NULL;

  FilePath path = GetPathForExtensionResource(extension_path,
                                              request->url().path());
  if (path.value().empty())
    return NULL;

  return new URLRequestFileJob(request, path);
}

void RegisterExtensionProtocol() {
  // Being a standard scheme allows us to resolve relative paths
  url_util::AddStandardScheme(kExtensionURLScheme);

  URLRequest::RegisterProtocolFactory(kExtensionURLScheme,
                                      &CreateURLRequestJob);
}
