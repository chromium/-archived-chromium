// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// Delegate calls from WebCore::MediaPlayerPrivate to google's video player.
// It contains PipelineImpl which is the actual media player pipeline, it glues
// the media player pipeline, data source, audio renderer and renderer.
// PipelineImpl would creates multiple threads and access some public methods
// of this class, so we need to be extra careful about concurrent access of
// methods and members.
//
// Properties that are shared by main thread and media threads:
//   CancelableTaskList tasks_;
//   ^--- This property is shared for keeping records of the tasks posted to
//        make sure there will be only one task for each task type that can
//        exist in the main thread.
//
// Methods that are accessed in media threads:
//   SetVideoRenderer()
//   ^--- Called during the initialization of the pipeline, essentially from the
//        the pipeline thread.
//   PostRepaintTask()
//   ^--- Called from the video renderer thread to notify a video frame has
//        been prepared.
//   PostTask()
//   ^--- A method that helps posting tasks to the main thread, it is
//        accessed from main thread and media threads, it access the |tasks_|
//        internally. Needs locking inside to avoid concurrent access to
//        |tasks_|.
//
//
// Other issues:
// During tear down of the whole browser or a tab, the DOM tree may not be
// destructed nicely, and there will be some dangling media threads trying to
// the main thread, so we need this class to listen to destruction event of the
// main thread and cleanup the media threads when the even is received. Also
// at destruction of this class we will need to unhook it from destruction event
// list of the main thread.

#ifndef CHROME_RENDERER_WEBMEDIAPLAYER_DELEGATE_IMPL_H_
#define CHROME_RENDERER_WEBMEDIAPLAYER_DELEGATE_IMPL_H_

#include <vector>

#include "base/lock.h"
#include "base/message_loop.h"
#include "media/base/filters.h"
#include "media/base/pipeline_impl.h"
#include "webkit/glue/webmediaplayer_delegate.h"

class RenderView;
class VideoRendererImpl;

namespace media {
class FilterFactoryCollection;
}

// This typedef is used for WebMediaPlayerDelegateImpl::PostTask() and
// NotifyWebMediaPlayerTask in the source file.
typedef void (webkit_glue::WebMediaPlayer::*WebMediaPlayerMethod)();

class WebMediaPlayerDelegateImpl : public webkit_glue::WebMediaPlayerDelegate,
                                   public MessageLoop::DestructionObserver {
 public:
  explicit WebMediaPlayerDelegateImpl(RenderView* view);
  virtual ~WebMediaPlayerDelegateImpl();

  // Implementations of WebMediaPlayerDelegate, theses following methods are
  // called from the WebKit, essentially lives inside the main thread.
  virtual void Initialize(webkit_glue::WebMediaPlayer* media_player);

  virtual void Load(const GURL& url);
  virtual void CancelLoad();

  // Playback controls.
  virtual void Play();
  virtual void Pause();
  virtual void Stop();
  virtual void Seek(float time);
  virtual void SetEndTime(float time);
  virtual void SetPlaybackRate(float rate);
  virtual void SetVolume(float volume);
  virtual void SetVisible(bool visible);
  virtual bool IsTotalBytesKnown();

  // Methods for painting.
  virtual void SetRect(const gfx::Rect& rect);

  virtual void Paint(skia::PlatformCanvas *canvas, const gfx::Rect& rect);

  // True if a video is loaded.
  virtual bool IsVideo() const;

  // Dimension of the video.
  virtual size_t GetWidth() const;
  virtual size_t GetHeight() const;

  // Getters of playback state.
  virtual bool IsPaused() const;
  virtual bool IsSeeking() const;
  virtual float GetDuration() const;
  virtual float GetCurrentTime() const;
  virtual float GetPlayBackRate() const;
  virtual float GetVolume() const;
  virtual float GetMaxTimeBuffered() const;
  virtual float GetMaxTimeSeekable() const;

  // Get rate of loading the resource.
  virtual int32 GetDataRate() const;

  // Internal states of loading and network.
  // TODO(hclam): Ask the pipeline about the state rather than having reading
  // them from members which would cause race conditions.
  virtual webkit_glue::WebMediaPlayer::NetworkState GetNetworkState() const {
    return network_state_;
  }
  virtual webkit_glue::WebMediaPlayer::ReadyState GetReadyState() const {
    return ready_state_;
  }

  virtual int64 GetBytesLoaded() const;
  virtual int64 GetTotalBytes() const;

  // As we are closing the tab or even the browser, main_loop_ is destroyed
  // even before this object gets destructed, so we need to know when
  // main_loop_ is being destroyed and we can stop posting repaint task
  // to it.
  virtual void WillDestroyCurrentMessageLoop();

  // Callbacks.
  void DidInitializePipeline(bool successful);

  // Called from tasks posted to main_loop_ from this object to remove
  // reference of them.
  void DidTask(CancelableTask* task);

  // Public methods to be called from renderers and data source.
  void SetVideoRenderer(VideoRendererImpl* video_renderer);

  // Called from VideoRenderer to fire a repaint task to main_loop_.
  void PostRepaintTask();

  // Inline getters.
  webkit_glue::WebMediaPlayer* web_media_player() { return web_media_player_; }
  RenderView* view() { return view_; }

 private:
  // Methods for posting tasks and cancelling tasks. This method may lives in
  // the main thread or the media threads.
  void PostTask(int index, WebMediaPlayerMethod method);

  // Cancel all tasks currently lives in |main_loop_|.
  void CancelAllTasks();

  // Indexes for tasks.
  enum {
    kRepaintTaskIndex = 0,
    kReadyStateTaskIndex,
    kNetworkStateTaskIndex,
    kLastTaskIndex
  };

  // TODO(hclam): get rid of these members and read from the pipeline directly.
  webkit_glue::WebMediaPlayer::NetworkState network_state_;
  webkit_glue::WebMediaPlayer::ReadyState ready_state_;

  // Message loops for posting tasks between Chrome's main thread. Also used
  // for DCHECKs so methods calls won't execute in the wrong thread.
  MessageLoop* main_loop_;

  // A collection of factories for creating filters.
  scoped_refptr<media::FilterFactoryCollection> filter_factory_;

  // The actual pipeline. We do it a composition here because we expect to have
  // the same lifetime as the pipeline.
  media::PipelineImpl pipeline_;

  // We have the interface to VideoRenderer to delegate paint messages to it
  // from WebKit.
  VideoRendererImpl* video_renderer_;

  webkit_glue::WebMediaPlayer* web_media_player_;
  RenderView* view_;

  // List of tasks for holding pointers to all tasks currently in the
  // |main_loop_|. |tasks_| can be access from main thread or the media threads
  // we need a lock for protecting it.
  Lock task_lock_;
  typedef std::vector<CancelableTask*> CancelableTaskList;
  CancelableTaskList tasks_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerDelegateImpl);
};

#endif  // ifndef CHROME_RENDERER_WEBMEDIAPLAYER_DELEGATE_IMPL_H_
