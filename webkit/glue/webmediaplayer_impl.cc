// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "googleurl/src/gurl.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/null_audio_renderer.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/api/public/WebSize.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/glue/media/simple_data_source.h"
#include "webkit/glue/media/video_renderer_impl.h"
#include "webkit/glue/webmediaplayer_impl.h"

using WebKit::WebCanvas;
using WebKit::WebRect;
using WebKit::WebSize;

namespace webkit_glue {

/////////////////////////////////////////////////////////////////////////////
// Task to be posted on main thread that fire WebMediaPlayer methods.

class NotifyWebMediaPlayerTask : public CancelableTask {
 public:
  NotifyWebMediaPlayerTask(WebMediaPlayerImpl* media_player,
                           WebMediaPlayerClientMethod method)
      : media_player_(media_player),
        method_(method) {}

  virtual void Run() {
    if (media_player_) {
      (media_player_->client()->*(method_))();
      media_player_->DidTask(this);
    }
  }

  virtual void Cancel() {
    media_player_ = NULL;
  }

 private:
  WebMediaPlayerImpl* media_player_;
  WebMediaPlayerClientMethod method_;

  DISALLOW_COPY_AND_ASSIGN(NotifyWebMediaPlayerTask);
};

/////////////////////////////////////////////////////////////////////////////
// WebMediaPlayerImpl implementation

WebMediaPlayerImpl::WebMediaPlayerImpl(WebKit::WebMediaPlayerClient* client,
                                       media::FilterFactoryCollection* factory)
    : network_state_(WebKit::WebMediaPlayer::Empty),
      ready_state_(WebKit::WebMediaPlayer::HaveNothing),
      main_loop_(NULL),
      filter_factory_(factory),
      video_renderer_(NULL),
      client_(client),
      tasks_(kLastTaskIndex) {
  // Add in the default filter factories.
  filter_factory_->AddFactory(media::FFmpegDemuxer::CreateFilterFactory());
  filter_factory_->AddFactory(media::FFmpegAudioDecoder::CreateFactory());
  filter_factory_->AddFactory(media::FFmpegVideoDecoder::CreateFactory());
  filter_factory_->AddFactory(media::NullAudioRenderer::CreateFilterFactory());
  filter_factory_->AddFactory(VideoRendererImpl::CreateFactory(this));
  // TODO(hclam): Provide a valid routing id to simple data source.
  filter_factory_->AddFactory(
      SimpleDataSource::CreateFactory(MessageLoop::current(), 0));

  DCHECK(client_);

  // Saves the current message loop.
  DCHECK(!main_loop_);
  main_loop_ = MessageLoop::current();

  // Also we want to be notified of |main_loop_| destruction.
  main_loop_->AddDestructionObserver(this);
}

WebMediaPlayerImpl::~WebMediaPlayerImpl() {
  pipeline_.Stop();

  // Cancel all tasks posted on the |main_loop_|.
  CancelAllTasks();

  // Finally tell the |main_loop_| we don't want to be notified of destruction
  // event.
  if (main_loop_) {
    main_loop_->RemoveDestructionObserver(this);
  }
}

void WebMediaPlayerImpl::load(const WebKit::WebURL& url) {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // Initialize the pipeline
  pipeline_.Start(filter_factory_.get(), url.spec(),
      NewCallback(this, &WebMediaPlayerImpl::OnPipelineInitialize));
}

void WebMediaPlayerImpl::cancelLoad() {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // TODO(hclam): Calls to render_view_ to stop resource load
}

void WebMediaPlayerImpl::play() {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // TODO(hclam): We should restore the previous playback rate rather than
  // having it at 1.0.
  pipeline_.SetPlaybackRate(1.0f);
}

void WebMediaPlayerImpl::pause() {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  pipeline_.SetPlaybackRate(0.0f);
}

void WebMediaPlayerImpl::stop() {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // We can fire Stop() multiple times.
  pipeline_.Stop();
}

void WebMediaPlayerImpl::seek(float seconds) {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // Try to preserve as much accuracy as possible.
  float microseconds = seconds * base::Time::kMicrosecondsPerSecond;
  if (seconds != 0)
  pipeline_.Seek(
      base::TimeDelta::FromMicroseconds(static_cast<int64>(microseconds)),
      NewCallback(this, &WebMediaPlayerImpl::OnPipelineSeek));
}

void WebMediaPlayerImpl::setEndTime(float seconds) {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // TODO(hclam): add method call when it has been implemented.
  return;
}

void WebMediaPlayerImpl::setRate(float rate) {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  pipeline_.SetPlaybackRate(rate);
}

void WebMediaPlayerImpl::setVolume(float volume) {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  pipeline_.SetVolume(volume);
}

void WebMediaPlayerImpl::setVisible(bool visible) {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // TODO(hclam): add appropriate method call when pipeline has it implemented.
  return;
}

bool WebMediaPlayerImpl::setAutoBuffer(bool autoBuffer) {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return false;
}

bool WebMediaPlayerImpl::totalBytesKnown() {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return pipeline_.GetTotalBytes() != 0;
}

bool WebMediaPlayerImpl::hasVideo() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  size_t width, height;
  pipeline_.GetVideoSize(&width, &height);
  return width != 0 && height != 0;
}

WebKit::WebSize WebMediaPlayerImpl::naturalSize() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  size_t width, height;
  pipeline_.GetVideoSize(&width, &height);
  return WebKit::WebSize(width, height);
}

bool WebMediaPlayerImpl::paused() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return pipeline_.GetPlaybackRate() == 0.0f;
}

bool WebMediaPlayerImpl::seeking() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return tasks_[kTimeChangedTaskIndex] != NULL;
}

float WebMediaPlayerImpl::duration() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return static_cast<float>(pipeline_.GetDuration().InSecondsF());
}

float WebMediaPlayerImpl::currentTime() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return static_cast<float>(pipeline_.GetTime().InSecondsF());
}

int WebMediaPlayerImpl::dataRate() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // TODO(hclam): Add this method call if pipeline has it in the interface.
  return 0;
}

float WebMediaPlayerImpl::maxTimeBuffered() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return static_cast<float>(pipeline_.GetBufferedTime().InSecondsF());
}

float WebMediaPlayerImpl::maxTimeSeekable() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // TODO(scherkus): move this logic down into the pipeline.
  if (pipeline_.GetTotalBytes() == 0) {
    return 0.0f;
  }
  double total_bytes = static_cast<double>(pipeline_.GetTotalBytes());
  double buffered_bytes = static_cast<double>(pipeline_.GetBufferedBytes());
  double duration = static_cast<double>(pipeline_.GetDuration().InSecondsF());
  return static_cast<float>(duration * (buffered_bytes / total_bytes));
}

unsigned long long WebMediaPlayerImpl::bytesLoaded() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return pipeline_.GetBufferedBytes();
}

unsigned long long WebMediaPlayerImpl::totalBytes() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return pipeline_.GetTotalBytes();
}

void WebMediaPlayerImpl::setSize(const WebSize& size) {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  if (video_renderer_) {
    // TODO(scherkus): Change API to use SetSize().
    video_renderer_->SetRect(gfx::Rect(0, 0, size.width, size.height));
  }
}

void WebMediaPlayerImpl::paint(WebCanvas* canvas,
                               const WebRect& rect) {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  if (video_renderer_) {
    video_renderer_->Paint(canvas, rect);
  }
}

void WebMediaPlayerImpl::WillDestroyCurrentMessageLoop() {
  pipeline_.Stop();
}

void WebMediaPlayerImpl::OnPipelineInitialize(bool successful) {
  if (successful) {
    // Since we have initialized the pipeline, say we have everything.
    // TODO(hclam): change this to report the correct status.
    ready_state_ = WebKit::WebMediaPlayer::HaveEnoughData;
    network_state_ = WebKit::WebMediaPlayer::Loaded;
  } else {
    // TODO(hclam): should use pipeline_.GetError() to determine the state
    // properly and reports error using MediaError.
    ready_state_ = WebKit::WebMediaPlayer::HaveNothing;
    network_state_ = WebKit::WebMediaPlayer::NetworkError;
  }

  PostTask(kNetworkStateTaskIndex,
           &WebKit::WebMediaPlayerClient::networkStateChanged);
  PostTask(kReadyStateTaskIndex,
           &WebKit::WebMediaPlayerClient::readyStateChanged);
}

void WebMediaPlayerImpl::OnPipelineSeek(bool successful) {
  PostTask(kTimeChangedTaskIndex,
           &WebKit::WebMediaPlayerClient::timeChanged);
}

void WebMediaPlayerImpl::SetVideoRenderer(VideoRendererImpl* video_renderer) {
  video_renderer_ = video_renderer;
}

void WebMediaPlayerImpl::DidTask(CancelableTask* task) {
  AutoLock auto_lock(task_lock_);

  for (size_t i = 0; i < tasks_.size(); ++i) {
    if (tasks_[i] == task) {
      tasks_[i] = NULL;
      return;
    }
  }
  NOTREACHED();
}

void WebMediaPlayerImpl::CancelAllTasks() {
  AutoLock auto_lock(task_lock_);
  // Loop through the list of tasks and cancel tasks that are still alive.
  for (size_t i = 0; i < tasks_.size(); ++i) {
    if (tasks_[i])
      tasks_[i]->Cancel();
  }
}

void WebMediaPlayerImpl::PostTask(int index,
                                  WebMediaPlayerClientMethod method) {
  DCHECK(main_loop_);

  AutoLock auto_lock(task_lock_);
  if (!tasks_[index]) {
    CancelableTask* task = new NotifyWebMediaPlayerTask(this, method);
    tasks_[index] = task;
    main_loop_->PostTask(FROM_HERE, task);
  }
}

void WebMediaPlayerImpl::PostRepaintTask() {
  PostTask(kRepaintTaskIndex, &WebKit::WebMediaPlayerClient::repaint);
}

}  // namespace webkit_glue
