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

#include "chrome/browser/google_url_tracker.h"

#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "net/base/load_flags.h"

const char GoogleURLTracker::kDefaultGoogleHomepage[] =
    "http://www.google.com/";

GoogleURLTracker::GoogleURLTracker()
    : google_url_(g_browser_process->local_state()->GetString(
          prefs::kLastKnownGoogleURL)),
#pragma warning(suppress: 4355)  // Okay to pass "this" here.
      fetcher_factory_(this) {
  // Because this function can be called during startup, when kicking off a URL
  // fetch can eat up 20 ms of time, we delay five seconds, which is hopefully
  // long enough to be after startup, but still get results back quickly.
  // Ideally, instead of this timer, we'd do something like "check if the
  // browser is starting up, and if so, come back later", but there is currently
  // no function to do this.
  static const int kStartFetchDelayMS = 5000;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      fetcher_factory_.NewRunnableMethod(&GoogleURLTracker::StartFetch),
      kStartFetchDelayMS);
}

GURL GoogleURLTracker::GoogleURL() {
  const GoogleURLTracker* const tracker =
      g_browser_process->google_url_tracker();
  return tracker ? tracker->google_url_ : GURL(kDefaultGoogleHomepage);
}

void GoogleURLTracker::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kLastKnownGoogleURL,
                            ASCIIToWide(kDefaultGoogleHomepage));
}

// static
bool GoogleURLTracker::CheckAndConvertToGoogleBaseURL(const GURL& url,
                                                      GURL* base_url) {
  // Only allow updates if the new URL appears to be on google.xx, google.co.xx,
  // or google.com.xx.  Cases other than this are either malicious, or doorway
  // pages for hotel WiFi connections and the like.
  // NOTE: Obviously the above is not as secure as whitelisting all known Google
  // frontpage domains, but for now we're trying to prevent login pages etc.
  // from ruining the user experience, rather than preventing hijacking.
  std::vector<std::string> host_components;
  SplitStringDontTrim(url.host(), '.', &host_components);
  if (host_components.size() < 2)
    return false;
  std::string& component = host_components[host_components.size() - 2];
  if (component != "google") {
    if ((host_components.size() < 3) ||
        ((component != "co") && (component != "com")))
      return false;
    if (host_components[host_components.size() - 3] != "google")
      return false;
  }

  // If the url's path does not begin "/intl/", reset it to "/".  Other paths
  // represent services such as iGoogle that are irrelevant to the baseURL.
  *base_url = url.path().compare(0, 6, "/intl/") ? url.GetWithEmptyPath() : url;
  return true;
}

void GoogleURLTracker::StartFetch() {
  fetcher_.reset(new URLFetcher(GURL(kDefaultGoogleHomepage), URLFetcher::HEAD,
                                this));
  fetcher_->set_load_flags(net::LOAD_DISABLE_CACHE);
  fetcher_->set_request_context(Profile::GetDefaultRequestContext());
  fetcher_->Start();
}

void GoogleURLTracker::OnURLFetchComplete(const URLFetcher* source,
                                          const GURL& url,
                                          const URLRequestStatus& status,
                                          int response_code,
                                          const ResponseCookies& cookies,
                                          const std::string& data) {
  // Delete the fetcher on this function's exit.
  scoped_ptr<URLFetcher> clean_up_fetcher(fetcher_.release());

  // Don't update the URL if the request didn't succeed.
  if (!status.is_success() || (response_code != 200))
    return;

  // See if the response URL was one we want to use, and if so, convert to the
  // appropriate Google base URL.
  GURL base_url;
  if (!CheckAndConvertToGoogleBaseURL(url, &base_url))
    return;

  // Update the saved base URL if it has changed.
  const std::wstring base_url_str(UTF8ToWide(base_url.spec()));
  if (g_browser_process->local_state()->GetString(prefs::kLastKnownGoogleURL) !=
      base_url_str) {
    g_browser_process->local_state()->SetString(prefs::kLastKnownGoogleURL,
                                                base_url_str);
    google_url_ = base_url;
    NotificationService::current()->Notify(NOTIFY_GOOGLE_URL_UPDATED,
                                           NotificationService::AllSources(),
                                           NotificationService::NoDetails());
  }
}
