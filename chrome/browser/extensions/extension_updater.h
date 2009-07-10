// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_UPDATER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_UPDATER_H_

#include <deque>
#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/scoped_vector.h"
#include "base/task.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/version.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

class Extension;
class ExtensionUpdaterTest;
class MessageLoop;
class ExtensionUpdaterFileHandler;

// A class for doing auto-updates of installed Extensions. Used like this:
//
// ExtensionUpdater* updater = new ExtensionUpdater(my_extensions_service,
//                                                  update_frequency_secs,
//                                                  file_io_loop);
// updater.Start();
// ....
// updater.Stop();
class ExtensionUpdater
    : public URLFetcher::Delegate,
      public base::RefCountedThreadSafe<ExtensionUpdater> {
 public:
  // Holds a pointer to the passed |service|, using it for querying installed
  // extensions and installing updated ones. The |frequency_seconds| parameter
  // controls how often update checks are scheduled.
  ExtensionUpdater(ExtensionUpdateService* service,
                   int frequency_seconds,
                   MessageLoop* file_io_loop);

  virtual ~ExtensionUpdater();

  // Starts the updater running, with the first check scheduled for
  // |frequency_seconds| from now. Eventually ExtensionUpdater will persist the
  // time the last check happened, and do the first check |frequency_seconds_|
  // from then (potentially adding a short wait if the browser just started).
  // (http://crbug.com/12545).
  void Start();

  // Stops the updater running, cancelling any outstanding update manifest and
  // crx downloads. Does not cancel any in-progress installs.
  void Stop();

 private:
  friend class ExtensionUpdaterTest;
  friend class ExtensionUpdaterFileHandler;
  class ParseHelper;

  // An update manifest looks like this:
  //
  // <?xml version='1.0' encoding='UTF-8'?>
  // <gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>
  //  <app appid='12345'>
  //   <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'
  //                version='1.2.3.4' prodversionmin='2.0.143.0' />
  //  </app>
  // </gupdate>
  //
  // The "appid" attribute of the <app> tag refers to the unique id of the
  // extension. The "codebase" attribute of the <updatecheck> tag is the url to
  // fetch the updated crx file, and the "prodversionmin" attribute refers to
  // the minimum version of the chrome browser that the update applies to.

  // The result of parsing one <app> tag in an xml update check manifest.
  struct ParseResult {
    std::string extension_id;
    scoped_ptr<Version> version;
    scoped_ptr<Version> browser_min_version;
    GURL crx_url;
  };

  // We need to keep track of the extension id associated with a url when
  // doing a fetch.
  struct ExtensionFetch {
    std::string id;
    GURL url;
    ExtensionFetch() : id(""), url() {}
    ExtensionFetch(std::string i, GURL u) : id(i), url(u) {}
  };

  // These are needed for unit testing, to help identify the correct mock
  // URLFetcher objects.
  static const int kManifestFetcherId = 1;
  static const int kExtensionFetcherId = 1;

  // Constants for the update manifest.
  static const char* kExpectedGupdateProtocol;
  static const char* kExpectedGupdateXmlns;

  // Does common work from constructors.
  void Init();

  // URLFetcher::Delegate interface.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // These do the actual work when a URL fetch completes.
  virtual void OnManifestFetchComplete(const GURL& url,
                                       const URLRequestStatus& status,
                                       int response_code,
                                       const std::string& data);
  virtual void OnCRXFetchComplete(const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const std::string& data);

  // Called when a crx file has been written into a temp file, and is ready
  // to be installed.
  void OnCRXFileWritten(const std::string& id, const FilePath& path);

  // Callback for when ExtensionsService::Install is finished.
  void OnExtensionInstallFinished(const FilePath& path, Extension* extension);

  // BaseTimer::ReceiverMethod callback.
  void TimerFired();

  // Begins an update check - called with url to fetch an update manifest.
  void StartUpdateCheck(const GURL& url);

  // Begins (or queues up) download of an updated extension.
  void FetchUpdatedExtension(const std::string& id, GURL url);

  typedef std::vector<ParseResult*> ParseResultList;

  // Given a list of potential updates, returns the indices of the ones that are
  // applicable (are actually a new version, etc.) in |result|.
  std::vector<int> DetermineUpdates(const ParseResultList& possible_updates);

  // Parses an update manifest xml string into ParseResult data. On success, it
  // inserts new ParseResult items into |result| and returns true. On failure,
  // it returns false and puts nothing into |result|.
  static bool Parse(const std::string& manifest_xml, ParseResultList* result);

  // Outstanding url fetch requests for manifests and updates.
  scoped_ptr<URLFetcher> manifest_fetcher_;
  scoped_ptr<URLFetcher> extension_fetcher_;

  // Pending manifests and extensions to be fetched when the appropriate fetcher
  // is available.
  std::deque<GURL> manifests_pending_;
  std::deque<ExtensionFetch> extensions_pending_;

  // The extension currently being fetched (if any).
  ExtensionFetch current_extension_fetch_;

  // Pointer back to the service that owns this ExtensionUpdater.
  ExtensionUpdateService* service_;

  base::RepeatingTimer<ExtensionUpdater> timer_;
  int frequency_seconds_;

  // The MessageLoop where we should do file I/O.
  MessageLoop* file_io_loop_;

  scoped_refptr<ExtensionUpdaterFileHandler> file_handler_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUpdater);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_UPDATER_H_
