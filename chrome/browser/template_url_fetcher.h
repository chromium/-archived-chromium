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

#ifndef CHROME_BROWSER_TEMPLATE_URL_FETCHER_H__
#define CHROME_BROWSER_TEMPLATE_URL_FETCHER_H__

#include "chrome/browser/profile.h"
#include "chrome/browser/url_fetcher.h"
#include "chrome/common/scoped_vector.h"

class GURL;
class Profile;
class TemplateURL;
class WebContents;

// TemplateURLFetcher is responsible for downloading OpenSearch description
// documents, creating a TemplateURL from the OSDD, and adding the TemplateURL
// to the TemplateURLModel. Downloading is done in the background.
//
class TemplateURLFetcher {
 public:
  // Creates a TemplateURLFetcher with the specified Profile.
  explicit TemplateURLFetcher(Profile* profile);

  // If TemplateURLFetcher is not already downloading the OSDD for osdd_url,
  // it is downloaded. If successful and the result can be parsed, a TemplateURL
  // is added to the TemplateURLModel.
  void ScheduleDownload(const std::wstring& keyword,
                        const GURL& osdd_url,
                        const GURL& favicon_url,
                        const HWND parent_window,
                        bool autodetected);

 private:
  friend class RequestDelegate;

  // A RequestDelegate is created to download each OSDD. When done downloading
  // RequestCompleted is invoked back on the TemplateURLFetcher.
  class RequestDelegate : public URLFetcher::Delegate {
   public:
    RequestDelegate(TemplateURLFetcher* fetcher,
                    const std::wstring& keyword,
                    const GURL& osdd_url,
                    const GURL& favicon_url,
                    const HWND parent_window,
                    bool autodetected)
#pragma warning(disable:4355)
        : url_fetcher_(osdd_url, URLFetcher::GET, this),
          fetcher_(fetcher),
          keyword_(keyword),
          osdd_url_(osdd_url),
          favicon_url_(favicon_url),
          parent_window_(parent_window),
          autodetected_(autodetected) {
      url_fetcher_.set_request_context(fetcher->profile()->GetRequestContext());
      url_fetcher_.Start();
    }

    // If data contains a valid OSDD, a TemplateURL is created and added to
    // the TemplateURLModel.
    virtual void OnURLFetchComplete(const URLFetcher* source,
                                    const GURL& url,
                                    const URLRequestStatus& status,
                                    int response_code,
                                    const ResponseCookies& cookies,
                                    const std::string& data);

    // URL of the OSDD.
    const GURL& url() const { return osdd_url_; }

    // Keyword to use.
    const std::wstring keyword() const { return keyword_; }

   private:
    URLFetcher url_fetcher_;
    TemplateURLFetcher* fetcher_;
    const std::wstring keyword_;
    const GURL osdd_url_;
    const GURL favicon_url_;
    bool autodetected_;

    // Used to determine where to place a confirmation dialog. May be NULL,
    // in which case the confirmation will be centered in the screen if needed.
    const HWND parent_window_;

    DISALLOW_EVIL_CONSTRUCTORS(RequestDelegate);
  };

  Profile* profile() const { return profile_; }

  // Invoked from the RequestDelegate when done downloading.
  void RequestCompleted(RequestDelegate* request);

  Profile* profile_;

  // In progress requests.
  ScopedVector<RequestDelegate> requests_;

  DISALLOW_EVIL_CONSTRUCTORS(TemplateURLFetcher);
};

#endif  // CHROME_BROWSER_OSDD_FETCHER_H__
