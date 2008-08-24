// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APP_H__
#define CHROME_BROWSER_WEB_APP_H__

#include <vector>

#include "base/ref_counted.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/gears_integration.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "googleurl/src/gurl.h"
#include "skia/include/SkBitmap.h"

class Profile;
class WebContents;

// A WebApp represents a page that Gears has installed a shortcut for. A WebApp
// has a name, url and set of images (potentially empty). The images are lazily
// loaded when asked for.
//
// The images are first loaded from the WebDatabase. If the images are not in
// the WebDB, the list of images is obtained from Gears then downloaded via the
// WebContents set by SetWebContents. As images are loaded they are pushed to
// the WebDatabase. Observers are notified any time the set of images changes.
class WebApp : public base::RefCounted<WebApp>, public WebDataServiceConsumer {
 public:
  typedef std::vector<SkBitmap> Images;

  // The Observer is notified any time the set of images contained in the WebApp
  // changes.
  class Observer {
   public:
    virtual void WebAppImagesChanged(WebApp* web_app) = 0;
  };

  // Creates a WebApp by name and url. This variant is only used if Gears
  // doesn't know about the shortcut.
  WebApp(Profile* profile,
         const GURL& url,
         const std::wstring& name);

  // Creates a WebApp from a Gears shortcut.
  WebApp(Profile* profile,
         const GearsShortcutData& shortcut);
  ~WebApp();

  // Sets the specified image. This is invoked from the WebContents when an
  // image finishes downloading. If image_url is one of the images this WebApp
  // asked to download, it is pushed to the database and the observer is
  // notified. If the image isn't one that was asked for by this WebApp, nothing
  // happens.
  void SetImage(const GURL& image_url, const SkBitmap& image);

  // Returns the set of images. If the images haven't been loaded yet, they are
  // asked for.
  const Images& GetImages();

  // Convenience to get the favicon from the set of images. If a favicon sized
  // image isn't found, an empty image is returned.
  SkBitmap GetFavIcon();

  // Name of the app.
  const std::wstring& name() const { return name_; }

  // URL to the app.
  const GURL& url() const { return url_; }

  // Sets the WebContents that is using this WebApp. This is used if the
  // database doesn't have all the images. If NULL, images won't be downloaded
  // if they aren't in the db.
  void SetWebContents(WebContents* host);

  // WebContents used to download images; may be null.
  WebContents* web_contents() { return web_contents_; }

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

 private:
  // Requests the images for this app from the web db. Does nothing if the
  // images have already been requested.
  void LoadImagesFromWebData();

  // Notification from the WebDB that our request for the images has completed.
  // This adds all the images from the request to this WebApp, and if not all
  // images have been downloaded, the images are requested from the webContents.
  // Similarly if a favicon sized image isn't available, one is asked for from
  // history.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);

  // Callback from history when the favicon is available. If we don't have a
  // favicon sized image, the image is added to this WebApp's list of images.
  void OnFavIconFromHistory(HistoryService::Handle handle,
                            bool know_favicon,
                            scoped_refptr<RefCountedBytes> data,
                            bool expired,
                            GURL icon_url);

  // Requests the favicon from history.
  void LoadFavIconFromHistory();

  // Asks the hosting WebApp to download all the images.
  void DownloadImagesFromSite();

  // Returns the position of the favicon, or images_.end() if no favicon sized
  // image is available
  Images::iterator GetFavIconIterator();

  // An URLs in image_urls_ that are data encoded PNGs are extracted and added
  // to images_.
  void ExtractPNGEncodedURLs();

  void NotifyObservers();

  // WebContents used to download images, may be null.
  WebContents* web_contents_;

  // Profile used for WebDataservice and History.
  Profile* profile_;

  // URL of the app.
  const GURL url_;

  // Name of the app.
  const std::wstring name_;

  // Have the images been loaded from the WebDB? This is initially false and set
  // true when GetImages is invoked.
  bool loaded_images_from_web_data_;

  // If non-zero, indicates we have a loading pending from the WebDB.
  WebDataService::Handle image_load_handle_;

  // Set of images.
  Images images_;

  // Set of image urls.
  std::set<GURL> image_urls_;

  // Should the images be downloaded from the page? This is false if we don't
  // know the set of image urls (weren't created from a GearsShortcutData) or
  // the image urls in the GearsShortcutData were empty.
  bool download_images_;

  // Used for history request for the favicon.
  CancelableRequestConsumer request_consumer_;

  ObserverList<Observer> observer_list_;

  DISALLOW_EVIL_CONSTRUCTORS(WebApp);
};

#endif  // CHROME_BROWSER_WEB_APP_H__

