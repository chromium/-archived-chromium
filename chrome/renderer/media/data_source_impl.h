// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// A Chrome specific data source for video stack pipeline. The actual resource
// loading would happen in the browser process. This class is given a file
// handle and will ask for progress of downloading from RenderView which
// delegates requests to browser process through IPC. Asynchronous IO will be
// performed on the file handle.
//
// This class is access by 4 different threads during it's lifetime, namely:
// 1. Render thread
//    Thread that runs WebKit objects and construct this class. Updates about
//    progress for resource loading also happens in this thread.
// 2. Pipeline thread
//    Closing thread of the video stack pipeline, it initialized this class
//    and performs stopping in an orderly fashion.
// 3. Demuxer thread
//    Thread created by the pipeline and ask for data from this class.
//    media::DataSource methods are called from this thread.
// 4. IO thread
//    Performs file stream construction and callback of read completion also
//    comes from this thread.
//
// Methods in the class categorized by the thread(s) they are running on:
//
// Render thread
//   +-- DataSourceImpl()
//   |     Perform construction of this class.
//   |-- static CreateFactory()
//   |     Called during construction of this class.
//   |-- OnInitialize()
//   |     Task posted by Initialize() to kick start resource loading.
//   |-- OnCancel()
//   |     Cancel the resource loading.
//   |-- OnDownloadProgress()
//   |     Receives download progress information for the response data file.
//   |-- OnUploadProgress()
//   |-- OnReceivedRedirect()
//   |-- OnReceivedResponse()
//   |-- OnReceivedData()
//   |-- OnCompletedRequest()
//   |-- GetURLForDebugging()
//   \-- ReleaseRendererResources();
//
// Pipeline thread
//   +-- Initialize()
//   |     Performs initialization of data source in the pipeline.
//   \-- Stop()
//         Cease all activities during pipeline tear down.
//
// Demuxer thread
//   +-- Read()
//   |     Called to read from the media file.
//   |-- GetPosition()
//   |     Called to obtain current position in the file.
//   |-- SetPosition()
//   |     Performs a seek operation.
//   \-- GetSize()
//         Retrieve the size of the resource.
//
// IO thread
//   +-- OnCreateFileStream()
//   |     Callback for construction of net::FileStream in an IO message loop.
//   |-- OnReadFileStream()
//   |     Actual read operation on FileStream performs here.
//   |-- OnCloseFileStream()
//   |     Closing of FileSteram happens in this method.
//   |-- OnSeekFileStream()
//   |     Actual seek operation happens here.
//   \-- OnDidFileStreamRead()
//         Callback for asynchronous file read completion.


#ifndef CHROME_RENDERER_MEDIA_DATA_SOURCE_IMPL_H_
#define CHROME_RENDERER_MEDIA_DATA_SOURCE_IMPL_H_

#include <string>

#include "base/lock.h"
#include "base/platform_file.h"
#include "base/scoped_ptr.h"
#include "base/waitable_event.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/base/media_format.h"
#include "net/base/completion_callback.h"
#include "net/base/file_stream.h"
#include "webkit/glue/resource_loader_bridge.h"

class WebMediaPlayerDelegateImpl;

class DataSourceImpl : public media::DataSource,
                       public webkit_glue::ResourceLoaderBridge::Peer {
 public:
  // Methods called from render thread ----------------------------------------
  // Static methods for creating this class.
  static media::FilterFactory* CreateFactory(
    WebMediaPlayerDelegateImpl* delegate) {
      return new media::FilterFactoryImpl1<DataSourceImpl,
        WebMediaPlayerDelegateImpl*>(delegate);
  }

  // webkit_glue::ResourceLoaderBridge::Peer implementations, receive events
  // for resource loading.
  virtual void OnDownloadProgress(uint64 position, uint64 size);
  virtual void OnUploadProgress(uint64 position, uint64 size);
  virtual void OnReceivedRedirect(const GURL& new_url);
  virtual void OnReceivedResponse(
      const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
      bool content_filtered);
  virtual void OnReceivedData(const char* data, int len);
  virtual void OnCompletedRequest(const URLRequestStatus& status,
                                  const std::string& security_info);
  virtual std::string GetURLForDebugging();

  // Release all resources associated with RenderView, particularly
  // ResourceLoaderBridge created by ResourceDispatcher. This method should only
  // be executed on render thread, we have this method standalone and public
  // here because WebMediaPlayerDelegateImpl will be calling this method when
  // render thread is being destroyed, and in that case we can't post tasks to
  // render thread from pipeline thread anymore so we have to release resources
  // manually from WebMediaPlayerDelegateImpl and we will not do the same thing
  // in Stop().
  void ReleaseRendererResources();

  // Methods called from pipeline thread --------------------------------------
  virtual bool Initialize(const std::string& url);
  // media::MediaFilter implementation.
  virtual void Stop();

  // Methods called from demuxer thread ---------------------------------------
  // media::DataSource implementation.
  virtual size_t Read(uint8* data, size_t size);
  virtual bool GetPosition(int64* position_out);
  virtual bool SetPosition(int64 position);
  virtual bool GetSize(int64* size_out);

  const media::MediaFormat& media_format();

 private:
  friend class media::FilterFactoryImpl1<DataSourceImpl,
                                         WebMediaPlayerDelegateImpl*>;

  // Methods called from render thread ----------------------------------------
  explicit DataSourceImpl(WebMediaPlayerDelegateImpl* delegate);
  virtual ~DataSourceImpl();

  // Tasks to be posted on render thread.
  void OnInitialize(std::string uri);
  void OnCancel();

  // Methods called from IO thread --------------------------------------------
  // Handlers for file reading.
  void OnCreateFileStream(base::PlatformFile file);
  void OnReadFileStream(uint8* data, size_t size);
  void OnCloseFileStream();
  void OnSeekFileStream(net::Whence whence, int64 position);
  void OnDidFileStreamRead(int size);

  media::MediaFormat media_format_;

  // Pointer to the delegate which provides access to RenderView, this is set
  // in construction and can be accessed in all threads safely.
  WebMediaPlayerDelegateImpl* delegate_;

  // Message loop of render thread.
  MessageLoop* render_loop_;

  // A common lock for protecting members accessed by multiple threads.
  Lock lock_;

  // A flag that indicates whether this object has been called to stop.
  bool stopped_;

  // URI to the resource being downloaded.
  std::string uri_;

  // Members for keeping track of downloading progress.
  base::WaitableEvent download_event_;
  int64 downloaded_bytes_;
  int64 total_bytes_;
  bool total_bytes_known_;

  // Members related to resource loading with RenderView.
  webkit_glue::ResourceLoaderBridge* resource_loader_bridge_;
  base::WaitableEvent resource_release_event_;

  // Members used for reading.
  base::WaitableEvent read_event_;
  net::CompletionCallbackImpl<DataSourceImpl> read_callback_;
  scoped_ptr<net::FileStream> stream_;
  size_t last_read_size_;
  int64 position_;
  MessageLoop* io_loop_;

  // Events for other operations on stream_.
  base::WaitableEvent close_event_;
  base::WaitableEvent seek_event_;

  DISALLOW_COPY_AND_ASSIGN(DataSourceImpl);
};

#endif  // CHROME_RENDERER_MEDIA_DATA_SOURCE_IMPL_H_
