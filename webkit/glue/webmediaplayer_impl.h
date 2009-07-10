// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.
//
// Delegate calls from WebCore::MediaPlayerPrivate to Chrome's video player.
// It contains PipelineImpl which is the actual media player pipeline, it glues
// the media player pipeline, data source, audio renderer and renderer.
// PipelineImpl would creates multiple threads and access some public methods
// of this class, so we need to be extra careful about concurrent access of
// methods and members.
//
// WebMediaPlayerImpl works with multiple objects, the most important ones are:
//
// media::PipelineImpl
//   The media playback pipeline.
//
// VideoRendererImpl
//   Video renderer object.
//
// WebMediaPlayerImpl::Proxy
//   Proxies methods calls from the media pipeline to WebKit.
//
// WebKit::WebMediaPlayerClient
//   WebKit client of this media player object.
//
// The following diagram shows the relationship of these objects:
//   (note: ref-counted reference is marked by a "r".)
//
// WebMediaPlayerImpl ------> PipelineImpl
//    |            ^               | r
//    |            |               v
//    |            |        VideoRendererImpl
//    |            |          ^ r
//    |            |          |
//    |      r     |    r     |
//    .------>   Proxy  <-----.
//    |
//    |
//    v
// WebMediaPlayerClient
//
// Notice that Proxy and VideoRendererImpl are referencing each other. This
// interdependency has to be treated carefully.
//
// Other issues:
// During tear down of the whole browser or a tab, the DOM tree may not be
// destructed nicely, and there will be some dangling media threads trying to
// the main thread, so we need this class to listen to destruction event of the
// main thread and cleanup the media threads when the even is received. Also
// at destruction of this class we will need to unhook it from destruction event
// list of the main thread.

#ifndef WEBKIT_GLUE_WEBMEDIAPLAYER_IMPL_H_
#define WEBKIT_GLUE_WEBMEDIAPLAYER_IMPL_H_

#include <vector>

#include "base/gfx/platform_canvas.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/lock.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "media/base/filters.h"
#include "media/base/pipeline_impl.h"
#include "webkit/api/public/WebMediaPlayer.h"
#include "webkit/api/public/WebMediaPlayerClient.h"

class GURL;

namespace media {
class FilterFactoryCollection;
}

namespace webkit_glue {

class VideoRendererImpl;

class WebMediaPlayerImpl : public WebKit::WebMediaPlayer,
                           public MessageLoop::DestructionObserver {
 public:
  // A proxy class that dispatches method calls from the media pipeline to
  // WebKit. Since there are multiple threads in the media pipeline and there's
  // need for the media pipeline to call to WebKit, e.g. repaint requests,
  // initialization events, etc, we have this class to bridge all method calls
  // from the media pipeline on different threads and serialize these calls
  // on the render thread.
  // Because of the nature of this object that it works with different threads,
  // it is made ref-counted.
  class Proxy : public base::RefCountedThreadSafe<Proxy> {
   public:
    Proxy(MessageLoop* render_loop,
          WebMediaPlayerImpl* webmediaplayer);
    virtual ~Proxy();

    // Fire a repaint event to WebKit.
    void Repaint();

    // Report to WebKit that time has changed.
    void TimeChanged();

    // Report to WebKit that network state has changed.
    void NetworkStateChanged(WebKit::WebMediaPlayer::NetworkState state);

    // Report the WebKit that ready state has changed.
    void ReadyStateChanged(WebKit::WebMediaPlayer::ReadyState state);

    // Public methods to be called from video renderer.
    void SetVideoRenderer(VideoRendererImpl* video_renderer);

   private:
    friend class WebMediaPlayerImpl;

    // Invoke |webmediaplayer_| to perform a repaint.
    void RepaintTask();

    // Invoke |webmediaplayer_| to notify a time change event.
    void TimeChangedTask();

    // Saves the internal network state and notify WebKit to pick up the change.
    void NetworkStateChangedTask(WebKit::WebMediaPlayer::NetworkState state);

    // Saves the internal ready state and notify WebKit to pick the change.
    void ReadyStateChangedTask(WebKit::WebMediaPlayer::ReadyState state);

    void Paint(skia::PlatformCanvas* canvas, const gfx::Rect& dest_rect);

    void SetSize(const gfx::Rect& rect);

    // Detach from |webmediaplayer_|.
    void Detach();

    void PipelineInitializationCallback(bool success);

    void PipelineSeekCallback(bool success);

    // The render message loop where WebKit lives.
    MessageLoop* render_loop_;
    WebMediaPlayerImpl* webmediaplayer_;
    scoped_refptr<VideoRendererImpl> video_renderer_;

    Lock lock_;
    int outstanding_repaints_;
  };

  // Construct a WebMediaPlayerImpl with reference to the client, and media
  // filter factory collection. By providing the filter factory collection
  // the implementor can provide more specific media filters that does resource
  // loading and rendering. |factory| should contain filter factories for:
  // 1. Data source
  // 2. Audio renderer
  // 3. Video renderer (optional)
  //
  // There are some default filters provided by this method:
  // 1. FFmpeg demuxer
  // 2. FFmpeg audio decoder
  // 3. FFmpeg video decoder
  // 4. Video renderer
  // 5. Null audio renderer
  // The video renderer provided by this class is using the graphics context
  // provided by WebKit to perform renderering. The simple data source does
  // resource loading by loading the whole resource object into memory. Null
  // audio renderer is a fake audio device that plays silence. Provider of the
  // |factory| can override the default filters by adding extra filters to
  // |factory| before calling this method.
  WebMediaPlayerImpl(WebKit::WebMediaPlayerClient* client,
                     media::FilterFactoryCollection* factory);
  virtual ~WebMediaPlayerImpl();

  virtual void load(const WebKit::WebURL& url);
  virtual void cancelLoad();

  // Playback controls.
  virtual void play();
  virtual void pause();
  virtual void seek(float seconds);
  virtual void setEndTime(float seconds);
  virtual void setRate(float rate);
  virtual void setVolume(float volume);
  virtual void setVisible(bool visible);
  virtual bool setAutoBuffer(bool autoBuffer);
  virtual bool totalBytesKnown();
  virtual float maxTimeBuffered() const;
  virtual float maxTimeSeekable() const;

  // Methods for painting.
  virtual void setSize(const WebKit::WebSize& size);

  virtual void paint(WebKit::WebCanvas* canvas, const WebKit::WebRect& rect);

  // True if a video is loaded.
  virtual bool hasVideo() const;

  // Dimensions of the video.
  virtual WebKit::WebSize naturalSize() const;

  // Getters of playback state.
  virtual bool paused() const;
  virtual bool seeking() const;
  virtual float duration() const;
  virtual float currentTime() const;

  // Get rate of loading the resource.
  virtual int32 dataRate() const;

  // Internal states of loading and network.
  // TODO(hclam): Ask the pipeline about the state rather than having reading
  // them from members which would cause race conditions.
  virtual WebKit::WebMediaPlayer::NetworkState networkState() const {
    return network_state_;
  }
  virtual WebKit::WebMediaPlayer::ReadyState readyState() const {
    return ready_state_;
  }

  virtual unsigned long long bytesLoaded() const;
  virtual unsigned long long totalBytes() const;

  // As we are closing the tab or even the browser, |main_loop_| is destroyed
  // even before this object gets destructed, so we need to know when
  // |main_loop_| is being destroyed and we can stop posting repaint task
  // to it.
  virtual void WillDestroyCurrentMessageLoop();

  void Repaint();

  void TimeChanged();

  void SetNetworkState(WebKit::WebMediaPlayer::NetworkState state);

  void SetReadyState(WebKit::WebMediaPlayer::ReadyState state);

 private:
  // Destroy resources held.
  void Destroy();

  // Getter method to |client_|.
  WebKit::WebMediaPlayerClient* GetClient();

  // TODO(hclam): get rid of these members and read from the pipeline directly.
  WebKit::WebMediaPlayer::NetworkState network_state_;
  WebKit::WebMediaPlayer::ReadyState ready_state_;

  // Message loops for posting tasks between Chrome's main thread. Also used
  // for DCHECKs so methods calls won't execute in the wrong thread.
  MessageLoop* main_loop_;

  // A collection of factories for creating filters.
  scoped_refptr<media::FilterFactoryCollection> filter_factory_;

  // The actual pipeline and the thread it runs on.
  scoped_ptr<media::PipelineImpl> pipeline_;
  base::Thread pipeline_thread_;

  WebKit::WebMediaPlayerClient* client_;

  scoped_refptr<Proxy> proxy_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImpl);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBMEDIAPLAYER_IMPL_H_
