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

#include "chrome/browser/google_util.h"

#include <string>

#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google_url_tracker.h"
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
                     WideToUTF8(g_browser_process->GetApplicationLocale()));
}

GURL AppendGoogleTLDParam(const GURL& url) {
  const std::string google_domain(
      RegistryControlledDomainService::GetDomainAndRegistry(
      GoogleURLTracker::GoogleURL()));
  const size_t first_dot = google_domain.find('.');
  if (first_dot == std::string::npos) {
    NOTREACHED();
    return url;
  }
  return AppendParam(url, "sd", google_domain.substr(first_dot + 1));
}

}  // namespace google_util
