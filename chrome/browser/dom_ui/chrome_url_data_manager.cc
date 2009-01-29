// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/chrome_url_data_manager.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/ref_counted_util.h"
#include "googleurl/src/url_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_job.h"
#ifdef CHROME_PERSONALIZATION
// TODO(timsteele): Remove all CHROME_PERSONALIZATION code in this file.
// It is only temporarily needed to configure some personalization data sources
// that will go away soon.
#include "chrome/personalization/personalization.h"
#endif

// The URL scheme used for internal chrome resources.
// TODO(glen): Choose a better location for this.
static const char kChromeURLScheme[] = "chrome";

// The single global instance of ChromeURLDataManager.
ChromeURLDataManager chrome_url_data_manager;

// URLRequestChromeJob is a URLRequestJob that manages running chrome-internal
// resource requests asynchronously.
// It hands off URL requests to ChromeURLDataManager, which asynchronously
// calls back once the data is available.
class URLRequestChromeJob : public URLRequestJob {
 public:
  explicit URLRequestChromeJob(URLRequest* request);
  virtual ~URLRequestChromeJob();

  // URLRequestJob implementation.
  virtual void Start();
  virtual void Kill();
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read);
  virtual bool GetMimeType(std::string* mime_type);

  // Called by ChromeURLDataManager to notify us that the data blob is ready
  // for us.
  void DataAvailable(RefCountedBytes* bytes);

  void SetMimeType(const std::string& mime_type) {
    mime_type_ = mime_type;
  }

 private:
  // Helper for Start(), to let us start asynchronously.
  // (This pattern is shared by most URLRequestJob implementations.)
  void StartAsync();

  // Do the actual copy from data_ (the data we're serving) into |buf|.
  // Separate from ReadRawData so we can handle async I/O.
  void CompleteRead(net::IOBuffer* buf, int buf_size, int* bytes_read);

  // The actual data we're serving.  NULL until it's been fetched.
  scoped_refptr<RefCountedBytes> data_;
  // The current offset into the data that we're handing off to our
  // callers via the Read interfaces.
  int data_offset_;

  // For async reads, we keep around a pointer to the buffer that
  // we're reading into.
  scoped_refptr<net::IOBuffer> pending_buf_;
  int pending_buf_size_;
  std::string mime_type_;

  DISALLOW_EVIL_CONSTRUCTORS(URLRequestChromeJob);
};

// URLRequestChromeFileJob is a URLRequestJob that acts like a file:// URL
class URLRequestChromeFileJob : public URLRequestFileJob {
 public:
  URLRequestChromeFileJob(URLRequest* request, const FilePath& path);
  virtual ~URLRequestChromeFileJob();

 private:
  DISALLOW_EVIL_CONSTRUCTORS(URLRequestChromeFileJob);
};

void RegisterURLRequestChromeJob() {
  // Being a standard scheme allows us to resolve relative paths. This method
  // is invoked multiple times during testing, so only add the scheme once.
  url_parse::Component url_scheme_component(0, arraysize(kChromeURLScheme) - 1);
  if (!url_util::IsStandard(kChromeURLScheme, arraysize(kChromeURLScheme) - 1,
                            url_scheme_component)) {
    url_util::AddStandardScheme(kChromeURLScheme);
  }

  std::wstring inspector_dir;
  if (PathService::Get(chrome::DIR_INSPECTOR, &inspector_dir))
    chrome_url_data_manager.AddFileSource("inspector", inspector_dir);

  URLRequest::RegisterProtocolFactory(kChromeURLScheme,
                                      &ChromeURLDataManager::Factory);
#ifdef CHROME_PERSONALIZATION
  url_util::AddStandardScheme(kPersonalizationScheme);
  URLRequest::RegisterProtocolFactory(kPersonalizationScheme,
                                      &ChromeURLDataManager::Factory); 
#endif
}

void UnregisterURLRequestChromeJob() {
  std::wstring inspector_dir;
  if (PathService::Get(chrome::DIR_INSPECTOR, &inspector_dir))
    chrome_url_data_manager.RemoveFileSource("inspector");
}

// static
void ChromeURLDataManager::URLToRequest(const GURL& url,
                                        std::string* source_name,
                                        std::string* path) {
#ifdef CHROME_PERSONALIZATION
  DCHECK(url.SchemeIs(kChromeURLScheme) ||
         url.SchemeIs(kPersonalizationScheme));
#else
  DCHECK(url.SchemeIs(kChromeURLScheme));
#endif

  if (!url.is_valid()) {
    NOTREACHED();
    return;
  }

  // Our input looks like: chrome://source_name/extra_bits?foo .
  // So the url's "host" is our source, and everything after the host is
  // the path.
  source_name->assign(url.host());

  const std::string& spec = url.possibly_invalid_spec();
  const url_parse::Parsed& parsed = url.parsed_for_possibly_invalid_spec();
  int offset = parsed.CountCharactersBefore(url_parse::Parsed::PATH, false);
  ++offset;  // Skip the slash at the beginning of the path.
  if (offset < static_cast<int>(spec.size()))
    path->assign(spec.substr(offset));
}

// static
bool ChromeURLDataManager::URLToFilePath(const GURL& url,
                                         std::wstring* file_path) {
  // Parse the URL into a request for a source and path.
  std::string source_name;
  std::string relative_path;
  URLToRequest(url, &source_name, &relative_path);

  FileSourceMap::const_iterator i(
      chrome_url_data_manager.file_sources_.find(source_name));
  if (i == chrome_url_data_manager.file_sources_.end())
    return false;

  file_path->assign(i->second);
  file_util::AppendToPath(file_path, UTF8ToWide(relative_path));
  return true;
}

ChromeURLDataManager::ChromeURLDataManager() : next_request_id_(0) { }

ChromeURLDataManager::~ChromeURLDataManager() { }

void ChromeURLDataManager::AddDataSource(scoped_refptr<DataSource> source) {
  // TODO(jackson): A new data source with same name should not clobber the
  // existing one.
  data_sources_[source->source_name()] = source;
}

void ChromeURLDataManager::AddFileSource(const std::string& source_name,
                                         const std::wstring& file_path) {
  DCHECK(file_sources_.count(source_name) == 0);
  file_sources_[source_name] = file_path;
}

void ChromeURLDataManager::RemoveFileSource(const std::string& source_name) {
  DCHECK(file_sources_.count(source_name) == 1);
  file_sources_.erase(source_name);
}

bool ChromeURLDataManager::StartRequest(const GURL& url,
                                        URLRequestChromeJob* job) {
  // Parse the URL into a request for a source and path.
  std::string source_name;
  std::string path;
  URLToRequest(url, &source_name, &path);

  // Look up the data source for the request.
  DataSourceMap::iterator i = data_sources_.find(source_name);
  if (i == data_sources_.end())
    return false;
  DataSource* source = i->second;

  // Save this request so we know where to send the data.
  RequestID request_id = next_request_id_++;
  pending_requests_.insert(std::make_pair(request_id, job));

  // TODO(eroman): would be nicer if the mimetype were set at the same time
  // as the data blob. For now do it here, since NotifyHeadersComplete() is
  // going to get called once we return.
  job->SetMimeType(source->GetMimeType(path));

  // Forward along the request to the data source.
  source->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(source, &DataSource::StartDataRequest,
                        path, request_id));
  return true;
}

void ChromeURLDataManager::RemoveRequest(URLRequestChromeJob* job) {
  // Remove the request from our list of pending requests.
  // If/when the source sends the data that was requested, the data will just
  // be thrown away.
  for (PendingRequestMap::iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    if (i->second == job) {
      pending_requests_.erase(i);
      return;
    }
  }
}

void ChromeURLDataManager::DataAvailable(
    RequestID request_id,
    scoped_refptr<RefCountedBytes> bytes) {
  // Forward this data on to the pending URLRequest, if it exists.
  PendingRequestMap::iterator i = pending_requests_.find(request_id);
  if (i != pending_requests_.end()) {
    URLRequestChromeJob* job = i->second;
    pending_requests_.erase(i);
    job->DataAvailable(bytes);
  }
}

void ChromeURLDataManager::DataSource::SendResponse(
    RequestID request_id,
    RefCountedBytes* bytes) {
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(&chrome_url_data_manager,
                        &ChromeURLDataManager::DataAvailable,
                        request_id, scoped_refptr<RefCountedBytes>(bytes)));
}

// static
URLRequestJob* ChromeURLDataManager::Factory(URLRequest* request,
                                             const std::string& scheme) {
  // Try first with a file handler
  std::wstring path;
  if (ChromeURLDataManager::URLToFilePath(request->url(), &path))
    return new URLRequestChromeFileJob(request,
                                       FilePath::FromWStringHack(path));

  // Fall back to using a custom handler
  return new URLRequestChromeJob(request);
}

URLRequestChromeJob::URLRequestChromeJob(URLRequest* request)
    : URLRequestJob(request), data_offset_(0) {}

URLRequestChromeJob::~URLRequestChromeJob() {
}

void URLRequestChromeJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestChromeJob::StartAsync));
}

void URLRequestChromeJob::Kill() {
  chrome_url_data_manager.RemoveRequest(this);
}

bool URLRequestChromeJob::GetMimeType(std::string* mime_type) {
  *mime_type = mime_type_;
  return !mime_type_.empty();
}

void URLRequestChromeJob::DataAvailable(RefCountedBytes* bytes) {
  if (bytes) {
    // The request completed, and we have all the data.
    // Clear any IO pending status.
    SetStatus(URLRequestStatus());

    data_ = bytes;
    int bytes_read;
    if (pending_buf_.get()) {
      CompleteRead(pending_buf_, pending_buf_size_, &bytes_read);
      NotifyReadComplete(bytes_read);
      pending_buf_ = NULL;
    }
  } else {
    // The request failed.
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, 0));
  }
}

bool URLRequestChromeJob::ReadRawData(net::IOBuffer* buf, int buf_size,
                                      int* bytes_read) {
  if (!data_.get()) {
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
    DCHECK(!pending_buf_.get());
    pending_buf_ = buf;
    pending_buf_size_ = buf_size;
    return false;  // Tell the caller we're still waiting for data.
  }

  // Otherwise, the data is available.
  CompleteRead(buf, buf_size, bytes_read);
  return true;
}

void URLRequestChromeJob::CompleteRead(net::IOBuffer* buf, int buf_size,
                                       int* bytes_read) {
  int remaining = static_cast<int>(data_->data.size()) - data_offset_;
  if (buf_size > remaining)
    buf_size = remaining;
  if (buf_size > 0) {
    memcpy(buf->data(), &data_->data[0] + data_offset_, buf_size);
    data_offset_ += buf_size;
  }
  *bytes_read = buf_size;
}

void URLRequestChromeJob::StartAsync() {
  if (!request_)
    return;

  if (chrome_url_data_manager.StartRequest(request_->url(), this)) {
    NotifyHeadersComplete();
  } else {
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED,
                                      net::ERR_INVALID_URL));
  }
}

URLRequestChromeFileJob::URLRequestChromeFileJob(URLRequest* request,
                                                 const FilePath& path)
    : URLRequestFileJob(request, path) {
}

URLRequestChromeFileJob::~URLRequestChromeFileJob() { }

