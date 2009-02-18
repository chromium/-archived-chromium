// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_URL_TRACKER_H_
#define CHROME_BROWSER_GOOGLE_URL_TRACKER_H_

#include "chrome/browser/net/url_fetcher.h"
#include "chrome/common/notification_observer.h"
#include "googleurl/src/gurl.h"

class PrefService;

// This object is responsible for updating the Google URL at most once per run,
// and tracking the currently known value, which is also saved to a pref.
//
// Most consumers should only call GoogleURL(), which is guaranteed to
// synchronously return a value at all times (even during startup or in unittest
// mode).  Consumers who need to be notified when things change should listen to
// the notification service for NOTIFY_GOOGLE_URL_UPDATED, and call GoogleURL()
// again after receiving it, in order to get the updated value.
//
// To protect users' privacy and reduce server load, no updates will be
// performed (ever) unless at least one consumer registers interest by calling
// RequestServerCheck().
class GoogleURLTracker : public URLFetcher::Delegate,
                         public NotificationObserver {
 public:
  // Only the main browser process loop should call this, when setting up
  // g_browser_process->google_url_tracker_.  No code other than the
  // GoogleURLTracker itself should actually use
  // g_browser_process->google_url_tracker() (which shouldn't be hard, since
  // there aren't useful public functions on this object for consumers to access
  // anyway).
  GoogleURLTracker();

  ~GoogleURLTracker();

  // Returns the current Google URL.  This will return a valid URL even in
  // unittest mode.
  //
  // This is the only function most code should ever call.
  static GURL GoogleURL();

  // Requests that the tracker perform a server check to update the Google URL
  // as necessary.  This will happen at most once per run, not sooner than five
  // seconds after startup (checks requested before that time will occur then;
  // checks requested afterwards will occur immediately, if no other checks have
  // been made during this run).
  //
  // In unittest mode, this function does nothing.
  static void RequestServerCheck();

  static void RegisterPrefs(PrefService* prefs);

 private:
  FRIEND_TEST(GoogleURLTrackerTest, CheckAndConvertURL);

  // Determines if |url| is an appropriate source for a new Google base URL, and
  // update |base_url| to the appropriate base URL if so.  Returns whether the
  // check succeeded (and thus whether |base_url| was actually updated).
  static bool CheckAndConvertToGoogleBaseURL(const GURL& url, GURL* base_url);

  // Registers consumer interest in getting an updated URL from the server.
  void SetNeedToFetch();

  // Called when the five second startup sleep has finished.  Runs any pending
  // fetch.
  void FinishSleep();

  // Starts the fetch of the up-to-date Google URL if we actually want to fetch
  // it and can currently do so.
  void StartFetchIfDesirable();

  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher *source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  static const char kDefaultGoogleHomepage[];

  GURL google_url_;
  ScopedRunnableMethodFactory<GoogleURLTracker> fetcher_factory_;
  scoped_ptr<URLFetcher> fetcher_;
  bool in_startup_sleep_;  // True if we're in the five-second "no fetching"
                           // period that begins at browser start.
  bool already_fetched_;   // True if we've already fetched a URL once this run;
                           // we won't fetch again until after a restart.
  bool need_to_fetch_;     // True if a consumer actually wants us to fetch an
                           // updated URL.  If this is never set, we won't
                           // bother to fetch anything.
  bool request_context_available_;
                           // True when the profile has been loaded and the
                           // default request context created, so we can
                           // actually do the fetch with the right data.

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTracker);
};

#endif  // CHROME_BROWSER_GOOGLE_URL_TRACKER_H_

