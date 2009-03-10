// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BROWSER_DOM_UI_CHROME_URL_DATA_MANAGER_H__
#define BROWSER_DOM_UI_CHROME_URL_DATA_MANAGER_H__

#include <map>
#include <string>

#include "base/task.h"
#include "chrome/common/ref_counted_util.h"

class GURL;
class MessageLoop;
class URLRequest;
class URLRequestChromeJob;
class URLRequestJob;

// To serve dynamic data off of chrome: URLs, implement the
// ChromeURLDataManager::DataSource interface and register your handler
// with AddDataSource.

// ChromeURLDataManager lives on the IO thread, so any interfacing with
// it from the UI thread needs to go through an InvokeLater.
class ChromeURLDataManager {
 public:
  ChromeURLDataManager();
  ~ChromeURLDataManager();

  typedef int RequestID;

  // A DataSource is an object that can answer requests for data
  // asynchronously.  It should live on a thread that outlives the IO thread
  // (in particular, the UI thread).
  // An implementation of DataSource should handle calls to StartDataRequest()
  // by starting its (implementation-specific) asynchronous request for
  // the data, then call SendResponse() to notify
  class DataSource : public base::RefCountedThreadSafe<DataSource> {
   public:
    // See source_name_ and message_loop_ below for docs on these parameters.
    DataSource(const std::string& source_name,
               MessageLoop* message_loop)
        : source_name_(source_name), message_loop_(message_loop) {}
    virtual ~DataSource() {}

    // Sent by the DataManager to request data at |path|.  The source should
    // call SendResponse() when the data is available or if the request could
    // not be satisfied.
    virtual void StartDataRequest(const std::string& path, int request_id) = 0;

    // Return the mimetype that should be sent with this response, or empty
    // string to specify no mime type.
    virtual std::string GetMimeType(const std::string& path) const = 0;

    // Report that a request has resulted in the data |bytes|.
    // If the request can't be satisfied, pass NULL for |bytes| to indicate
    // the request is over.
    void SendResponse(int request_id, RefCountedBytes* bytes);

    MessageLoop* message_loop() const { return message_loop_; }
    const std::string& source_name() const { return source_name_; }

   private:
    // The name of this source.
    // E.g., for favicons, this could be "favicon", which results in paths for
    // specific resources like "favicon/34" getting sent to this source.
    const std::string source_name_;

    // The MessageLoop for the thread where this DataSource lives.
    // Used to send messages to the DataSource.
    MessageLoop* message_loop_;
  };

  // Add a DataSource to the collection of data sources.
  // Because we don't track users of a given path, we can't know when it's
  // safe to remove them, so the added source effectively leaks.
  // This could be improved in the future but currently the users of this
  // interface are conceptually permanent registration anyway.
  // Adding a second DataSource with the same name clobbers the first.
  // NOTE: Calling this from threads other the IO thread must be done via
  // InvokeLater.
  void AddDataSource(scoped_refptr<DataSource> source);

  // Add/remove a path from the collection of file sources.
  // A file source acts like a file:// URL to the specified path.
  // Calling this from threads other the IO thread must be done via
  // InvokeLater.
  void AddFileSource(const std::string& source_name, const std::wstring& path);
  void RemoveFileSource(const std::string& source_name);

  static URLRequestJob* Factory(URLRequest* request, const std::string& scheme);

private:
  friend class URLRequestChromeJob;

  // Parse a URL into the components used to resolve its request.
  static void URLToRequest(const GURL& url,
                           std::string* source,
                           std::string* path);

  // Translate a chrome resource URL into a local file path if there is one.
  // Returns false if there is no file handler for this URL
  static bool URLToFilePath(const GURL& url, std::wstring* file_path);

  // Called by the job when it's starting up.
  // Returns false if |url| is not a URL managed by this object.
  bool StartRequest(const GURL& url, URLRequestChromeJob* job);
  // Remove a request from the list of pending requests.
  void RemoveRequest(URLRequestChromeJob* job);

  // Sent by Request::SendResponse.
  void DataAvailable(RequestID request_id,
                     scoped_refptr<RefCountedBytes> bytes);

  // File sources of data, keyed by source name (e.g. "inspector").
  typedef std::map<std::string, std::wstring> FileSourceMap;
  FileSourceMap file_sources_;

  // Custom sources of data, keyed by source path (e.g. "favicon").
  typedef std::map<std::string, scoped_refptr<DataSource> > DataSourceMap;
  DataSourceMap data_sources_;

  // All pending URLRequestChromeJobs, keyed by ID of the request.
  // URLRequestChromeJob calls into this object when it's constructed and
  // destructed to ensure that the pointers in this map remain valid.
  typedef std::map<RequestID, URLRequestChromeJob*> PendingRequestMap;
  PendingRequestMap pending_requests_;

  // The ID we'll use for the next request we receive.
  RequestID next_request_id_;
};

// Since we have a single global ChromeURLDataManager, we don't need to
// grab a reference to it when creating Tasks involving it.
template <> struct RunnableMethodTraits<ChromeURLDataManager> {
  static void RetainCallee(ChromeURLDataManager*) {}
  static void ReleaseCallee(ChromeURLDataManager*) {}
};

// The single global instance of ChromeURLDataManager.
extern ChromeURLDataManager chrome_url_data_manager;

// Register our special URL handler under our special URL scheme.
// Must be done once at startup.
void RegisterURLRequestChromeJob();

// Undoes the registration done by RegisterURLRequestChromeJob.
void UnregisterURLRequestChromeJob();

#endif  // BROWSER_DOM_UI_CHROME_URL_DATA_MANAGER_H__
