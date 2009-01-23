// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_protocols.h"

#include "base/string_util.h"
#include "chrome/browser/extensions/extension.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "googleurl/src/url_util.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_file_job.h"

// Defined in extension.h.
extern const char kExtensionURLScheme[];
extern const char kUserScriptURLScheme[];

// Factory registered with URLRequest to create URLRequestJobs for extension://
// URLs.
static URLRequestJob* CreateExtensionURLRequestJob(URLRequest* request,
                                                   const std::string& scheme) {
  ChromeURLRequestContext* context =
      static_cast<ChromeURLRequestContext*>(request->context());

  // chrome-extension://extension-id/resource/path.js
  FilePath directory_path = context->GetPathForExtension(request->url().host());
  if (directory_path.value().empty())
    return NULL;

  std::string resource = request->url().path();
  FilePath path = Extension::GetResourcePath(directory_path, resource);

  return new URLRequestFileJob(request, path);
}

// Factory registered with URLRequest to create URLRequestJobs for
// chrome-user-script:/ URLs.
static URLRequestJob* CreateUserScriptURLRequestJob(URLRequest* request,
                                                    const std::string& scheme) {
  ChromeURLRequestContext* context =
      static_cast<ChromeURLRequestContext*>(request->context());

  // chrome-user-script:/user-script-name.user.js
  FilePath directory_path = context->user_script_dir_path();
  std::string resource = request->url().path();

  FilePath path = Extension::GetResourcePath(directory_path, resource);
  return new URLRequestFileJob(request, path);
}

void RegisterExtensionProtocols() {
  // Being a standard scheme allows us to resolve relative paths. This is used
  // by extensions, but not by standalone user scripts.
  url_util::AddStandardScheme(kExtensionURLScheme);

  URLRequest::RegisterProtocolFactory(kExtensionURLScheme,
                                      &CreateExtensionURLRequestJob);
  URLRequest::RegisterProtocolFactory(kUserScriptURLScheme,
                                      &CreateUserScriptURLRequestJob);
}
