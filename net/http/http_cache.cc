// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cache.h"

#include <algorithm>

#include "base/compiler_specific.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

#include "base/message_loop.h"
#include "base/pickle.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/http/http_util.h"

using base::Time;

namespace net {

// disk cache entry data indices.
enum {
  kResponseInfoIndex,
  kResponseContentIndex
};

// These values can be bit-wise combined to form the flags field of the
// serialized HttpResponseInfo.
enum {
  // The version of the response info used when persisting response info.
  RESPONSE_INFO_VERSION = 1,

  // We reserve up to 8 bits for the version number.
  RESPONSE_INFO_VERSION_MASK = 0xFF,

  // This bit is set if the response info has a cert at the end.
  RESPONSE_INFO_HAS_CERT = 1 << 8,

  // This bit is set if the response info has a security-bits field (security
  // strength, in bits, of the SSL connection) at the end.
  RESPONSE_INFO_HAS_SECURITY_BITS = 1 << 9,

  // This bit is set if the response info has a cert status at the end.
  RESPONSE_INFO_HAS_CERT_STATUS = 1 << 10,

  // This bit is set if the response info has vary header data.
  RESPONSE_INFO_HAS_VARY_DATA = 1 << 11,

  // TODO(darin): Add other bits to indicate alternate request methods and
  // whether or not we are storing a partial document.  For now, we don't
  // support storing those.
};

//-----------------------------------------------------------------------------

struct HeaderNameAndValue {
  const char* name;
  const char* value;
};

// If the request includes one of these request headers, then avoid caching
// to avoid getting confused.
static const HeaderNameAndValue kPassThroughHeaders[] = {
  { "range", NULL },                // causes unexpected 206s
  { "if-modified-since", NULL },    // causes unexpected 304s
  { "if-none-match", NULL },        // causes unexpected 304s
  { "if-unmodified-since", NULL },  // causes unexpected 412s
  { "if-match", NULL },             // causes unexpected 412s
  { NULL, NULL }
};

// If the request includes one of these request headers, then avoid reusing
// our cached copy if any.
static const HeaderNameAndValue kForceFetchHeaders[] = {
  { "cache-control", "no-cache" },
  { "pragma", "no-cache" },
  { NULL, NULL }
};

// If the request includes one of these request headers, then force our
// cached copy (if any) to be revalidated before reusing it.
static const HeaderNameAndValue kForceValidateHeaders[] = {
  { "cache-control", "max-age=0" },
  { NULL, NULL }
};

static bool HeaderMatches(const HttpUtil::HeadersIterator& h,
                          const HeaderNameAndValue* search) {
  for (; search->name; ++search) {
    if (!LowerCaseEqualsASCII(h.name_begin(), h.name_end(), search->name))
      continue;

    if (!search->value)
      return true;

    HttpUtil::ValuesIterator v(h.values_begin(), h.values_end(), ',');
    while (v.GetNext()) {
      if (LowerCaseEqualsASCII(v.value_begin(), v.value_end(), search->value))
        return true;
    }
  }
  return false;
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

HttpCache::ActiveEntry::ActiveEntry(disk_cache::Entry* e)
    : disk_entry(e),
      writer(NULL),
      will_process_pending_queue(false),
      doomed(false) {
}

HttpCache::ActiveEntry::~ActiveEntry() {
  if (disk_entry)
    disk_entry->Close();
}

//-----------------------------------------------------------------------------

class HttpCache::Transaction
    : public HttpTransaction, public RevocableStore::Revocable {
 public:
  explicit Transaction(HttpCache* cache)
      : RevocableStore::Revocable(&cache->transactions_),
        request_(NULL),
        cache_(cache),
        entry_(NULL),
        network_trans_(NULL),
        callback_(NULL),
        mode_(NONE),
        read_offset_(0),
        effective_load_flags_(0),
        final_upload_progress_(0),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            network_info_callback_(this, &Transaction::OnNetworkInfoAvailable)),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            network_read_callback_(this, &Transaction::OnNetworkReadCompleted)),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            cache_read_callback_(new CancelableCompletionCallback<Transaction>(
                this, &Transaction::OnCacheReadCompleted))) {
  }

  // Clean up the transaction.
  virtual ~Transaction();

  // HttpTransaction methods:
  virtual int Start(const HttpRequestInfo*, CompletionCallback*);
  virtual int RestartIgnoringLastError(CompletionCallback*);
  virtual int RestartWithAuth(const std::wstring& username,
                              const std::wstring& password,
                              CompletionCallback* callback);
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback*);
  virtual const HttpResponseInfo* GetResponseInfo() const;
  virtual LoadState GetLoadState() const;
  virtual uint64 GetUploadProgress(void) const;

  // The transaction has the following modes, which apply to how it may access
  // its cache entry.
  //
  //  o If the mode of the transaction is NONE, then it is in "pass through"
  //    mode and all methods just forward to the inner network transaction.
  //
  //  o If the mode of the transaction is only READ, then it may only read from
  //    the cache entry.
  //
  //  o If the mode of the transaction is only WRITE, then it may only write to
  //    the cache entry.
  //
  //  o If the mode of the transaction is READ_WRITE, then the transaction may
  //    optionally modify the cache entry (e.g., possibly corresponding to
  //    cache validation).
  //
  enum Mode {
    NONE       = 0x0,
    READ       = 0x1,
    WRITE      = 0x2,
    READ_WRITE = READ | WRITE
  };

  Mode mode() const { return mode_; }

  const std::string& key() const { return cache_key_; }

  // Associates this transaction with a cache entry.
  int AddToEntry();

  // Called by the HttpCache when the given disk cache entry becomes accessible
  // to the transaction.  Returns network error code.
  int EntryAvailable(ActiveEntry* entry);

 private:
  // This is a helper function used to trigger a completion callback.  It may
  // only be called if callback_ is non-null.
  void DoCallback(int rv);

  // This will trigger the completion callback if appropriate.
  int HandleResult(int rv);

  // Set request_ and fields derived from it.
  void SetRequest(const HttpRequestInfo* request);

  // Returns true if the request should be handled exclusively by the network
  // layer (skipping the cache entirely).
  bool ShouldPassThrough();

  // Returns true if we should force an end-to-end fetch.
  bool ShouldBypassCache();

  // Called to begin reading from the cache.  Returns network error code.
  int BeginCacheRead();

  // Called to begin validating the cache entry.  Returns network error code.
  int BeginCacheValidation();

  // Called to begin a network transaction.  Returns network error code.
  int BeginNetworkRequest();

  // Called to restart a network transaction after an error.  Returns network
  // error code.
  int RestartNetworkRequest();

  // Called to restart a network transaction with authentication credentials.
  // Returns network error code.
  int RestartNetworkRequestWithAuth(const std::wstring& username,
                                    const std::wstring& password);

  // Called to determine if we need to validate the cache entry before using it.
  bool RequiresValidation();

  // Called to make the request conditional (to ask the server if the cached
  // copy is valid).  Returns true if able to make the request conditional.
  bool ConditionalizeRequest();

  // Called to populate response_ from the cache entry.
  int ReadResponseInfoFromEntry();

  // Called to write data to the cache entry.  If the write fails, then the
  // cache entry is destroyed.  Future calls to this function will just do
  // nothing without side-effect.
  void WriteToEntry(int index, int offset, IOBuffer* data, int data_len);

  // Called to write response_ to the cache entry.
  void WriteResponseInfoToEntry();

  // Called to truncate response content in the entry.
  void TruncateResponseData();

  // Called to append response data to the cache entry.
  void AppendResponseDataToEntry(IOBuffer* data, int data_len);

  // Called when we are done writing to the cache entry.
  void DoneWritingToEntry(bool success);

  // Called to signal completion of the network transaction's Start method:
  void OnNetworkInfoAvailable(int result);

  // Called to signal completion of the network transaction's Read method:
  void OnNetworkReadCompleted(int result);

  // Called to signal completion of the cache's ReadData method:
  void OnCacheReadCompleted(int result);

  const HttpRequestInfo* request_;
  scoped_ptr<HttpRequestInfo> custom_request_;
  HttpCache* cache_;
  HttpCache::ActiveEntry* entry_;
  scoped_ptr<HttpTransaction> network_trans_;
  CompletionCallback* callback_;  // consumer's callback
  HttpResponseInfo response_;
  HttpResponseInfo auth_response_;
  std::string cache_key_;
  Mode mode_;
  scoped_refptr<IOBuffer> read_buf_;
  int read_offset_;
  int effective_load_flags_;
  uint64 final_upload_progress_;
  CompletionCallbackImpl<Transaction> network_info_callback_;
  CompletionCallbackImpl<Transaction> network_read_callback_;
  scoped_refptr<CancelableCompletionCallback<Transaction> >
      cache_read_callback_;
};

HttpCache::Transaction::~Transaction() {
  if (!revoked()) {
    if (entry_) {
      cache_->DoneWithEntry(entry_, this);
    } else {
      cache_->RemovePendingTransaction(this);
    }
  }

  // If there is an outstanding callback, mark it as cancelled so running it
  // does nothing.
  cache_read_callback_->Cancel();

  // We could still have a cache read in progress, so we just null the cache_
  // pointer to signal that we are dead.  See OnCacheReadCompleted.
  cache_ = NULL;
}

int HttpCache::Transaction::Start(const HttpRequestInfo* request,
                                  CompletionCallback* callback) {
  DCHECK(request);
  DCHECK(callback);

  // ensure that we only have one asynchronous call at a time.
  DCHECK(!callback_);

  if (revoked())
    return ERR_UNEXPECTED;

  SetRequest(request);

  int rv;

  if (ShouldPassThrough()) {
    // if must use cache, then we must fail.  this can happen for back/forward
    // navigations to a page generated via a form post.
    if (effective_load_flags_ & LOAD_ONLY_FROM_CACHE)
      return ERR_CACHE_MISS;

    rv = BeginNetworkRequest();
  } else {
    cache_key_ = cache_->GenerateCacheKey(request);

    // requested cache access mode
    if (effective_load_flags_ & LOAD_ONLY_FROM_CACHE) {
      mode_ = READ;
    } else if (effective_load_flags_ & LOAD_BYPASS_CACHE) {
      mode_ = WRITE;
    } else {
      mode_ = READ_WRITE;
    }

    rv = AddToEntry();
  }

  // setting this here allows us to check for the existance of a callback_ to
  // determine if we are still inside Start.
  if (rv == ERR_IO_PENDING)
    callback_ = callback;

  return rv;
}

int HttpCache::Transaction::RestartIgnoringLastError(
    CompletionCallback* callback) {
  DCHECK(callback);

  // ensure that we only have one asynchronous call at a time.
  DCHECK(!callback_);

  if (revoked())
    return ERR_UNEXPECTED;

  int rv = RestartNetworkRequest();

  if (rv == ERR_IO_PENDING)
    callback_ = callback;

  return rv;
}

int HttpCache::Transaction::RestartWithAuth(
    const std::wstring& username,
    const std::wstring& password,
    CompletionCallback* callback) {
  DCHECK(auth_response_.headers);
  DCHECK(callback);

  // Ensure that we only have one asynchronous call at a time.
  DCHECK(!callback_);

  if (revoked())
    return ERR_UNEXPECTED;

  // Clear the intermediate response since we are going to start over.
  auth_response_ = HttpResponseInfo();

  int rv = RestartNetworkRequestWithAuth(username, password);

  if (rv == ERR_IO_PENDING)
    callback_ = callback;

  return rv;
}

int HttpCache::Transaction::Read(IOBuffer* buf, int buf_len,
                                 CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK(buf_len > 0);
  DCHECK(callback);

  DCHECK(!callback_);

  if (revoked())
    return ERR_UNEXPECTED;

  // If we have an intermediate auth response at this point, then it means the
  // user wishes to read the network response (the error page).  If there is a
  // previous response in the cache then we should leave it intact.
  if (auth_response_.headers && mode_ != NONE) {
    DCHECK(mode_ & WRITE);
    DoneWritingToEntry(mode_ == READ_WRITE);
    mode_ = NONE;
  }

  int rv;

  switch (mode_) {
    case NONE:
    case WRITE:
      DCHECK(network_trans_.get());
      rv = network_trans_->Read(buf, buf_len, &network_read_callback_);
      read_buf_ = buf;
      if (rv >= 0)
        OnNetworkReadCompleted(rv);
      break;
    case READ:
      DCHECK(entry_);
      cache_read_callback_->AddRef();  // Balanced in OnCacheReadCompleted.
      rv = entry_->disk_entry->ReadData(kResponseContentIndex, read_offset_,
                                        buf, buf_len, cache_read_callback_);
      read_buf_ = buf;
      if (rv >= 0) {
        OnCacheReadCompleted(rv);
      } else if (rv != ERR_IO_PENDING) {
        cache_read_callback_->Release();
      }
      break;
    default:
      NOTREACHED();
      rv = ERR_FAILED;
  }

  if (rv == ERR_IO_PENDING)
    callback_ = callback;
  return rv;
}

const HttpResponseInfo* HttpCache::Transaction::GetResponseInfo() const {
  // Null headers means we encountered an error or haven't a response yet
  if (auth_response_.headers)
    return &auth_response_;
  return (response_.headers || response_.ssl_info.cert) ? &response_ : NULL;
}

LoadState HttpCache::Transaction::GetLoadState() const {
  if (network_trans_.get())
    return network_trans_->GetLoadState();
  if (entry_ || !request_)
    return LOAD_STATE_IDLE;
  return LOAD_STATE_WAITING_FOR_CACHE;
}

uint64 HttpCache::Transaction::GetUploadProgress() const {
  if (network_trans_.get())
    return network_trans_->GetUploadProgress();
  return final_upload_progress_;
}

int HttpCache::Transaction::AddToEntry() {
  ActiveEntry* entry = NULL;

  if (revoked())
    return ERR_UNEXPECTED;

  if (mode_ == WRITE) {
    cache_->DoomEntry(cache_key_);
  } else {
    entry = cache_->FindActiveEntry(cache_key_);
    if (!entry) {
      entry = cache_->OpenEntry(cache_key_);
      if (!entry) {
        if (mode_ & WRITE) {
          mode_ = WRITE;
        } else {
          if (cache_->mode() == PLAYBACK)
            DLOG(INFO) << "Playback Cache Miss: " << request_->url;

          // entry does not exist, and not permitted to create a new entry, so
          // we must fail.
          return HandleResult(ERR_CACHE_MISS);
        }
      }
    }
  }

  if (mode_ == WRITE) {
    DCHECK(!entry);
    entry = cache_->CreateEntry(cache_key_);
    if (!entry) {
      DLOG(WARNING) << "unable to create cache entry";
      mode_ = NONE;
      return BeginNetworkRequest();
    }
  }

  return cache_->AddTransactionToEntry(entry, this);
}

int HttpCache::Transaction::EntryAvailable(ActiveEntry* entry) {
  // We now have access to the cache entry.
  //
  //  o if we are the writer for the transaction, then we can start the network
  //    transaction.
  //
  //  o if we are a reader for the transaction, then we can start reading the
  //    cache entry.
  //
  //  o if we can read or write, then we should check if the cache entry needs
  //    to be validated and then issue a network request if needed or just read
  //    from the cache if the cache entry is already valid.
  //
  int rv;
  entry_ = entry;
  switch (mode_) {
    case READ:
      rv = BeginCacheRead();
      break;
    case WRITE:
      rv = BeginNetworkRequest();
      break;
    case READ_WRITE:
      rv = BeginCacheValidation();
      break;
    default:
      NOTREACHED();
      rv = ERR_FAILED;
  }
  return rv;
}

void HttpCache::Transaction::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(callback_);

  // since Run may result in Read being called, clear callback_ up front.
  CompletionCallback* c = callback_;
  callback_ = NULL;
  c->Run(rv);
}

int HttpCache::Transaction::HandleResult(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  if (callback_)
    DoCallback(rv);
  return rv;
}

void HttpCache::Transaction::SetRequest(const HttpRequestInfo* request) {
  request_ = request;
  effective_load_flags_ = request_->load_flags;

  // When in playback mode, we want to load exclusively from the cache.
  if (cache_->mode() == PLAYBACK)
    effective_load_flags_ |= LOAD_ONLY_FROM_CACHE;

  // When in record mode, we want to NEVER load from the cache.
  // The reason for this is beacuse we save the Set-Cookie headers
  // (intentionally).  If we read from the cache, we replay them
  // prematurely.
  if (cache_->mode() == RECORD)
    effective_load_flags_ |= LOAD_BYPASS_CACHE;

  // If HttpCache has type MEDIA make sure LOAD_ENABLE_DOWNLOAD_FILE is set,
  // otherwise make sure LOAD_ENABLE_DOWNLOAD_FILE is not set when HttpCache
  // has type other than MEDIA.
  if (cache_->type() == HttpCache::MEDIA) {
    DCHECK(effective_load_flags_ & LOAD_ENABLE_DOWNLOAD_FILE);
  } else {
    DCHECK(!(effective_load_flags_ & LOAD_ENABLE_DOWNLOAD_FILE));
  }

  // Some headers imply load flags.  The order here is significant.
  //
  //   LOAD_DISABLE_CACHE   : no cache read or write
  //   LOAD_BYPASS_CACHE    : no cache read
  //   LOAD_VALIDATE_CACHE  : no cache read unless validation
  //
  // The former modes trump latter modes, so if we find a matching header we
  // can stop iterating kSpecialHeaders.
  //
  static const struct {
    const HeaderNameAndValue* search;
    int load_flag;
  } kSpecialHeaders[] = {
    { kPassThroughHeaders, LOAD_DISABLE_CACHE },
    { kForceFetchHeaders, LOAD_BYPASS_CACHE },
    { kForceValidateHeaders, LOAD_VALIDATE_CACHE },
  };

  // scan request headers to see if we have any that would impact our load flags
  HttpUtil::HeadersIterator it(request_->extra_headers.begin(),
                               request_->extra_headers.end(),
                               "\r\n");
  while (it.GetNext()) {
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kSpecialHeaders); ++i) {
      if (HeaderMatches(it, kSpecialHeaders[i].search)) {
        effective_load_flags_ |= kSpecialHeaders[i].load_flag;
        break;
      }
    }
  }
}

bool HttpCache::Transaction::ShouldPassThrough() {
  // We may have a null disk_cache if there is an error we cannot recover from,
  // like not enough disk space, or sharing violations.
  if (!cache_->disk_cache())
    return true;

  // When using the record/playback modes, we always use the cache
  // and we never pass through.
  if (cache_->mode() == RECORD || cache_->mode() == PLAYBACK)
    return false;

  if (effective_load_flags_ & LOAD_DISABLE_CACHE)
    return true;

  if (request_->method == "GET")
    return false;

  if (request_->method == "POST" &&
      request_->upload_data && request_->upload_data->identifier())
    return false;

  // TODO(darin): add support for caching HEAD responses
  return true;
}

int HttpCache::Transaction::BeginCacheRead() {
  DCHECK(mode_ == READ);

  // read response headers
  return HandleResult(ReadResponseInfoFromEntry());
}

int HttpCache::Transaction::BeginCacheValidation() {
  DCHECK(mode_ == READ_WRITE);

  int rv = ReadResponseInfoFromEntry();
  if (rv != OK) {
    DCHECK(rv != ERR_IO_PENDING);
  } else if (effective_load_flags_ & LOAD_PREFERRING_CACHE ||
             !RequiresValidation()) {
    cache_->ConvertWriterToReader(entry_);
    mode_ = READ;
  } else {
    // Make the network request conditional, to see if we may reuse our cached
    // response.  If we cannot do so, then we just resort to a normal fetch.
    // Our mode remains READ_WRITE for a conditional request.  We'll switch to
    // either READ or WRITE mode once we hear back from the server.
    if (!ConditionalizeRequest())
      mode_ = WRITE;
    return BeginNetworkRequest();
  }
  return HandleResult(rv);
}

int HttpCache::Transaction::BeginNetworkRequest() {
  DCHECK(mode_ & WRITE || mode_ == NONE);
  DCHECK(!network_trans_.get());

  network_trans_.reset(cache_->network_layer_->CreateTransaction());
  if (!network_trans_.get())
    return net::ERR_FAILED;

  int rv = network_trans_->Start(request_, &network_info_callback_);
  if (rv != ERR_IO_PENDING)
    OnNetworkInfoAvailable(rv);
  return rv;
}

int HttpCache::Transaction::RestartNetworkRequest() {
  DCHECK(mode_ & WRITE || mode_ == NONE);
  DCHECK(network_trans_.get());

  int rv = network_trans_->RestartIgnoringLastError(&network_info_callback_);
  if (rv != ERR_IO_PENDING)
    OnNetworkInfoAvailable(rv);
  return rv;
}

int HttpCache::Transaction::RestartNetworkRequestWithAuth(
    const std::wstring& username,
    const std::wstring& password) {
  DCHECK(mode_ & WRITE || mode_ == NONE);
  DCHECK(network_trans_.get());

  int rv = network_trans_->RestartWithAuth(username, password,
                                           &network_info_callback_);
  if (rv != ERR_IO_PENDING)
    OnNetworkInfoAvailable(rv);
  return rv;
}

bool HttpCache::Transaction::RequiresValidation() {
  // TODO(darin): need to do more work here:
  //  - make sure we have a matching request method
  //  - watch out for cached responses that depend on authentication
  // In playback mode, nothing requires validation.
  if (cache_->mode() == net::HttpCache::PLAYBACK)
    return false;

  if (effective_load_flags_ & LOAD_VALIDATE_CACHE)
    return true;

  if (response_.headers->RequiresValidation(
          response_.request_time, response_.response_time, Time::Now()))
    return true;

  // Since Vary header computation is fairly expensive, we save it for last.
  if (response_.vary_data.is_valid() &&
          !response_.vary_data.MatchesRequest(*request_, *response_.headers))
    return true;

  return false;
}

bool HttpCache::Transaction::ConditionalizeRequest() {
  DCHECK(response_.headers);

  // This only makes sense for cached 200 responses.
  if (response_.headers->response_code() != 200)
    return false;

  // Just use the first available ETag and/or Last-Modified header value.
  // TODO(darin): Or should we use the last?

  std::string etag_value;
  response_.headers->EnumerateHeader(NULL, "etag", &etag_value);

  std::string last_modified_value;
  response_.headers->EnumerateHeader(NULL, "last-modified",
                                     &last_modified_value);

  if (etag_value.empty() && last_modified_value.empty())
    return false;

  // Need to customize the request, so this forces us to allocate :(
  custom_request_.reset(new HttpRequestInfo(*request_));
  request_ = custom_request_.get();

  if (!etag_value.empty()) {
    custom_request_->extra_headers.append("If-None-Match: ");
    custom_request_->extra_headers.append(etag_value);
    custom_request_->extra_headers.append("\r\n");
  }

  if (!last_modified_value.empty()) {
    custom_request_->extra_headers.append("If-Modified-Since: ");
    custom_request_->extra_headers.append(last_modified_value);
    custom_request_->extra_headers.append("\r\n");
  }

  return true;
}

int HttpCache::Transaction::ReadResponseInfoFromEntry() {
  DCHECK(entry_);

  if (!HttpCache::ReadResponseInfo(entry_->disk_entry, &response_))
    return ERR_FAILED;

  // If the cache object is used for media file, we want the file handle of
  // response data.
  if (cache_->type() == HttpCache::MEDIA) {
    response_.response_data_file =
        entry_->disk_entry->GetPlatformFile(kResponseContentIndex);
  }

  return OK;
}

void HttpCache::Transaction::WriteToEntry(int index, int offset,
                                          IOBuffer* data, int data_len) {
  if (!entry_)
    return;

  int rv = entry_->disk_entry->WriteData(index, offset, data, data_len, NULL,
                                         true);
  if (rv != data_len) {
    DLOG(ERROR) << "failed to write response data to cache";
    DoneWritingToEntry(false);
  }
}

void HttpCache::Transaction::WriteResponseInfoToEntry() {
  if (!entry_)
    return;

  // Do not cache no-store content (unless we are record mode).  Do not cache
  // content with cert errors either.  This is to prevent not reporting net
  // errors when loading a resource from the cache.  When we load a page over
  // HTTPS with a cert error we show an SSL blocking page.  If the user clicks
  // proceed we reload the resource ignoring the errors.  The loaded resource
  // is then cached.  If that resource is subsequently loaded from the cache,
  // no net error is reported (even though the cert status contains the actual
  // errors) and no SSL blocking page is shown.  An alternative would be to
  // reverse-map the cert status to a net error and replay the net error.
  if ((cache_->mode() != RECORD &&
      response_.headers->HasHeaderValue("cache-control", "no-store")) ||
      net::IsCertStatusError(response_.ssl_info.cert_status)) {
    DoneWritingToEntry(false);
    return;
  }

  // When writing headers, we normally only write the non-transient
  // headers; when in record mode, record everything.
  bool skip_transient_headers = (cache_->mode() != RECORD);

  if (!HttpCache::WriteResponseInfo(entry_->disk_entry, &response_,
                                    skip_transient_headers)) {
    DLOG(ERROR) << "failed to write response info to cache";
    DoneWritingToEntry(false);
  }
}

void HttpCache::Transaction::AppendResponseDataToEntry(IOBuffer* data,
                                                       int data_len) {
  if (!entry_)
    return;

  int current_size = entry_->disk_entry->GetDataSize(kResponseContentIndex);
  WriteToEntry(kResponseContentIndex, current_size, data, data_len);
}

void HttpCache::Transaction::TruncateResponseData() {
  if (!entry_)
    return;

  // If the cache is for media files, we try to prepare the response data
  // file as an external file and truncate it afterwards.
  // Recipient of ResponseInfo should judge from |response_.response_data_file|
  // to tell whether an external file of response data is available for reading
  // or not.
  // TODO(hclam): we should prepare the target stream as extern file only
  // if we get a valid response from server, i.e. 200. We don't want empty
  // cache files for redirection or external files for erroneous requests.
  response_.response_data_file = base::kInvalidPlatformFileValue;
  if (cache_->type() == HttpCache::MEDIA) {
    response_.response_data_file =
        entry_->disk_entry->UseExternalFile(kResponseContentIndex);
  }

  // Truncate the stream.
  WriteToEntry(kResponseContentIndex, 0, NULL, 0);
}

void HttpCache::Transaction::DoneWritingToEntry(bool success) {
  if (!entry_)
    return;

  if (cache_->mode() == RECORD)
    DLOG(INFO) << "Recorded: " << request_->method << request_->url
               << " status: " << response_.headers->response_code();

  cache_->DoneWritingToEntry(entry_, success);
  entry_ = NULL;
  mode_ = NONE;  // switch to 'pass through' mode
}

void HttpCache::Transaction::OnNetworkInfoAvailable(int result) {
  DCHECK(result != ERR_IO_PENDING);

  if (revoked()) {
    HandleResult(ERR_UNEXPECTED);
    return;
  }

  if (result == OK) {
    const HttpResponseInfo* new_response = network_trans_->GetResponseInfo();
    if (new_response->headers->response_code() == 401 ||
        new_response->headers->response_code() == 407) {
      auth_response_ = *new_response;
    } else {
      // Are we expecting a response to a conditional query?
      if (mode_ == READ_WRITE) {
        if (new_response->headers->response_code() == 304) {
          // Update cached response based on headers in new_response
          // TODO(wtc): should we update cached certificate
          // (response_.ssl_info), too?
          response_.headers->Update(*new_response->headers);
          if (response_.headers->HasHeaderValue("cache-control", "no-store")) {
            cache_->DoomEntry(cache_key_);
          } else {
            WriteResponseInfoToEntry();
          }

          if (entry_) {
            cache_->ConvertWriterToReader(entry_);
            // We no longer need the network transaction, so destroy it.
            final_upload_progress_ = network_trans_->GetUploadProgress();
            network_trans_.reset();
            mode_ = READ;
          }
        } else {
          mode_ = WRITE;
        }
      }

      if (!(mode_ & READ)) {
        response_ = *new_response;
        WriteResponseInfoToEntry();

        // Truncate response data
        TruncateResponseData();

        // If this response is a redirect, then we can stop writing now.  (We
        // don't need to cache the response body of a redirect.)
        if (response_.headers->IsRedirect(NULL))
          DoneWritingToEntry(true);
      }
    }
  } else if (IsCertificateError(result)) {
    response_.ssl_info = network_trans_->GetResponseInfo()->ssl_info;
  }
  HandleResult(result);
}

void HttpCache::Transaction::OnNetworkReadCompleted(int result) {
  DCHECK(mode_ & WRITE || mode_ == NONE);

  if (revoked()) {
    HandleResult(ERR_UNEXPECTED);
    return;
  }

  if (result > 0) {
    AppendResponseDataToEntry(read_buf_, result);
  } else if (result == 0) {  // end of file
    DoneWritingToEntry(true);
  }
  HandleResult(result);
}

void HttpCache::Transaction::OnCacheReadCompleted(int result) {
  DCHECK(cache_);
  cache_read_callback_->Release();  // Balance the AddRef() from Start().

  if (revoked()) {
    HandleResult(ERR_UNEXPECTED);
    return;
  }

  if (result > 0) {
    read_offset_ += result;
  } else if (result == 0) {  // end of file
    cache_->DoneReadingFromEntry(entry_, this);
    entry_ = NULL;
  }
  HandleResult(result);
}

//-----------------------------------------------------------------------------

HttpCache::HttpCache(ProxyService* proxy_service,
                     const std::wstring& cache_dir,
                     int cache_size)
    : disk_cache_dir_(cache_dir),
      mode_(NORMAL),
      type_(COMMON),
      network_layer_(HttpNetworkLayer::CreateFactory(proxy_service)),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
      in_memory_cache_(false),
      cache_size_(cache_size) {
}

HttpCache::HttpCache(HttpNetworkSession* session,
                     const std::wstring& cache_dir,
                     int cache_size)
    : disk_cache_dir_(cache_dir),
      mode_(NORMAL),
      type_(COMMON),
      network_layer_(HttpNetworkLayer::CreateFactory(session)),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
      in_memory_cache_(false),
      cache_size_(cache_size) {
}

HttpCache::HttpCache(ProxyService* proxy_service, int cache_size)
    : mode_(NORMAL),
      type_(COMMON),
      network_layer_(HttpNetworkLayer::CreateFactory(proxy_service)),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
      in_memory_cache_(true),
      cache_size_(cache_size) {
}

HttpCache::HttpCache(HttpTransactionFactory* network_layer,
                     disk_cache::Backend* disk_cache)
    : mode_(NORMAL),
      type_(COMMON),
      network_layer_(network_layer),
      disk_cache_(disk_cache),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
      in_memory_cache_(false),
      cache_size_(0) {
}

HttpCache::~HttpCache() {
  // If we have any active entries remaining, then we need to deactivate them.
  // We may have some pending calls to OnProcessPendingQueue, but since those
  // won't run (due to our destruction), we can simply ignore the corresponding
  // will_process_pending_queue flag.
  while (!active_entries_.empty()) {
    ActiveEntry* entry = active_entries_.begin()->second;
    entry->will_process_pending_queue = false;
    entry->pending_queue.clear();
    entry->readers.clear();
    entry->writer = NULL;
    DeactivateEntry(entry);
  }

  ActiveEntriesSet::iterator it = doomed_entries_.begin();
  for (; it != doomed_entries_.end(); ++it)
    delete *it;
}

HttpTransaction* HttpCache::CreateTransaction() {
  // Do lazy initialization of disk cache if needed.
  if (!disk_cache_.get()) {
    DCHECK(cache_size_ >= 0);
    if (in_memory_cache_) {
      // We may end up with no folder name and no cache if the initialization
      // of the disk cache fails. We want to be sure that what we wanted to have
      // was an in-memory cache.
      disk_cache_.reset(disk_cache::CreateInMemoryCacheBackend(cache_size_));
    } else if (!disk_cache_dir_.empty()) {
      disk_cache_.reset(disk_cache::CreateCacheBackend(disk_cache_dir_, true,
                                                       cache_size_));
      disk_cache_dir_.clear();  // Reclaim memory.
    }
  }
  return new HttpCache::Transaction(this);
}

HttpCache* HttpCache::GetCache() {
  return this;
}

void HttpCache::Suspend(bool suspend) {
  network_layer_->Suspend(suspend);
}

// static
bool HttpCache::ReadResponseInfo(disk_cache::Entry* disk_entry,
                                 HttpResponseInfo* response_info) {
  int size = disk_entry->GetDataSize(kResponseInfoIndex);

  scoped_refptr<IOBuffer> buffer = new IOBuffer(size);
  int rv = disk_entry->ReadData(kResponseInfoIndex, 0, buffer, size, NULL);
  if (rv != size) {
    DLOG(ERROR) << "ReadData failed: " << rv;
    return false;
  }

  Pickle pickle(buffer->data(), size);
  void* iter = NULL;

  // read flags and verify version
  int flags;
  if (!pickle.ReadInt(&iter, &flags))
    return false;
  int version = flags & RESPONSE_INFO_VERSION_MASK;
  if (version != RESPONSE_INFO_VERSION) {
    DLOG(ERROR) << "unexpected response info version: " << version;
    return false;
  }

  // read request-time
  int64 time_val;
  if (!pickle.ReadInt64(&iter, &time_val))
    return false;
  response_info->request_time = Time::FromInternalValue(time_val);
  response_info->was_cached = true;  // Set status to show cache resurrection.

  // read response-time
  if (!pickle.ReadInt64(&iter, &time_val))
    return false;
  response_info->response_time = Time::FromInternalValue(time_val);

  // read response-headers
  response_info->headers = new HttpResponseHeaders(pickle, &iter);
  DCHECK(response_info->headers->response_code() != -1);

  // read ssl-info
  if (flags & RESPONSE_INFO_HAS_CERT) {
    response_info->ssl_info.cert =
        X509Certificate::CreateFromPickle(pickle, &iter);
  }
  if (flags & RESPONSE_INFO_HAS_CERT_STATUS) {
    int cert_status;
    if (!pickle.ReadInt(&iter, &cert_status))
      return false;
    response_info->ssl_info.cert_status = cert_status;
  }
  if (flags & RESPONSE_INFO_HAS_SECURITY_BITS) {
    int security_bits;
    if (!pickle.ReadInt(&iter, &security_bits))
      return false;
    response_info->ssl_info.security_bits = security_bits;
  }

  // read vary-data
  if (flags & RESPONSE_INFO_HAS_VARY_DATA) {
    if (!response_info->vary_data.InitFromPickle(pickle, &iter))
      return false;
  }

  return true;
}

// static
bool HttpCache::WriteResponseInfo(disk_cache::Entry* disk_entry,
                                  const HttpResponseInfo* response_info,
                                  bool skip_transient_headers) {
  int flags = RESPONSE_INFO_VERSION;
  if (response_info->ssl_info.cert) {
    flags |= RESPONSE_INFO_HAS_CERT;
    flags |= RESPONSE_INFO_HAS_CERT_STATUS;
  }
  if (response_info->ssl_info.security_bits != -1)
    flags |= RESPONSE_INFO_HAS_SECURITY_BITS;
  if (response_info->vary_data.is_valid())
    flags |= RESPONSE_INFO_HAS_VARY_DATA;

  Pickle pickle;
  pickle.WriteInt(flags);
  pickle.WriteInt64(response_info->request_time.ToInternalValue());
  pickle.WriteInt64(response_info->response_time.ToInternalValue());

  net::HttpResponseHeaders::PersistOptions persist_options =
      net::HttpResponseHeaders::PERSIST_RAW;

  if (skip_transient_headers) {
    persist_options =
        net::HttpResponseHeaders::PERSIST_SANS_COOKIES |
        net::HttpResponseHeaders::PERSIST_SANS_CHALLENGES |
        net::HttpResponseHeaders::PERSIST_SANS_HOP_BY_HOP |
        net::HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE;
  }

  response_info->headers->Persist(&pickle, persist_options);

  if (response_info->ssl_info.cert) {
    response_info->ssl_info.cert->Persist(&pickle);
    pickle.WriteInt(response_info->ssl_info.cert_status);
  }
  if (response_info->ssl_info.security_bits != -1)
    pickle.WriteInt(response_info->ssl_info.security_bits);

  if (response_info->vary_data.is_valid())
    response_info->vary_data.Persist(&pickle);

  scoped_refptr<WrappedIOBuffer> data = new WrappedIOBuffer(
      reinterpret_cast<const char*>(pickle.data()));
  int len = static_cast<int>(pickle.size());

  return disk_entry->WriteData(kResponseInfoIndex, 0, data, len, NULL,
                               true) == len;
}

// Generate a key that can be used inside the cache.
std::string HttpCache::GenerateCacheKey(const HttpRequestInfo* request) {
  std::string url = request->url.spec();
  if (request->url.has_ref())
    url.erase(url.find_last_of('#'));

  if (mode_ == NORMAL) {
    // No valid URL can begin with numerals, so we should not have to worry
    // about collisions with normal URLs.
    if (request->upload_data && request->upload_data->identifier())
      url.insert(0, StringPrintf("%lld/", request->upload_data->identifier()));
    return url;
  }

  // In playback and record mode, we cache everything.

  // Lazily initialize.
  if (playback_cache_map_ == NULL)
    playback_cache_map_.reset(new PlaybackCacheMap());

  // Each time we request an item from the cache, we tag it with a
  // generation number.  During playback, multiple fetches for the same
  // item will use the same generation number and pull the proper
  // instance of an URL from the cache.
  int generation = 0;
  DCHECK(playback_cache_map_ != NULL);
  if (playback_cache_map_->find(url) != playback_cache_map_->end())
    generation = (*playback_cache_map_)[url];
  (*playback_cache_map_)[url] = generation + 1;

  // The key into the cache is GENERATION # + METHOD + URL.
  std::string result = IntToString(generation);
  result.append(request->method);
  result.append(url);
  return result;
}

void HttpCache::DoomEntry(const std::string& key) {
  // Need to abandon the ActiveEntry, but any transaction attached to the entry
  // should not be impacted.  Dooming an entry only means that it will no
  // longer be returned by FindActiveEntry (and it will also be destroyed once
  // all consumers are finished with the entry).
  ActiveEntriesMap::iterator it = active_entries_.find(key);
  if (it == active_entries_.end()) {
    disk_cache_->DoomEntry(key);
  } else {
    ActiveEntry* entry = it->second;
    active_entries_.erase(it);

    // We keep track of doomed entries so that we can ensure that they are
    // cleaned up properly when the cache is destroyed.
    doomed_entries_.insert(entry);

    entry->disk_entry->Doom();
    entry->doomed = true;

    DCHECK(entry->writer || !entry->readers.empty());
  }
}

void HttpCache::FinalizeDoomedEntry(ActiveEntry* entry) {
  DCHECK(entry->doomed);
  DCHECK(!entry->writer);
  DCHECK(entry->readers.empty());
  DCHECK(entry->pending_queue.empty());

  ActiveEntriesSet::iterator it = doomed_entries_.find(entry);
  DCHECK(it != doomed_entries_.end());
  doomed_entries_.erase(it);

  delete entry;
}

HttpCache::ActiveEntry* HttpCache::FindActiveEntry(const std::string& key) {
  ActiveEntriesMap::const_iterator it = active_entries_.find(key);
  return it != active_entries_.end() ? it->second : NULL;
}

HttpCache::ActiveEntry* HttpCache::OpenEntry(const std::string& key) {
  DCHECK(!FindActiveEntry(key));

  disk_cache::Entry* disk_entry;
  if (!disk_cache_->OpenEntry(key, &disk_entry))
    return NULL;

  return ActivateEntry(key, disk_entry);
}

HttpCache::ActiveEntry* HttpCache::CreateEntry(const std::string& key) {
  DCHECK(!FindActiveEntry(key));

  disk_cache::Entry* disk_entry;
  if (!disk_cache_->CreateEntry(key, &disk_entry))
    return NULL;

  return ActivateEntry(key, disk_entry);
}

void HttpCache::DestroyEntry(ActiveEntry* entry) {
  if (entry->doomed) {
    FinalizeDoomedEntry(entry);
  } else {
    DeactivateEntry(entry);
  }
}

HttpCache::ActiveEntry* HttpCache::ActivateEntry(
    const std::string& key,
    disk_cache::Entry* disk_entry) {
  ActiveEntry* entry = new ActiveEntry(disk_entry);
  active_entries_[key] = entry;
  return entry;
}

#if defined(OS_WIN)
#pragma optimize("", off)
#endif
// Avoid optimizing local_entry out of the code.
void HttpCache::DeactivateEntry(ActiveEntry* entry) {
  // TODO(rvargas): remove this code and go back to DCHECKS once we find out
  // why are we crashing. I'm just trying to gather more info for bug 3931.
  ActiveEntry local_entry = *entry;
  size_t readers_size = local_entry.readers.size();
  size_t pending_size = local_entry.pending_queue.size();

  ActiveEntriesMap::iterator it =
      active_entries_.find(entry->disk_entry->GetKey());
  CHECK(it != active_entries_.end());
  CHECK(it->second == entry);

  if (local_entry.will_process_pending_queue || local_entry.doomed ||
      local_entry.writer || readers_size || pending_size) {
    CHECK(false);
  }

  active_entries_.erase(it);
  delete entry;

  // Avoid closing the disk_entry again on the destructor.
  local_entry.disk_entry = NULL;
}
#if defined(OS_WIN)
#pragma optimize("", on)
#endif

int HttpCache::AddTransactionToEntry(ActiveEntry* entry, Transaction* trans) {
  DCHECK(entry);

  // We implement a basic reader/writer lock for the disk cache entry.  If
  // there is already a writer, then everyone has to wait for the writer to
  // finish before they can access the cache entry.  There can be multiple
  // readers.
  //
  // NOTE: If the transaction can only write, then the entry should not be in
  // use (since any existing entry should have already been doomed).

  if (entry->writer || entry->will_process_pending_queue) {
    entry->pending_queue.push_back(trans);
    return ERR_IO_PENDING;
  }

  if (trans->mode() & Transaction::WRITE) {
    // transaction needs exclusive access to the entry
    if (entry->readers.empty()) {
      entry->writer = trans;
    } else {
      entry->pending_queue.push_back(trans);
      return ERR_IO_PENDING;
    }
  } else {
    // transaction needs read access to the entry
    entry->readers.push_back(trans);
  }

  // We do this before calling EntryAvailable to force any further calls to
  // AddTransactionToEntry to add their transaction to the pending queue, which
  // ensures FIFO ordering.
  if (!entry->writer && !entry->pending_queue.empty())
    ProcessPendingQueue(entry);

  return trans->EntryAvailable(entry);
}

void HttpCache::DoneWithEntry(ActiveEntry* entry, Transaction* trans) {
  // If we already posted a task to move on to the next transaction and this was
  // the writer, there is nothing to cancel.
  if (entry->will_process_pending_queue && entry->readers.empty())
    return;

  if (entry->writer) {
    // TODO(rvargas): convert this to a DCHECK.
    CHECK(trans == entry->writer);
    // Assume that this is not a successful write.
    DoneWritingToEntry(entry, false);
  } else {
    DoneReadingFromEntry(entry, trans);
  }
}

void HttpCache::DoneWritingToEntry(ActiveEntry* entry, bool success) {
  DCHECK(entry->readers.empty());

  entry->writer = NULL;

  if (success) {
    ProcessPendingQueue(entry);
  } else {
    // TODO(rvargas): convert this to a DCHECK.
    CHECK(!entry->will_process_pending_queue);

    // We failed to create this entry.
    TransactionList pending_queue;
    pending_queue.swap(entry->pending_queue);

    entry->disk_entry->Doom();
    DestroyEntry(entry);

    // We need to do something about these pending entries, which now need to
    // be added to a new entry.
    while (!pending_queue.empty()) {
      pending_queue.front()->AddToEntry();
      pending_queue.pop_front();
    }
  }
}

void HttpCache::DoneReadingFromEntry(ActiveEntry* entry, Transaction* trans) {
  DCHECK(!entry->writer);

  TransactionList::iterator it =
      std::find(entry->readers.begin(), entry->readers.end(), trans);
  DCHECK(it != entry->readers.end());

  entry->readers.erase(it);

  ProcessPendingQueue(entry);
}

void HttpCache::ConvertWriterToReader(ActiveEntry* entry) {
  DCHECK(entry->writer);
  DCHECK(entry->writer->mode() == Transaction::READ_WRITE);
  DCHECK(entry->readers.empty());

  Transaction* trans = entry->writer;

  entry->writer = NULL;
  entry->readers.push_back(trans);

  ProcessPendingQueue(entry);
}

void HttpCache::RemovePendingTransaction(Transaction* trans) {
  ActiveEntriesMap::const_iterator i = active_entries_.find(trans->key());
  if (i == active_entries_.end())
    return;

  TransactionList& pending_queue = i->second->pending_queue;

  TransactionList::iterator j =
      find(pending_queue.begin(), pending_queue.end(), trans);
  if (j == pending_queue.end())
    return;

  pending_queue.erase(j);
}

void HttpCache::ProcessPendingQueue(ActiveEntry* entry) {
  // Multiple readers may finish with an entry at once, so we want to batch up
  // calls to OnProcessPendingQueue.  This flag also tells us that we should
  // not delete the entry before OnProcessPendingQueue runs.
  if (entry->will_process_pending_queue)
    return;
  entry->will_process_pending_queue = true;

  MessageLoop::current()->PostTask(FROM_HERE,
      task_factory_.NewRunnableMethod(&HttpCache::OnProcessPendingQueue,
                                      entry));
}

void HttpCache::OnProcessPendingQueue(ActiveEntry* entry) {
  entry->will_process_pending_queue = false;

  // TODO(rvargas): Convert this to a DCHECK.
  CHECK(!entry->writer);

  // If no one is interested in this entry, then we can de-activate it.
  if (entry->pending_queue.empty()) {
    if (entry->readers.empty())
      DestroyEntry(entry);
    return;
  }

  // Promote next transaction from the pending queue.
  Transaction* next = entry->pending_queue.front();
  if ((next->mode() & Transaction::WRITE) && !entry->readers.empty())
    return;  // have to wait

  entry->pending_queue.erase(entry->pending_queue.begin());

  AddTransactionToEntry(entry, next);
}

//-----------------------------------------------------------------------------

}  // namespace net
