// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_util.h"

#include <string>

#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google_url_tracker.h"
#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domain.h"

namespace {

// A helper method for adding a query param to |url|.
GURL AppendParam(const GURL& url,
                 const std::string& param_name,
                 const std::string& param_value) {
  std::string query(url.query());
  if (!query.empty())
    query += "&";
  query += param_name + "=" + param_value;
  GURL::Replacements repl;
  repl.SetQueryStr(query);
  return url.ReplaceComponents(repl);
}

}  // anonymous namespace

namespace google_util {

GURL AppendGoogleLocaleParam(const GURL& url) {
  return AppendParam(url, "hl",
                     g_browser_process->GetApplicationLocale());
}

GURL AppendGoogleTLDParam(const GURL& url) {
  const std::string google_domain(
      net::RegistryControlledDomainService::GetDomainAndRegistry(
          GoogleURLTracker::GoogleURL()));
  const size_t first_dot = google_domain.find('.');
  if (first_dot == std::string::npos) {
    NOTREACHED();
    return url;
  }
  return AppendParam(url, "sd", google_domain.substr(first_dot + 1));
}

}  // namespace google_util
