// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_BUFFERED_DATA_SOURCE_H_
#define CHROME_RENDERER_MEDIA_BUFFERED_DATA_SOURCE_H_

#include <deque>
#include <string>

#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "base/waitable_event.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/base/media_format.h"
#include "media/base/pipeline.h"
#include "net/base/completion_callback.h"
#include "net/base/file_stream.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "googleurl/src/gurl.h"

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader
// This class works inside demuxer thread and render thread. It contains a
// resource loader bridge and does the actual resource loading. This object
// does buffering internally, it defers the resource loading if buffer is
// full and un-defers the resource loading if it is under buffered.
class BufferedResourceLoader :
    public base::RefCountedThreadSafe<BufferedResourceLoader>,
    public webkit_glue::ResourceLoaderBridge::Peer {
 public:
  BufferedResourceLoader(int32 routing_id,
                         const GURL& url,
                         int64 first_byte_position,
                         int64 last_byte_position);
  virtual ~BufferedResourceLoader();

  // Start the resource loading with the specified URL and range.
  // This method call can operate in two modes, synchronous and asynchronous.
  // If |start_callback| is NULL, this method operates in synchronous mode and
  // it returns true if the load has started successfully, false otherwise. It
  // returns only if a resource is received from the server or this loader is
  // called to stop.
  // If |start_callback| is not NULL, this method operates in asynchronous mode
  // and it returns net::ERR_IO_PENDING if the request is going to start.
  // Once there's a response from the server, success or fail |start_callback|
  // is called with the result.
  // Note that |start_callback| is called within a lock to prevent invoking an
  // invalid callback method while this object is called to stop.
  int Start(net::CompletionCallback* callback);

  // Stops this loader. Wakes up all synchronous actions.
  void Stop();

  // Reads the specified |size| into |buffer| and returns number of bytes copied
  // into the buffer. Returns 0 if the response has completed and there's no
  // no buffer left. Returns media::kReadError on error. The read starts from
  // the current position referred by calling GetOffset(). This method call is
  // synchronous, it returns only the required amount of bytes is read, the
  // loader is stopped, this resource loading has completed or the read has
  // timed out. Read() and SeekForward() cannot be called concurrently.
  size_t Read(uint8* buffer, size_t size);

  // Seek forward to |position| in bytes in the entire instance of the media
  // object, returns true if successful. If the seek operation cannot be
  // performed because it's seeking backward, the loader has been stopped,
  // the seek |position| exceed bufferable range or the seek operation has
  // timed out, returns false.
  // There cannot be SeekForward() while another thread is calling Read().
  bool SeekForward(int64 position);

  // Returns the position in bytes that this loader is downloading from.
  int64 GetOffset();

  // Gets and sets the buffering limit of this loader.
  int64 GetBufferLimit();
  void SetBufferLimit(size_t buffe_limit);

  // Gets and sets the timeout for the synchronous operations.
  size_t GetTimeout();
  void SetTimeout(size_t milliseconds);

  // Gets the content length in bytes of the instance after this loader has been
  // started.
  int64 content_length() { return content_length_; }

  /////////////////////////////////////////////////////////////////////////////
  // webkit_glue::ResourceLoaderBridge::Peer implementations.
  virtual void OnUploadProgress(uint64 position, uint64 size) {}
  virtual void OnReceivedRedirect(const GURL& new_url);
  virtual void OnReceivedResponse(
      const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
      bool content_filtered);
  virtual void OnReceivedData(const char* data, int len);
  virtual void OnCompletedRequest(const URLRequestStatus& status,
      const std::string& security_info);
  std::string GetURLForDebugging() { return url_.spec(); }

 private:
  // Append buffer to the queue of buffers.
  void AppendToBuffer(const uint8* buffer, size_t size);
  void SignalComplete();
  bool ShouldEnableDefer();
  bool ShouldDisableDefer();

  void OnStart();
  void OnDestroy();
  void OnDisableDeferLoading();
  void OnEnableDeferLoading();

  void InvokeAndResetStartCallback(int error);

  struct Buffer {
    Buffer(size_t len) : taken(0), size(len), data(new uint8[len]) { }

    // The amount of buffer in bytes consumed in this buffer starting from
    // index 0.
    size_t taken;
    size_t size;
    scoped_array<uint8> data;
  };

  scoped_ptr<net::CompletionCallback> start_callback_;
  scoped_ptr<webkit_glue::ResourceLoaderBridge> bridge_;
  int64 offset_;
  int64 content_length_;

  std::deque<Buffer*> buffers_;
  size_t buffered_bytes_;
  size_t buffer_limit_;
  base::WaitableEvent buffer_event_;

  bool deferred_;
  bool stopped_;
  bool completed_;
  bool range_requested_;
  bool async_start_;

  int32 routing_id_;
  GURL url_;
  int64 first_byte_position_;
  int64 last_byte_position_;

  MessageLoop* render_loop_;
  // A lock that protects usage of the following members:
  // - buffers_
  // - buffered_bytes_
  // - buffered_limit_
  // - deferred_
  // - stopped_
  // - completed_
  Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(BufferedResourceLoader);
};

class BufferedDataSource : public media::DataSource {
 public:
  // Methods called from pipeline thread
  // Static methods for creating this class.
  static media::FilterFactory* CreateFactory(int32 routing_id) {
    return new media::FilterFactoryImpl1<BufferedDataSource, int32>(routing_id);
  }
  virtual bool Initialize(const std::string& url);

  // media::MediaFilter implementation.
  virtual void Stop();

  // media::DataSource implementation.
  // Called from demuxer thread.
  virtual size_t Read(uint8* data, size_t size);
  virtual bool GetPosition(int64* position_out);
  virtual bool SetPosition(int64 position);
  virtual bool GetSize(int64* size_out);
  virtual bool IsSeekable();

  const media::MediaFormat& media_format();

 private:
  friend class media::FilterFactoryImpl1<BufferedDataSource, int32>;
  // Call to filter host to trigger an error, be sure not to call this method
  // while the lock is acquired.
  void HandleError(media::PipelineError error);

  // Callback method from BufferedResourceLoader for the initial url request.
  // |error| is net::OK if the request has started successfully or |error| is
  // a code representing the actual network error.
  void InitialRequestStarted(int error);
  void OnInitialRequestStarted(int error);

  explicit BufferedDataSource(int32 routing_id);
  virtual ~BufferedDataSource();

  media::MediaFormat media_format_;
  GURL url_;

  int32 routing_id_;

  // A common lock for protecting members accessed by multiple threads.
  Lock lock_;
  bool stopped_;

  // Members used for reading.
  int64 position_;
  // Members for total bytes of the requested object.
  int64 total_bytes_;

  // Members related to resource loading with RenderView.
  scoped_refptr<BufferedResourceLoader> buffered_resource_loader_;

  // The message loop of the pipeline thread.
  MessageLoop* pipeline_loop_;

  DISALLOW_COPY_AND_ASSIGN(BufferedDataSource);
};

#endif  // CHROME_RENDERER_MEDIA_BUFFERED_DATA_SOURCE_H_
