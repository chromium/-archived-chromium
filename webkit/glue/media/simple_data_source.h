// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An extremely simple implementation of DataSource that downloads the entire
// media resource into memory before signaling that initialization has finished.
// Primarily used to test <audio> and <video> with buffering/caching removed
// from the equation.

#ifndef WEBKIT_GLUE_MEDIA_SIMPLE_DATA_SOURCE_H_
#define WEBKIT_GLUE_MEDIA_SIMPLE_DATA_SOURCE_H_

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "webkit/glue/media/media_resource_loader_bridge_factory.h"

class MessageLoop;
class WebMediaPlayerDelegateImpl;

namespace webkit_glue {

class SimpleDataSource : public media::DataSource,
                         public webkit_glue::ResourceLoaderBridge::Peer {
 public:
  static media::FilterFactory* CreateFactory(
      MessageLoop* message_loop,
      webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory) {
    return new media::FilterFactoryImpl2<
        SimpleDataSource,
        MessageLoop*,
        webkit_glue::MediaResourceLoaderBridgeFactory*>(message_loop,
                                                        bridge_factory);
  }

  // MediaFilter implementation.
  virtual void Stop();

  // DataSource implementation.
  virtual bool Initialize(const std::string& url);
  virtual const media::MediaFormat& media_format();
  virtual size_t Read(uint8* data, size_t size);
  virtual bool GetPosition(int64* position_out);
  virtual bool SetPosition(int64 position);
  virtual bool GetSize(int64* size_out);
  virtual bool IsSeekable();

  // webkit_glue::ResourceLoaderBridge::Peer implementation.
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

 private:
  friend class media::FilterFactoryImpl2<
      SimpleDataSource,
      MessageLoop*,
      webkit_glue::MediaResourceLoaderBridgeFactory*>;
  SimpleDataSource(
      MessageLoop* render_loop,
      webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory);
  virtual ~SimpleDataSource();

  // Updates |url_| and |media_format_| with the given URL.
  void SetURL(const GURL& url);

  // Creates and starts the resource loading on the render thread.
  void StartTask();

  // Cancels and deletes the resource loading on the render thread.
  void CancelTask();

  // Primarily used for asserting the bridge is loading on the render thread.
  MessageLoop* render_loop_;

  // Factory to create a bridge.
  scoped_ptr<webkit_glue::MediaResourceLoaderBridgeFactory> bridge_factory_;

  // Bridge used to load the media resource.
  scoped_ptr<webkit_glue::ResourceLoaderBridge> bridge_;

  media::MediaFormat media_format_;
  GURL url_;
  std::string data_;
  int64 size_;
  int64 position_;

  // Simple state tracking variable.
  enum State {
    UNINITIALIZED,
    INITIALIZING,
    INITIALIZED,
    STOPPED,
  };
  State state_;

  // Used for accessing |state_|.
  Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(SimpleDataSource);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_MEDIA_SIMPLE_DATA_SOURCE_H_
