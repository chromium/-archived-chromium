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
