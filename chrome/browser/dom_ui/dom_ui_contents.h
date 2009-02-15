// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains code for managing local HTML UI at chrome-ui:// URLs.

#ifndef CHROME_BROWSER_DOM_UI_CONTENTS_H_
#define CHROME_BROWSER_DOM_UI_CONTENTS_H_

#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "webkit/glue/webpreferences.h"

class DOMUI;
class render_view_host;

// FavIconSource is the gateway between network-level chrome:
// requests for favicons and the history backend that serves these.
class FavIconSource : public ChromeURLDataManager::DataSource {
 public:
  explicit FavIconSource(Profile* profile);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path, int request_id);

  virtual std::string GetMimeType(const std::string&) const {
    // Rely on image decoder inferring the correct type.
    return std::string();
  }

  // Called when favicon data is available from the history backend.
  void OnFavIconDataAvailable(
      HistoryService::Handle request_handle,
      bool know_favicon,
      scoped_refptr<RefCountedBytes> data,
      bool expired,
      GURL url);

 private:
  Profile* const profile_;
  CancelableRequestConsumerT<int, 0> cancelable_consumer_;

  // Raw PNG representation of the favicon to show when the favicon
  // database doesn't have a favicon for a webpage.
  scoped_refptr<RefCountedBytes> default_favicon_;

  DISALLOW_COPY_AND_ASSIGN(FavIconSource);
};

// ThumbnailSource is the gateway between network-level chrome:
// requests for thumbnails and the history backend that serves these.
class ThumbnailSource : public ChromeURLDataManager::DataSource {
 public:
  explicit ThumbnailSource(Profile* profile);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path, int request_id);

  virtual std::string GetMimeType(const std::string&) const {
    // Rely on image decoder inferring the correct type.
    return std::string();
  }

  // Called when thumbnail data is available from the history backend.
  void OnThumbnailDataAvailable(
      HistoryService::Handle request_handle,
      scoped_refptr<RefCountedBytes> data);

 private:
  Profile* const profile_;
  CancelableRequestConsumerT<int, 0> cancelable_consumer_;

  // Raw PNG representation of the thumbnail to show when the thumbnail
  // database doesn't have a thumbnail for a webpage.
  scoped_refptr<RefCountedBytes> default_thumbnail_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailSource);
};

// Exposed for use by BrowserURLHandler.
bool DOMUIContentsCanHandleURL(GURL* url, TabContentsType* result_type);

class DOMUIContents : public WebContents {
 public:
  DOMUIContents(Profile* profile,
                SiteInstance* instance,
                RenderViewHostFactory* render_view_factory);
  ~DOMUIContents();

  //
  // WebContents overrides
  //
  virtual void ProcessDOMUIMessage(const std::string& message,
                                   const std::string& content);
  virtual bool CreateRenderViewForRenderManager(
      RenderViewHost* render_view_host);
  // Override this method so we can ensure that javascript and image loading
  // are always on even for DOMUIHost tabs.
  virtual WebPreferences GetWebkitPrefs();

  //
  // TabContents overrides
  // 
  virtual void UpdateHistoryForNavigation(const GURL& url,
      const ViewHostMsg_FrameNavigate_Params& params) { }
  virtual bool NavigateToPendingEntry(bool reload);

  // Return the scheme used. We currently use chrome:
  static const std::string GetScheme();

 private:
  // Return a DOM UI for the provided URL.
  DOMUI* GetDOMUIForURL(const GURL& url);

  // The DOMUI we own and show.
  DOMUI* current_ui_;

  DISALLOW_COPY_AND_ASSIGN(DOMUIContents);
};

#endif  // CHROME_BROWSER_DOM_UI_CONTENTS_H_
