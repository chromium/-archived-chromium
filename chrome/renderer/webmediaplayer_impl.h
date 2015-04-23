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
// Properties that are shared by main thread and media threads:
//   CancelableTaskList tasks_;
//   ^--- This property is shared for keeping records of the tasks posted to
//        make sure there will be only one task for each task type that can
//        exist in the main thread.
//
// Methods that are accessed in media threads:
//   SetAudioRenderer()
//   ^--- Called during the initialization of the pipeline, essentially from the
//        the pipeline thread.
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

#ifndef CHROME_RENDERER_WEBMEDIAPLAYER_IMPL_H_
#define CHROME_RENDERER_WEBMEDIAPLAYER_IMPL_H_

#include <vector>

#include "base/gfx/platform_canvas.h"
#include "base/lock.h"
#include "base/message_loop.h"
#include "media/base/filters.h"
#include "media/base/pipeline_impl.h"
#include "webkit/api/public/WebMediaPlayer.h"
#include "webkit/api/public/WebMediaPlayerClient.h"

class AudioRendererImpl;
class DataSourceImpl;
class GURL;
class RenderView;
class VideoRendererImpl;

namespace media {
class FilterFactoryCollection;
}

// This typedef is used for WebMediaPlayerImpl::PostTask() and
// NotifyWebMediaPlayerTask in the source file.
typedef void (WebKit::WebMediaPlayerClient::*WebMediaPlayerClientMethod)();

class WebMediaPlayerImpl : public WebKit::WebMediaPlayer,
                           public MessageLoop::DestructionObserver {
 public:
  WebMediaPlayerImpl(RenderView* view, WebKit::WebMediaPlayerClient* client);
  virtual ~WebMediaPlayerImpl();

  virtual void load(const WebKit::WebURL& url);
  virtual void cancelLoad();

  // Playback controls.
  virtual void play();
  virtual void pause();
  virtual void stop();
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

  // Notification from |pipeline_| when initialization has finished.
  void OnPipelineInitialize(bool successful);

  // Notification from |pipeline_| when a seek has finished.
  void OnPipelineSeek(bool successful);

  // Called from tasks posted to |main_loop_| from this object to remove
  // reference of them.
  void DidTask(CancelableTask* task);

  // Public methods to be called from renderers and data source so that
  // WebMediaPlayerImpl has references to them.
  void SetVideoRenderer(VideoRendererImpl* video_renderer);

  // Called from VideoRenderer to fire a repaint task to |main_loop_|.
  void PostRepaintTask();

  // Inline getters.
  WebKit::WebMediaPlayerClient* client() { return client_; }
  RenderView* view() { return view_; }

 private:
  // Methods for posting tasks and cancelling tasks. This method may lives in
  // the main thread or the media threads.
  void PostTask(int index, WebMediaPlayerClientMethod method);

  // Cancel all tasks currently live in |main_loop_|.
  void CancelAllTasks();

  // Indexes for tasks.
  enum {
    kRepaintTaskIndex = 0,
    kReadyStateTaskIndex,
    kNetworkStateTaskIndex,
    kTimeChangedTaskIndex,
    kLastTaskIndex
  };

  // TODO(hclam): get rid of these members and read from the pipeline directly.
  WebKit::WebMediaPlayer::NetworkState network_state_;
  WebKit::WebMediaPlayer::ReadyState ready_state_;

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
  scoped_refptr<VideoRendererImpl> video_renderer_;

  WebKit::WebMediaPlayerClient* client_;
  RenderView* view_;

  // List of tasks for holding pointers to all tasks currently in the
  // |main_loop_|. |tasks_| can be access from main thread or the media threads
  // we need a lock for protecting it.
  Lock task_lock_;
  typedef std::vector<CancelableTask*> CancelableTaskList;
  CancelableTaskList tasks_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImpl);
};

#endif  // CHROME_RENDERER_WEBMEDIAPLAYER_IMPL_H_
