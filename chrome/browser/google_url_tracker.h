// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/url_fetcher.h"

class PrefService;

// This object is responsible for updating the Google URL exactly once (when
// first constructed) and tracking the currently known value, which is also
// saved to a pref.
class GoogleURLTracker : public URLFetcher::Delegate {
 public:
  // Only the main browser process loop should call this, when setting up
  // g_browser_process->google_url_tracker_.  No code other than the
  // GoogleURLTracker itself should actually use
  // g_browser_process->google_url_tracker() (which shouldn't be hard, since
  // there aren't useful public functions on this object for consumers to access
  // anyway).
  GoogleURLTracker();

  // This returns the current Google URL.  If this is the first call to it in
  // this session, it also kicks off a timer to fetch the correct Google URL;
  // when that fetch completes, it will fire NOTIFY_GOOGLE_URL_UPDATED.
  //
  // This is the only function most code should ever call.
  //
  // This will return a valid URL even in unittest mode.
  static GURL GoogleURL();

  static void RegisterPrefs(PrefService* prefs);

 private:
  FRIEND_TEST(GoogleURLTrackerTest, CheckAndConvertURL);

  // Determines if |url| is an appropriate source for a new Google base URL, and
  // update |base_url| to the appropriate base URL if so.  Returns whether the
  // check succeeded (and thus whether |base_url| was actually updated).
  static bool CheckAndConvertToGoogleBaseURL(const GURL& url, GURL* base_url);

  // Starts the fetch of the up-to-date Google URL.
  void StartFetch();

  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher *source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  static const char kDefaultGoogleHomepage[];

  GURL google_url_;
  ScopedRunnableMethodFactory<GoogleURLTracker> fetcher_factory_;
  scoped_ptr<URLFetcher> fetcher_;

  DISALLOW_EVIL_CONSTRUCTORS(GoogleURLTracker);
};

