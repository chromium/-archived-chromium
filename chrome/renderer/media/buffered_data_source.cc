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

// Define the number of bytes in a megabyte.
const size_t kMegabyte = 1024 * 1024;

// Backward capacity of the buffer, by default 2MB.
const size_t kBackwardCapcity = 2 * kMegabyte;

// Forward capacity of the buffer, by default 10MB.
const size_t kForwardCapacity = 10 * kMegabyte;

// The threshold of bytes that we should wait until the data arrives in the
// future instead of restarting a new connection. This number is defined in the
// number of bytes, we should determine this value from typical connection speed
// and amount of time for a suitable wait. Now I just make a guess for this
// number to be 2MB.
// TODO(hclam): determine a better value for this.
const size_t kForwardWaitThreshold = 2 * kMegabyte;

// Defines how long we should wait for more data before we declare a connection
// timeout and start a new request.
// TODO(hclam): set it to 5s, calibrate this value later.
const int64 kDataTransferTimeoutSeconds = 5;

// Defines how many times we should try to read from a buffered resource loader
// before we declare a read error. After each failure of read from a buffered
// resource loader, a new one is created to be read.
const size_t kReadTrials = 3;

// A helper method that accepts only HTTP, HTTPS and FILE protocol.
bool IsSchemeSupported(const GURL& url) {
  return url.SchemeIs(kHttpScheme) ||
         url.SchemeIs(kHttpsScheme) ||
         url.SchemeIsFile();
}

}  // namespace

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader
BufferedResourceLoader::BufferedResourceLoader(
    MessageLoop* message_loop,
    webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory,
    const GURL& url,
    int64 first_byte_position,
    int64 last_byte_position)
    : start_callback_(NULL),
      bridge_(NULL),
      offset_(0),
      content_length_(kPositionNotSpecified),
      completion_error_(net::OK),
      buffer_(new media::SeekableBuffer(kBackwardCapcity, kForwardCapacity)),
      deferred_(false),
      stopped_(false),
      completed_(false),
      range_requested_(false),
      async_start_(false),
      bridge_factory_(bridge_factory),
      url_(url),
      first_byte_position_(first_byte_position),
      last_byte_position_(last_byte_position),
      render_loop_(message_loop),
      buffer_available_(&lock_) {
}

BufferedResourceLoader::~BufferedResourceLoader() {
}

int BufferedResourceLoader::Start(net::CompletionCallback* start_callback) {
  AutoLock auto_lock(lock_);

  // Make sure we only start no more than once.
  DCHECK(!bridge_.get());
  DCHECK(!start_callback_.get());
  start_callback_.reset(start_callback);

  // Save the information that we are doing an asynchronous start since
  // start_callback_ may get reset, we can't rely on it.
  if (start_callback_.get())
    async_start_ = true;

  // We may receive stop signal while we are inside this method, it's because
  // Start() may get called on demuxer thread while Stop() is called on
  // pipeline thread.
  if (!stopped_) {
    render_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &BufferedResourceLoader::OnStart));

    // Wait for response to arrive if we don't have an async start.
    // TODO(hclam): implement start timeout.
    if (!async_start_)
      buffer_available_.Wait();
  }

  // We may have stopped because of a bad response from the server.
  if (stopped_)
    return net::ERR_ABORTED;
  else if (completed_)
    return completion_error_;
  else if (async_start_)
    return net::ERR_IO_PENDING;
  return net::OK;
}

void BufferedResourceLoader::Stop() {
  AutoLock auto_lock(lock_);
  stopped_ = true;
  buffer_.reset();

  // Wakes up the waiting thread so they can catch the stop signal.
  buffer_available_.Signal();

  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &BufferedResourceLoader::OnDestroy));
}

int BufferedResourceLoader::Read(uint8* buffer,
                                 size_t* bytes_read,
                                 int64 position,
                                 size_t read_size) {
  // We are given the position to read from, so we will perform a seek first.
  int error = SeekInternal(position);
  if (error != net::OK)
    return error;

  // Then we perform a read.
  error = ReadInternal(buffer, bytes_read, read_size);
  if (error != net::OK)
    return error;

  // After a read operation, determine whether or not we need to disable
  // defer loading.
  if (ShouldDisableDefer()) {
    AutoLock auto_lock(lock_);
    if (!stopped_) {
      render_loop_->PostTask(FROM_HERE,
          NewRunnableMethod(this,
                            &BufferedResourceLoader::OnDisableDeferLoading));
    }
  }

  return net::OK;
}

int64 BufferedResourceLoader::GetOffset() {
  AutoLock auto_lock(lock_);
  return offset_;
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
  // Save the status error before we signal a the completion of the request
  // so blocked methods can pick the error and do interpretations.
  SignalComplete(status.os_error());

  // After the response has completed, we don't need the bridge any more.
  bridge_.reset();

  if (async_start_)
    InvokeAndResetStartCallback(status.os_error());
}

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader, private
int BufferedResourceLoader::ReadInternal(uint8* buffer,
                                         size_t* bytes_read,
                                         size_t read_size) {
  DCHECK(buffer);
  DCHECK(bytes_read);

  AutoLock auto_lock(lock_);

  // Return value for this method.
  int error = net::OK;
  // Ever waited for data to be downloaded.
  bool waited_for_buffer = false;
  // Total amount of bytes read from buffer.
  size_t taken = 0;
  while (taken < read_size) {
    // If stopped or the request has been completed, break the loop.
    if (stopped_) {
      error = net::ERR_ABORTED;
      break;
    }

    // Read into |data|.
    size_t unread_bytes = read_size - taken;
    size_t bytes_read_this_time = buffer_->Read(unread_bytes, buffer + taken);

    // Increment the total bytes read from the buffer so we know when to stop
    // reading.
    taken += bytes_read_this_time;
    DCHECK_LE(taken, read_size);

    // There are three conditions when we should terminate this loop:
    // 1. We have read enough bytes so we don't need to read anymore.
    // 2. The request has completed and the buffer is empty. We don't want to
    //    wait an additional iteration, so break on condition of empty forward
    //    buffer instead of breaking on failed read.
    // 3. The request has timed out. We waited for data in the last iteration
    //    but there isn't any more data in the buffer.
    if (taken == read_size) {
      break;
    }
    if (completed_ && buffer_->forward_bytes() == 0) {
      // We should return the completion error received.
      error = completion_error_;
      break;
    }
    if (waited_for_buffer && bytes_read_this_time == 0) {
      error = net::ERR_TIMED_OUT;
      break;
    }

    buffer_available_.TimedWait(
        base::TimeDelta::FromSeconds(kDataTransferTimeoutSeconds));
    waited_for_buffer = true;
  }

  // Adjust the offset and disable defer loading if needed.
  if (taken > 0)
    offset_ += taken;
  *bytes_read = taken;
  return error;
}

int BufferedResourceLoader::SeekInternal(int64 position) {
  // Use of |offset_| is safe without a lock, because it's modified only in
  // Read() and this method after Start(). Read() and Seek() happens
  // on the same thread.
  if (position == offset_)
    return net::OK;

  // Use the conditions to avoid overflow, since |position| and |offset_| are
  // int64, their difference can be greater than an int32.
  if (position > offset_ + kint32max)
    return net::ERR_FAILED;
  else if (position < offset_ + kint32min)
    return net::ERR_FAILED;

  int32 offset = static_cast<int32>(position - offset_);
  // Backward data are served directly from the buffer and will not be
  // downloaded in the future so we perform a backward seek now.
  if (offset < 0) {
    AutoLock auto_lock(lock_);
    if (buffer_->Seek(offset)) {
      offset_ = position;
      return net::OK;
    }
    return net::ERR_FAILED;
  }

  // If we are seeking too far ahead that we'll wait too long, return false.
  // We only perform this check for forward seeking because we don't want to
  // wait too long for data very far ahead to be downloaded.
  if (position >= offset_ + kForwardWaitThreshold)
    return net::ERR_FAILED;

  AutoLock auto_lock(lock_);

  // Ever waited for data to be downloaded.
  bool waited_for_buffer = false;
  // Perform seeking forward until we get to the offset.
  while(offset_ < position) {
    // Loader has stopped.
    if (stopped_)
      return net::ERR_ABORTED;

    // Response completed and seek position exceeds buffered range, we won't
    // receive any more data so return immediately. We have |completion_error_|
    // from OnCompletedRequest() so return it straight.
    if (completed_ && position >= offset_ + buffer_->forward_bytes())
      return completion_error_;

    // Seek as much as possible until we get the desired position.
    int32 forward_seek = std::min(static_cast<int32>(buffer_->forward_bytes()),
                                  offset);

    // If we have waited for buffer in the last loop iteration and we don't
    // get any more buffer, declare a timeout situation.
    if (waited_for_buffer && !forward_seek)
      return net::ERR_TIMED_OUT;

    if (!buffer_->Seek(forward_seek)) {
      NOTREACHED() << "We should have enough bytes but forward seek failed";
    }

    // Keep track of the total amount that we should seek forward. Decrements
    // this so we will hit the end of loop condition.
    offset -= forward_seek;

    // Increments the offset in the whole instance.
    offset_ += forward_seek;
    DCHECK_LE(offset_, position);

    // Break the loop if we reached the target position so we don't wait.
    if (offset_ == position)
      break;

    buffer_available_.TimedWait(
        base::TimeDelta::FromSeconds(kDataTransferTimeoutSeconds));
    waited_for_buffer = true;
  }
  return net::OK;
}

void BufferedResourceLoader::AppendToBuffer(const uint8* data, size_t size) {
  AutoLock auto_lock(lock_);
  if (!stopped_)
    buffer_->Append(size, data);
  buffer_available_.Signal();
}

void BufferedResourceLoader::SignalComplete(int error) {
  AutoLock auto_lock(lock_);
  completed_ = true;
  completion_error_ = error;
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
  DCHECK(!bridge_.get());

  AutoLock auto_lock(lock_);
  if (stopped_)
    return;

  if (first_byte_position_ != kPositionNotSpecified) {
    range_requested_ = true;
    // TODO(hclam): server may not support range request so |offset_| may not
    // equal to |first_byte_position_|.
    offset_ = first_byte_position_;
  }

  // Creates the bridge on render thread since we can only access
  // ResourceDispatcher on this thread.
  bridge_.reset(bridge_factory_->CreateBridge(url_,
                                              net::LOAD_BYPASS_CACHE,
                                              first_byte_position_,
                                              last_byte_position_));

  // And start the resource loading.
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
BufferedDataSource::BufferedDataSource(
    MessageLoop* render_loop,
    webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory)
    : stopped_(false),
      position_(0),
      total_bytes_(kPositionNotSpecified),
      bridge_factory_(bridge_factory),
      buffered_resource_loader_(NULL),
      render_loop_(render_loop),
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
          render_loop_,
          bridge_factory_.get(),
          url_,
          kPositionNotSpecified,
          kPositionNotSpecified);
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
  for (size_t trials = kReadTrials; trials > 0; --trials) {
    scoped_refptr<BufferedResourceLoader> resource_loader = NULL;
    {
      AutoLock auto_lock(lock_);
      resource_loader = buffered_resource_loader_;
    }

    size_t read = 0;
    int error = net::ERR_FAILED;
    if (resource_loader)
      error = resource_loader->Read(data, &read, position_, size);

    if (error == net::OK) {
      if (read >= 0) {
        position_ += read;
        return read;
      } else {
        return DataSource::kReadError;
      }
    } else {
      // We don't need the old BufferedResourceLoader, stop it and release the
      // reference.
      if (resource_loader) {
        resource_loader->Stop();
        resource_loader = NULL;
      }

      // Create a new request if we have not stopped.
      {
        AutoLock auto_lock(lock_);
        if (stopped_) {
          buffered_resource_loader_ = NULL;
          return DataSource::kReadError;
        }

        // Create a new resource loader.
        buffered_resource_loader_ =
            new BufferedResourceLoader(render_loop_,
                                       bridge_factory_.get(),
                                       url_,
                                       position_,
                                       kPositionNotSpecified);
        // Save the local copy.
        resource_loader = buffered_resource_loader_;
      }

      // Start the new resource loader.
      DCHECK(resource_loader);
      int error = resource_loader->Start(NULL);

      // Always fail if we can't start.
      // TODO(hclam): should handle timeout.
      if (error != net::OK) {
        HandleError(media::PIPELINE_ERROR_NETWORK);
        return DataSource::kReadError;
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
