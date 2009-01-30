// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TEMPLATE_URL_FETCHER_H_
#define CHROME_BROWSER_TEMPLATE_URL_FETCHER_H_

#include "base/gfx/native_widget_types.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/browser/profile.h"
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
                        const gfx::NativeView parent_window,
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
                    gfx::NativeView parent_window,
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
    gfx::NativeView parent_window_;

    DISALLOW_COPY_AND_ASSIGN(RequestDelegate);
  };

  Profile* profile() const { return profile_; }

  // Invoked from the RequestDelegate when done downloading.
  void RequestCompleted(RequestDelegate* request);

  Profile* profile_;

  // In progress requests.
  ScopedVector<RequestDelegate> requests_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLFetcher);
};

#endif  // CHROME_BROWSER_OSDD_FETCHER_H_
