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
      buffered_bytes_(0),
      buffer_limit_(10240000),  // By default 10MB.
      buffer_event_(false, false),
      deferred_(false),
      stopped_(false),
      completed_(false),
      range_requested_(false),
      async_start_(false),
      routing_id_(routing_id),
      url_(url),
      first_byte_position_(first_byte_position),
      last_byte_position_(last_byte_position),
      render_loop_(RenderThread::current()->message_loop()) {
}

BufferedResourceLoader::~BufferedResourceLoader() {
  STLDeleteElements<BufferQueue>(&buffers_);
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
  bool task_posted = false;
  {
    AutoLock auto_lock(lock_);
    if (!stopped_) {
      task_posted =  true;
      render_loop_->PostTask(FROM_HERE,
          NewRunnableMethod(this, &BufferedResourceLoader::OnStart));
    }
  }

  // Wait for response to arrive we don't have an async start.
  if (task_posted && !async_start_)
    buffer_event_.Wait();

  {
    AutoLock auto_lock(lock_);
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
  BufferQueue delete_queue;
  {
    AutoLock auto_lock(lock_);
    stopped_ = true;
    // Use of |buffers_| is protected by the lock, we can destroy it safely.
    delete_queue.swap(buffers_);
    buffers_.clear();
  }
  STLDeleteElements<BufferQueue>(&delete_queue);

  // Wakes up the waiting thread so they can catch the stop signal.
  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &BufferedResourceLoader::OnDestroy));
  buffer_event_.Signal();
}

size_t BufferedResourceLoader::Read(uint8* data, size_t size) {
  size_t taken = 0;
  while (taken < size) {
    Buffer* buffer = NULL;
    {
      AutoLock auto_lock(lock_);
      if (stopped_)
        break;
      if (!buffers_.empty())
        buffer = buffers_.front();
      else if (completed_)
        break;
    }
    if (buffer) {
      size_t copy = std::min(size - taken, buffer->size - buffer->taken);
      memcpy(data + taken, buffer->data.get() + buffer->taken, copy);
      taken += copy;
      buffer->taken += copy;
      if (buffer->taken == buffer->size) {
        // The buffer has been consumed, remove it.
        {
          AutoLock auto_lock(lock_);
          buffers_.pop_front();
        }
        delete buffer;
      }
    } else {
      buffer_event_.Wait();
    }
  }
  if (taken > 0) {
    offset_ += taken;
    {
      AutoLock auto_lock(lock_);
      buffered_bytes_ -= taken;
      DCHECK(buffered_bytes_ >= 0);
    }
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

bool BufferedResourceLoader::SeekForward(int64 position) {
  // Use of |offset_| is safe without a lock, because it's modified only in
  // Read() and this method after Start(). Read() and SeekForward() happens
  // on the same thread.
  // Seeking backward.
  if (position < offset_)
    return false;
  // Done seeking forward.
  else if (position == offset_)
    return true;

  while(true) {
    {
      AutoLock auto_lock(lock_);
      // Loader has stopped.
      if (stopped_)
        return false;
      // Seek position exceeds bufferable range, buffer_limit_ can be changed.
      if (position >= offset_ + buffer_limit_)
        return false;
      // Response completed and seek position exceeds buffered range.
      if (completed_ && position >= offset_ + buffered_bytes_)
        return false;

      if (!buffers_.empty()) {
        Buffer* buffer = buffers_.front();
        int64 bytes_to_take = position - offset_;
        if (!buffers_.empty()) {
          size_t available_bytes_in_buffer = buffer->size - buffer->taken;
          size_t taken = 0;
          if (available_bytes_in_buffer <= bytes_to_take) {
            taken = available_bytes_in_buffer;
            buffers_.pop_front();
            delete buffer;
          } else {
            taken = static_cast<size_t>(bytes_to_take);
            buffer->taken += taken;
          }
          offset_ += taken;
          if (bytes_to_take == taken)
            return true;
        }
        continue;
      }
    }
    buffer_event_.Wait();
  }
}

int64 BufferedResourceLoader::GetOffset() {
  AutoLock auto_lock(lock_);
  return offset_;
}

int64 BufferedResourceLoader::GetBufferLimit() {
  AutoLock auto_lock(lock_);
  return buffer_limit_;
}

void BufferedResourceLoader::SetBufferLimit(size_t buffer_limit) {
  {
    AutoLock auto_lock(lock_);
    buffer_limit_ = buffer_limit;
  }

  if (ShouldDisableDefer()) {
    AutoLock auto_lock(lock_);
    if (!stopped_) {
      render_loop_->PostTask(FROM_HERE,
          NewRunnableMethod(this,
                            &BufferedResourceLoader::OnDisableDeferLoading));
    }
  }
  if (ShouldEnableDefer()) {
    AutoLock auto_lock(lock_);
    if (!stopped_) {
      render_loop_->PostTask(FROM_HERE,
          NewRunnableMethod(this,
                            &BufferedResourceLoader::OnEnableDeferLoading));
    }
  }
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
  }

  // If we have started asynchronously we just need to invoke the callback or
  // we need to signal the Start() method to wake up.
  if (async_start_)
    InvokeAndResetStartCallback(net::OK);
  else
    buffer_event_.Signal();
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
  {
    AutoLock auto_lock(lock_);
    if (!stopped_) {
      Buffer* buffer = new Buffer(size);
      memcpy(buffer->data.get(), data, size);
      buffers_.push_back(buffer);
      buffered_bytes_ += size;
    }
  }
  buffer_event_.Signal();
}

void BufferedResourceLoader::SignalComplete() {
  {
    AutoLock auto_lock(lock_);
    completed_ = true;
  }
  buffer_event_.Signal();
}

bool BufferedResourceLoader::ShouldEnableDefer() {
  AutoLock auto_lock(lock_);
  if (deferred_) {
    return false;
  } else if (buffered_bytes_ >= buffer_limit_) {
    deferred_ = true;
    return true;
  }
  return false;
}

bool BufferedResourceLoader::ShouldDisableDefer() {
  AutoLock auto_lock(lock_);
  if (deferred_ && buffered_bytes_ < buffer_limit_ / 2)
    return true;
  else
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
  // 1. Use the existing resource loader to seek forward and read from it.
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

    if (resource_loader && resource_loader->SeekForward(position_)) {
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
  position_ = position;
  return true;
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
