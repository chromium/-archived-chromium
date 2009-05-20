// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/renderer/media/buffered_data_source.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/render_thread.h"
#include "media/base/filter_host.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "webkit/glue/webappcachecontext.h"

namespace {

const char kHttpScheme[] = "http";
const char kHttpsScheme[] = "https";
const int64 kPositionNotSpecified = -1;
const int kHttpOK = 200;
const int kHttpPartialContent = 206;

// Backward capacity of the buffer, by default 2MB.
const size_t kBackwardCapcity = 2048000;

// Forward capacity of the buffer, by default 10MB.
const size_t kForwardCapacity = 10240000;

// A helper method that accepts only HTTP, HTTPS and FILE protocol.
bool IsSchemeSupported(const GURL& url) {
  return url.SchemeIs(kHttpScheme) ||
         url.SchemeIs(kHttpsScheme) ||
         url.SchemeIsFile();
}

}  // namespace

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader
BufferedResourceLoader::BufferedResourceLoader(int32 routing_id,
                                               const GURL& url,
                                               int64 first_byte_position,
                                               int64 last_byte_position)
    : start_callback_(NULL),
      bridge_(NULL),
      offset_(0),
      content_length_(kPositionNotSpecified),
      buffer_(new media::SeekableBuffer(kBackwardCapcity, kForwardCapacity)),
      deferred_(false),
      stopped_(false),
      completed_(false),
      range_requested_(false),
      async_start_(false),
      routing_id_(routing_id),
      url_(url),
      first_byte_position_(first_byte_position),
      last_byte_position_(last_byte_position),
      render_loop_(RenderThread::current()->message_loop()),
      buffer_available_(&lock_) {
}

BufferedResourceLoader::~BufferedResourceLoader() {
}

int BufferedResourceLoader::Start(net::CompletionCallback* start_callback) {
  // Make sure we only start no more than once.
  DCHECK(!bridge_.get());
  DCHECK(!start_callback_.get());
  start_callback_.reset(start_callback);

  // Save the information that we are doing an asynchronous start since
  // start_callback_ may get reset, we can't rely on it.
  if (start_callback_.get())
    async_start_ = true;

  std::string header;
  if (first_byte_position_ != kPositionNotSpecified &&
      last_byte_position_ != kPositionNotSpecified) {
    header = StringPrintf("Range: bytes=%lld-%lld",
                          first_byte_position_,
                          last_byte_position_);
    range_requested_ = true;
    offset_ = first_byte_position_;
  } else if (first_byte_position_ != kPositionNotSpecified) {
    header = StringPrintf("Range: bytes=%lld-", first_byte_position_);
    range_requested_ = true;
    offset_ = first_byte_position_;
  } else if (last_byte_position_ != kPositionNotSpecified) {
    NOTIMPLEMENTED() << "Suffix length range request not implemented.";
  }

  bridge_.reset(RenderThread::current()->resource_dispatcher()->CreateBridge(
      "GET",
      GURL(url_),
      GURL(url_),
      GURL(),         // TODO(hclam): provide referer here.
      "null",         // TODO(abarth): provide frame_origin
      "null",         // TODO(abarth): provide main_frame_origin
      header,
      net::LOAD_BYPASS_CACHE,
      base::GetCurrentProcId(),
      ResourceType::MEDIA,
      0,
      // TODO(michaeln): delegate->mediaplayer->frame->
      //                    app_cache_context()->context_id()
      // For now don't service media resource requests from the appcache.
      WebAppCacheContext::kNoAppCacheContextId,
      routing_id_));

  // We may receive stop signal while we are inside this method, it's because
  // Start() may get called on demuxer thread while Stop() is called on
  // pipeline thread, so we want to protect the posting of OnStart() task
  // with a lock.
  {
    AutoLock auto_lock(lock_);
    if (!stopped_) {
      render_loop_->PostTask(FROM_HERE,
          NewRunnableMethod(this, &BufferedResourceLoader::OnStart));

      // Wait for response to arrive if we don't have an async start.
      if (!async_start_)
        buffer_available_.Wait();
    }

    // We may have stopped because of a bad response from the server.
    if (stopped_)
      return net::ERR_ABORTED;
    else if (completed_)
      return net::ERR_FAILED;
    else if (async_start_)
      return net::ERR_IO_PENDING;
    return net::OK;
  }
}

void BufferedResourceLoader::Stop() {
  {
    AutoLock auto_lock(lock_);
    stopped_ = true;
    buffer_.reset();

    // Wakes up the waiting thread so they can catch the stop signal.
    buffer_available_.Signal();
  }

  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &BufferedResourceLoader::OnDestroy));
}

size_t BufferedResourceLoader::Read(uint8* data, size_t size) {
  size_t taken = 0;
  {
    AutoLock auto_lock(lock_);
    while (taken < size) {
      // If stopped or the request has been completed, break the loop.
      if (stopped_)
        break;
      // Read into |data|.
      size_t unread_bytes = size - taken;
      size_t bytes_read = buffer_->Read(unread_bytes, data + taken);
      taken += bytes_read;
      DCHECK_LE(taken, size);
      if (taken == size)
        break;
      if (completed_ && bytes_read < unread_bytes)
        break;
      buffer_available_.Wait();
    }
  }

  // Adjust the offset and disable defer loading if needed.
  if (taken > 0) {
    offset_ += taken;
    if (ShouldDisableDefer()) {
      AutoLock auto_lock(lock_);
      if (!stopped_) {
        render_loop_->PostTask(FROM_HERE,
            NewRunnableMethod(this,
                              &BufferedResourceLoader::OnDisableDeferLoading));
      }
    }
  }
  return taken;
}

bool BufferedResourceLoader::Seek(int64 position) {
  // Use of |offset_| is safe without a lock, because it's modified only in
  // Read() and this method after Start(). Read() and Seek() happens
  // on the same thread.
  if (position == offset_)
    return true;

  // Use the conditions to avoid overflow, since |position| and |offset_| are
  // int64, their difference can be greater than an int32.
  if (position > offset_ + kint32max)
    return false;
  else if (position < offset_ + kint32min)
    return false;

  int32 offset = static_cast<int32>(position - offset_);
  // Backward data are served directly from the buffer and will not be
  // downloaded in the future so we perform a backward seek now.
  if (offset < 0) {
    AutoLock auto_lock(lock_);
    if (buffer_->Seek(offset)) {
      offset_ = position;
      return true;
    }
    return false;
  }

  // If we are seeking too far ahead that the buffer cannot serve, return false.
  // We only perform this check for forward seeking because we don't want to
  // wait too long for data very far ahead to be downloaded, and of course we
  // will never be able to seek to that position.
  if (position >= offset_ + buffer_->forward_capacity())
    return false;

  // Perform seeking forward until we get to the offset.
  AutoLock auto_lock(lock_);
  while(true) {
    // Loader has stopped.
    if (stopped_)
      return false;
    // Response completed and seek position exceeds buffered range.
    if (completed_ && position >= offset_ + buffer_->forward_bytes())
      return false;
    if (buffer_->Seek(offset)) {
      offset_ = position;
      break;
    }
    buffer_available_.Wait();
  }
  return true;
}

int64 BufferedResourceLoader::GetOffset() {
  AutoLock auto_lock(lock_);
  return offset_;
}

size_t BufferedResourceLoader::GetTimeout() {
  // TODO(hclam): implement.
  return 0;
}

void BufferedResourceLoader::SetTimeout(size_t milliseconds) {
  // TODO(hclam): implement.
}

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader,
//     webkit_glue::ResourceLoaderBridge::Peer implementations
void BufferedResourceLoader::OnReceivedRedirect(const GURL& new_url) {
  url_ = new_url;

  // If we got redirected to an unsupported protocol then stop.
  if (!IsSchemeSupported(new_url))
    Stop();
}

void BufferedResourceLoader::OnReceivedResponse(
    const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
    bool content_filtered) {
  int64 first_byte_position = -1;
  int64 last_byte_position = -1;
  int64 instance_size = -1;

  // The file:// protocol should be able to serve any request we want, so we
  // take an exception for file protocol.
  if (!url_.SchemeIsFile()) {
    if (!info.headers) {
      // We expect to receive headers because this is a HTTP or HTTPS protocol,
      // if not report failure.
      InvokeAndResetStartCallback(net::ERR_INVALID_RESPONSE);
      Stop();
      return;
    } else if (range_requested_) {
      if (info.headers->response_code() != kHttpPartialContent ||
          !info.headers->GetContentRange(&first_byte_position,
                                         &last_byte_position,
                                         &instance_size)) {
        // We requested a range, but server didn't reply with partial content or
        // the "Content-Range" header is corrupted.
        InvokeAndResetStartCallback(net::ERR_INVALID_RESPONSE);
        Stop();
        return;
      }
    } else if (info.headers->response_code() != kHttpOK) {
      // We didn't request a range but server didn't reply with "200 OK".
      InvokeAndResetStartCallback(net::ERR_FAILED);
      Stop();
      return;
    }
  }

  {
    AutoLock auto_lock(lock_);
    // |info.content_length| can be -1, in that case |content_length_| is
    // not specified and this is a streaming response.
    content_length_ = info.content_length;
    // We only care about the first byte position if it's given by the server.
    if (first_byte_position != kPositionNotSpecified)
      offset_ = first_byte_position;

    // If this is not an asynchronous start, signal the thread called Start().
    if (!async_start_)
      buffer_available_.Signal();
  }

  // If we have started asynchronously we need to invoke the callback.
  if (async_start_)
    InvokeAndResetStartCallback(net::OK);
}

void BufferedResourceLoader::OnReceivedData(const char* data, int len) {
  DCHECK(bridge_.get());

  AppendToBuffer(reinterpret_cast<const uint8*>(data), len);
  if (ShouldEnableDefer())
    bridge_->SetDefersLoading(true);
}

void BufferedResourceLoader::OnCompletedRequest(
    const URLRequestStatus& status, const std::string& security_info) {
  SignalComplete();

  // After the response has completed, we don't need the bridge any more.
  bridge_.reset();

  if (async_start_)
    InvokeAndResetStartCallback(status.os_error());
}

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader, private
void BufferedResourceLoader::AppendToBuffer(const uint8* data, size_t size) {
  AutoLock auto_lock(lock_);
  if (!stopped_)
    buffer_->Append(size, data);
  buffer_available_.Signal();
}

void BufferedResourceLoader::SignalComplete() {
  AutoLock auto_lock(lock_);
  completed_ = true;
  buffer_available_.Signal();
}

bool BufferedResourceLoader::ShouldEnableDefer() {
  AutoLock auto_lock(lock_);

  // If the resource loader has been stopped, we should not use |buffer_|.
  if (stopped_)
    return false;

  if (!deferred_ && buffer_->forward_bytes() >= buffer_->forward_capacity()) {
    deferred_ = true;
    return true;
  }
  return false;
}

bool BufferedResourceLoader::ShouldDisableDefer() {
  AutoLock auto_lock(lock_);

  // If the resource loader has been stopped, we should not use |buffer_|.
  if (stopped_)
    return false;

  if (deferred_ && buffer_->forward_bytes() < buffer_->forward_capacity() / 2) {
    deferred_ = false;
    return true;
  }
  return false;
}

void BufferedResourceLoader::OnStart() {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(bridge_.get());
  bridge_->Start(this);
}

void BufferedResourceLoader::OnDestroy() {
  DCHECK(MessageLoop::current() == render_loop_);
  if (bridge_.get()) {
    // Cancel the resource request.
    bridge_->Cancel();
    bridge_.reset();
  }
}

void BufferedResourceLoader::OnEnableDeferLoading() {
  DCHECK(MessageLoop::current() == render_loop_);
  // This message may arrive after the bridge is destroyed.
  if (bridge_.get())
    bridge_->SetDefersLoading(true);
}

void BufferedResourceLoader::OnDisableDeferLoading() {
  DCHECK(MessageLoop::current() == render_loop_);
  // This message may arrive after the bridge is destroyed.
  if (bridge_.get())
    bridge_->SetDefersLoading(false);
}

void BufferedResourceLoader::InvokeAndResetStartCallback(int error) {
  AutoLock auto_lock(lock_);
  if (start_callback_.get()) {
    start_callback_->Run(error);
    start_callback_.reset();
  }
}

//////////////////////////////////////////////////////////////////////////////
// BufferedDataSource
BufferedDataSource::BufferedDataSource(int routing_id)
    : routing_id_(routing_id),
      stopped_(false),
      position_(0),
      total_bytes_(kPositionNotSpecified),
      buffered_resource_loader_(NULL),
      pipeline_loop_(MessageLoop::current()) {
}

BufferedDataSource::~BufferedDataSource() {
}

void BufferedDataSource::Stop() {
  scoped_refptr<BufferedResourceLoader> resource_loader = NULL;
  // Set the stop signal first.
  {
    AutoLock auto_lock(lock_);
    stopped_ = true;
    resource_loader = buffered_resource_loader_;
    // Release the reference to the resource loader.
    buffered_resource_loader_ = NULL;
  }
  // Tell the loader to stop.
  if (resource_loader)
    resource_loader->Stop();
}

bool BufferedDataSource::Initialize(const std::string& url) {
  // Save the url.
  url_ = GURL(url);

  // Make sure we support the scheme of the URL.
  if (!IsSchemeSupported(url_)) {
    host_->Error(media::PIPELINE_ERROR_NETWORK);
    return false;
  }

  media_format_.SetAsString(media::MediaFormat::kMimeType,
                            media::mime_type::kApplicationOctetStream);
  media_format_.SetAsString(media::MediaFormat::kURL, url);

  // Setup the BufferedResourceLoader here.
  scoped_refptr<BufferedResourceLoader> resource_loader = NULL;
  {
    AutoLock auto_lock(lock_);
    if (!stopped_) {
      buffered_resource_loader_ = new BufferedResourceLoader(
          routing_id_, url_, kPositionNotSpecified, kPositionNotSpecified);
      resource_loader = buffered_resource_loader_;
    }
  }

  // Use the local reference to start the request.
  if (resource_loader) {
    if (net::ERR_IO_PENDING != resource_loader->Start(
            NewCallback(this, &BufferedDataSource::InitialRequestStarted))) {
      host_->Error(media::PIPELINE_ERROR_NETWORK);
      return false;
    }
    return true;
  }
  host_->Error(media::PIPELINE_ERROR_NETWORK);
  return false;
}

size_t BufferedDataSource::Read(uint8* data, size_t size) {
  // We try two times here:
  // 1. Use the existing resource loader to seek and read from it.
  // 2. If any of the above operations failed, we create a new resource loader
  //    starting with a new range. Goto 1.
  // TODO(hclam): change the logic here to do connection recovery and allow a
  // maximum of trials.
  for (int trials = 2; trials > 0; --trials) {
    scoped_refptr<BufferedResourceLoader> resource_loader = NULL;
    {
      AutoLock auto_lock(lock_);
      resource_loader = buffered_resource_loader_;
    }

    if (resource_loader && resource_loader->Seek(position_)) {
      size_t read = resource_loader->Read(data, size);
      if (read >= 0) {
        position_ += read;
        return read;
      } else {
        return DataSource::kReadError;
      }
    } else {
      // We enter here because the current resource loader cannot serve the
      // range requested, we will need to create a new request for doing it.
      scoped_refptr<BufferedResourceLoader> old_resource_loader = NULL;
      {
        AutoLock auto_lock(lock_);
        if (stopped_)
          return DataSource::kReadError;

        // Save the reference to the old resource loader, if we have a local
        // reference, use this local reference instead of creating a new one.
        if (resource_loader)
          old_resource_loader = resource_loader;
        else
          old_resource_loader = buffered_resource_loader_;

        // Create a new resource loader.
        resource_loader =
            new BufferedResourceLoader(routing_id_, url_, position_,
                                       kPositionNotSpecified);
        buffered_resource_loader_ = resource_loader;
      }
      if (old_resource_loader) {
        old_resource_loader->Stop();
        old_resource_loader = NULL;
      }
      if (resource_loader) {
        if (net::OK != resource_loader->Start(NULL)) {
          // We have started a new request but failed, report that.
          // TODO(hclam): should allow some retry mechanism here.
          HandleError(media::PIPELINE_ERROR_NETWORK);
          return DataSource::kReadError;
        }
      }
    }
  }
  return DataSource::kReadError;
}

bool BufferedDataSource::GetPosition(int64* position_out) {
  *position_out = position_;
  return true;
}

bool BufferedDataSource::SetPosition(int64 position) {
  // |total_bytes_| can be -1 for pure streaming. There may be a problem with
  // seeking for this case.
  if (position < total_bytes_) {
    position_ = position;
    return true;
  }
  return false;
}

bool BufferedDataSource::GetSize(int64* size_out) {
  if (total_bytes_ != kPositionNotSpecified) {
    *size_out = total_bytes_;
    return true;
  }
  *size_out = 0;
  return false;
}

bool BufferedDataSource::IsSeekable() {
  return total_bytes_ != kPositionNotSpecified;
}

void BufferedDataSource::HandleError(media::PipelineError error) {
  AutoLock auto_lock(lock_);
  if (!stopped_) {
    host_->Error(error);
  }
}

void BufferedDataSource::InitialRequestStarted(int error) {
  // Don't take any lock and call to |host_| here, this method is called from
  // BufferedResourceLoader after the response has started or failed, it is
  // very likely we are called within a lock in BufferedResourceLoader.
  // Acquiring an additional lock here we might have a deadlock situation,
  // but one thing very sure is that pipeline thread is still alive, so we
  // just need to post a task on that thread.
  pipeline_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &BufferedDataSource::OnInitialRequestStarted, error));
}

void BufferedDataSource::OnInitialRequestStarted(int error) {
  // Acquiring a lock should not be needed because stopped_ is only written
  // on pipeline thread and we are on pipeline thread but just to be safe.
  AutoLock auto_lock(lock_);
  if (!stopped_) {
    if (error == net::OK) {
      total_bytes_ = buffered_resource_loader_->content_length();
      if (IsSeekable()) {
        host_->SetTotalBytes(total_bytes_);
        // TODO(hclam): report the amount of bytes buffered accurately.
        host_->SetBufferedBytes(total_bytes_);
      }
      host_->InitializationComplete();
    } else {
      host_->Error(media::PIPELINE_ERROR_NETWORK);
    }
  }
}

const media::MediaFormat& BufferedDataSource::media_format() {
  return media_format_;
}
