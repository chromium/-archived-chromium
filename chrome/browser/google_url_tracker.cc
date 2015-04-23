// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_url_tracker.h"

#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"

const char GoogleURLTracker::kDefaultGoogleHomepage[] =
    "http://www.google.com/";

GoogleURLTracker::GoogleURLTracker()
    : google_url_(WideToUTF8(g_browser_process->local_state()->GetString(
          prefs::kLastKnownGoogleURL))),
      ALLOW_THIS_IN_INITIALIZER_LIST(fetcher_factory_(this)),
      in_startup_sleep_(true),
      already_fetched_(false),
      need_to_fetch_(false),
      request_context_available_(!!Profile::GetDefaultRequestContext()) {
  registrar_.Add(this, NotificationType::DEFAULT_REQUEST_CONTEXT_AVAILABLE,
                 NotificationService::AllSources());

  // Because this function can be called during startup, when kicking off a URL
  // fetch can eat up 20 ms of time, we delay five seconds, which is hopefully
  // long enough to be after startup, but still get results back quickly.
  // Ideally, instead of this timer, we'd do something like "check if the
  // browser is starting up, and if so, come back later", but there is currently
  // no function to do this.
  static const int kStartFetchDelayMS = 5000;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      fetcher_factory_.NewRunnableMethod(&GoogleURLTracker::FinishSleep),
      kStartFetchDelayMS);
}

GoogleURLTracker::~GoogleURLTracker() {
}

// static
GURL GoogleURLTracker::GoogleURL() {
  const GoogleURLTracker* const tracker =
      g_browser_process->google_url_tracker();
  return tracker ? tracker->google_url_ : GURL(kDefaultGoogleHomepage);
}

// static
void GoogleURLTracker::RequestServerCheck() {
  GoogleURLTracker* const tracker = g_browser_process->google_url_tracker();
  if (tracker)
    tracker->SetNeedToFetch();
}

// static
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

void GoogleURLTracker::SetNeedToFetch() {
  need_to_fetch_ = true;
  StartFetchIfDesirable();
}

void GoogleURLTracker::FinishSleep() {
  in_startup_sleep_ = false;
  StartFetchIfDesirable();
}

void GoogleURLTracker::StartFetchIfDesirable() {
  // Bail if a fetch isn't appropriate right now.  This function will be called
  // again each time one of the preconditions changes, so we'll fetch
  // immediately once all of them are met.
  //
  // See comments in header on the class, on RequestServerCheck(), and on the
  // various members here for more detail on exactly what the conditions are.
  if (in_startup_sleep_ || already_fetched_ || !need_to_fetch_ ||
      !request_context_available_)
    return;

  need_to_fetch_ = false;
  already_fetched_ = true;  // If fetching fails, we don't bother to reset this
                            // flag; we just live with an outdated URL for this
                            // run of the browser.
  fetcher_.reset(new URLFetcher(GURL(kDefaultGoogleHomepage), URLFetcher::HEAD,
                                this));
  // We don't want this fetch to affect existing state in the profile.  For
  // example, if a user has no Google cookies, this automatic check should not
  // cause one to be set, lest we alarm the user.
  fetcher_->set_load_flags(net::LOAD_DISABLE_CACHE |
                           net::LOAD_DO_NOT_SAVE_COOKIES);
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
    NotificationService::current()->Notify(NotificationType::GOOGLE_URL_UPDATED,
                                           NotificationService::AllSources(),
                                           NotificationService::NoDetails());
  }
}

void GoogleURLTracker::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  DCHECK_EQ(NotificationType::DEFAULT_REQUEST_CONTEXT_AVAILABLE, type.value);
  request_context_available_ = true;
  StartFetchIfDesirable();
}
