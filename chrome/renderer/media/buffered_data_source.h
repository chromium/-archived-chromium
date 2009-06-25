// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_BUFFERED_DATA_SOURCE_H_
#define CHROME_RENDERER_MEDIA_BUFFERED_DATA_SOURCE_H_

#include <string>

#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "base/condition_variable.h"
#include "googleurl/src/gurl.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/base/media_format.h"
#include "media/base/pipeline.h"
#include "media/base/seekable_buffer.h"
#include "net/base/completion_callback.h"
#include "net/base/file_stream.h"
#include "webkit/glue/media/media_resource_loader_bridge_factory.h"

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
  // |message_loop| - The message loop this resource loader should run on.
  // |bridge_factory| - Factory to create a ResourceLoaderBridge.
  // |url| - URL for the resource to be loaded.
  // |first_byte_position| - First byte to start loading from, -1 for not
  // specified.
  // |last_byte_position| - Last byte to be loaded, -1 for not specified.
  BufferedResourceLoader(
      MessageLoop* message_loop,
      webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory,
      const GURL& url,
      int64 first_byte_position,
      int64 last_byte_position);
  virtual ~BufferedResourceLoader();

  // Start the resource loading with the specified URL and range.
  // This method call can operate in two modes, synchronous and asynchronous.
  // If |start_callback| is NULL, this method operates in synchronous mode. It
  // has the following return values in this mode:
  // net::OK
  //   The load has started successfully.
  // net::ERR_FAILED
  //   The load cannot be started.
  // net::ERR_TIMEOUT
  //   The start operation took too long and has timed out.
  //
  // If |start_callback| is not NULL, this method operates in asynchronous mode
  // and it returns net::ERR_IO_PENDING if the request is going to start.
  // Once there's a response from the server, success or fail |start_callback|
  // is called with the result.
  // Note that |start_callback| is called within a lock to prevent invoking an
  // invalid callback method while this object is called to stop.
  int Start(net::CompletionCallback* callback);

  // Stops this loader. Wakes up all synchronous actions.
  void Stop();

  // Reads the specified |read_size| from |position| into |buffer| and returns
  // number of bytes read into variable |bytes_read|. This method call is
  // synchronous, when it returns it may not be able to fulfill what has been
  // requested.
  //
  // Produces the following return values:
  // net::OK
  //   The read operation was successful. |bytes_read| may be less than
  //   |read_size| even if the request was successful, which happens when the
  //   request has completed normally but there wasn't enough bytes to serve
  //   the request.
  // net::ERR_TIMEOUT
  //   The read operation has timed out and we didn't get enough bytes for what
  //   we requested.
  // net::ERR_FAILED
  //   The read operation has failed because the request completed abnormally
  //   or |position| is too far from the current position of this loader that
  //   we cannot serve.
  // net::ERR_ABORTED
  //   The loader has been stopped.
  int Read(uint8* buffer, size_t* bytes_read, int64 position, size_t read_size);

  // Returns the position in bytes that this loader is downloading from.
  int64 GetOffset();

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
  // Reads the specified |read_size| into |buffer| and returns number of bytes
  // read into variable |bytes_read|. This method call is synchronous, when it
  // returns it may not be able to fulfill what has been requested.
  //
  // Produces the following return values:
  // net::OK
  //   The read operation was successful. |bytes_read| may be less than
  //   |read_size| even if the request was successful, this happens when the
  //   request has completed normally but there isn't enough bytes to serve
  //   the request.
  // net::ERR_TIMEOUT
  //   The read operation has timed out and we didn't get enough bytes for what
  //   we have requested.
  // net::ERR_FAILED
  //   The read operation has failed because the request was completed
  //   abnormally.
  // net::ERR_ABORTED
  //   The loader has been stopped.
  int ReadInternal(uint8* buffer, size_t* bytes_read, size_t read_size);

  // Seek to |position| in bytes in the entire instance of the media
  // object. This method call is synchronous. It has the following return
  // values:
  //
  // net::OK
  //   The seek operation was successful.
  // net::ERR_FAILED
  //   The desired |position| is too far from the current offset, and we decided
  //   not to serve the |position| either because we don't want to wait too long
  //   for data to be downloaded or |position| was too far in the past that we
  //   don't have the data buffered.
  //   We may get this error if the request has completed abnormally.
  // net::ERR_TIMEOUT
  //   The seek operation took too long and timed out.
  // net::ERR_ABORTED
  //   The loader has been stopped.
  int SeekInternal(int64 position);

  // Append buffer to the queue of buffers.
  void AppendToBuffer(const uint8* buffer, size_t size);

  void SignalComplete(int error);
  bool ShouldEnableDefer();
  bool ShouldDisableDefer();

  void OnStart();
  void OnDestroy();
  void OnDisableDeferLoading();
  void OnEnableDeferLoading();

  void InvokeAndResetStartCallback(int error);

  scoped_ptr<net::CompletionCallback> start_callback_;
  scoped_ptr<webkit_glue::ResourceLoaderBridge> bridge_;
  int64 offset_;
  int64 content_length_;
  int completion_error_;

  scoped_ptr<media::SeekableBuffer> buffer_;

  bool deferred_;
  bool stopped_;
  bool completed_;
  bool range_requested_;
  bool async_start_;

  webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory_;
  GURL url_;
  int64 first_byte_position_;
  int64 last_byte_position_;

  MessageLoop* render_loop_;
  // A lock that protects usage of the following members:
  // - buffer_
  // - deferred_
  // - stopped_
  // - completed_
  Lock lock_;
  ConditionVariable buffer_available_;

  DISALLOW_COPY_AND_ASSIGN(BufferedResourceLoader);
};

class BufferedDataSource : public media::DataSource {
 public:
  // Methods called from pipeline thread
  // Static methods for creating this class.
  static media::FilterFactory* CreateFactory(
      MessageLoop* message_loop,
      webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory) {
    return new media::FilterFactoryImpl2<
        BufferedDataSource,
        MessageLoop*,
        webkit_glue::MediaResourceLoaderBridgeFactory*>(
        message_loop, bridge_factory);
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
  friend class media::FilterFactoryImpl2<
      BufferedDataSource,
      MessageLoop*,
      webkit_glue::MediaResourceLoaderBridgeFactory*>;
  // Call to filter host to trigger an error, be sure not to call this method
  // while the lock is acquired.
  void HandleError(media::PipelineError error);

  // Callback method from BufferedResourceLoader for the initial url request.
  // |error| is net::OK if the request has started successfully or |error| is
  // a code representing the actual network error.
  void InitialRequestStarted(int error);
  void OnInitialRequestStarted(int error);

  explicit BufferedDataSource(
      MessageLoop* render_loop,
      webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory);
  virtual ~BufferedDataSource();

  media::MediaFormat media_format_;
  GURL url_;

  // A common lock for protecting members accessed by multiple threads.
  Lock lock_;
  bool stopped_;

  // Members used for reading.
  int64 position_;
  // Members for total bytes of the requested object.
  int64 total_bytes_;

  // Members related to resource loading with RenderView.
  scoped_ptr<webkit_glue::MediaResourceLoaderBridgeFactory> bridge_factory_;
  scoped_refptr<BufferedResourceLoader> buffered_resource_loader_;

  // The message loop of the render thread.
  MessageLoop* render_loop_;

  // The message loop of the pipeline thread.
  MessageLoop* pipeline_loop_;

  DISALLOW_COPY_AND_ASSIGN(BufferedDataSource);
};

#endif  // CHROME_RENDERER_MEDIA_BUFFERED_DATA_SOURCE_H_
