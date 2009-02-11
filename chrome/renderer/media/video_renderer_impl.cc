// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/media/video_renderer_impl.h"
#include "media/base/buffers.h"
#include "media/base/filter_host.h"
#include "media/base/pipeline.h"

using media::MediaFormat;
using media::VideoFrame;


// The amount of time allowed to pre-submit a frame.  If UpdateQueue is called
// with a time within this limit, it will skip to the next frame in the queue
// even though the value for time it is called with is not yet at the frame's
// timestamp.  Time is specified in microseconds.
static const int64 kFrameSkipAheadLimit = 5000;

// If there are no frames in the queue, then this value is used for the
// amount of time to sleep until the next time update callback.  Time is
// specified in microseconds.
static const int64 kSleepIfNoFrame = 15000;

// Number of reads to have pending.
// TODO(ralph): Re-examine the pull model -- perhaps I was wrong.  I think
// that perhaps the demuxer is the point where pull becomes push.
static const size_t kDefaultNumberOfFrames = 4;

// Value used for the current_frame_timestamp_ to indicate that the
// |current_frame_| member should be treated as invalid.
static const int64 kNoCurrentFrame = -1;

//------------------------------------------------------------------------------

VideoRendererImpl::VideoRendererImpl(WebMediaPlayerDelegateImpl* delegate)
    : delegate_(delegate),
      submit_reads_task_(NULL),
      number_of_reads_needed_(kDefaultNumberOfFrames),
      current_frame_timestamp_(
          base::TimeDelta::FromMicroseconds(kNoCurrentFrame)),
      preroll_complete_(false) {
}

VideoRendererImpl::~VideoRendererImpl() {
  Stop();
}

// static
bool VideoRendererImpl::IsMediaFormatSupported(
    const media::MediaFormat* media_format) {
  int width;
  int height;
  return ParseMediaFormat(media_format, &width, &height);
}

// static
bool VideoRendererImpl::ParseMediaFormat(const media::MediaFormat* media_format,
                                         int* width_out,
                                         int* height_out) {
  DCHECK(media_format && width_out && height_out);
  std::string mime_type;
  return (media_format->GetAsString(MediaFormat::kMimeType, &mime_type) &&
          mime_type.compare(media::mime_type::kUncompressedVideo) == 0 &&
          media_format->GetAsInteger(MediaFormat::kWidth, width_out) &&
          media_format->GetAsInteger(MediaFormat::kHeight, height_out));
}

void VideoRendererImpl::Stop() {
  AutoLock auto_lock(lock_);
  DiscardAllFrames();
  if (submit_reads_task_) {
    // The task is owned by the message loop, so we don't delete it here.  We
    // know the task won't call us because we canceled it, and we know we are
    // on the pipeline thread, since we're in the filer's Stop method, so there
    // is no threading problem.  Just let the task be run by the message loop
    // and then be killed
    submit_reads_task_->Cancel();
    submit_reads_task_ = NULL;
  }
  delegate_ = NULL;  // This indicates we're no longer running
  decoder_ = NULL;   // Release reference to the decoder
}

bool VideoRendererImpl::Initialize(media::VideoDecoder* decoder) {
  int width;
  int height;
  if (!ParseMediaFormat(decoder_->GetMediaFormat(), &width, &height)) {
    return false;
  }
  current_frame_.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  if (!current_frame_.allocPixels(NULL, NULL)) {
    NOTREACHED();
    return false;
  }
  rect_.SetRect(0, 0, width, height);
  delegate_->SetVideoRenderer(this);
  host_->SetVideoSize(width, height);
  host_->SetTimeUpdateCallback(
      NewCallback(this, &VideoRendererImpl::TimeUpdateCallback));
  SubmitReads();
  return true;
}

void VideoRendererImpl::SubmitReads() {
  int number_to_read;
  {
    AutoLock auto_lock(lock_);
    submit_reads_task_ = NULL;
    number_to_read = number_of_reads_needed_;
    number_of_reads_needed_ = 0;
  }
  while (number_to_read > 0) {
    decoder_->Read(new media::AssignableBuffer<VideoRendererImpl,
                                               media::VideoFrame>(this));
    --number_to_read;
  }
}

void VideoRendererImpl::SetRect(const gfx::Rect& rect) {
  rect_ = rect;
  // TODO(ralphl) What are all these rects???
}

// This method is always called on the renderer's thread, so it will not be
// reentered.  However, it does maniuplate the queue and the current frame
// timestamp, so those manipulations need to happen with the lock held.
void VideoRendererImpl::Paint(skia::PlatformCanvas *canvas,
                              const gfx::Rect& rect) {
  VideoFrame* video_frame;
  base::TimeDelta time_of_next_frame;
  bool need_to_convert_frame = false;
  {
    AutoLock auto_lock(lock_);
    UpdateQueue(host_->GetPipelineStatus()->GetTime(), NULL, &video_frame,
                &time_of_next_frame);
    if (video_frame) {
      // if the |current_frame_| bitmap already has the RGB image of the
      // front video_frame then there's on no need to call CopyToCurentFrame
      // to convert the video_frame to RBG.  If we do need to convert a new
      // frame, then remember the time of the frame so we might be able to skip
      // this step if asked to repaint multiple times.  Note that the
      // |current_frame_timestamp_| member needs to only be accessed with the
      // |lock_| acquired, so we set the timestamp here even though the
      // conversion won't take place until we call CopyToCurrentFrame.  It's
      // not a problem because this method is the only place where the current
      // frame is updated, and it is always called on the renderer's thread.
      const base::TimeDelta frame_timestamp = video_frame->GetTimestamp();
      need_to_convert_frame = (current_frame_timestamp_ != frame_timestamp);
      if (need_to_convert_frame) {
        current_frame_timestamp_ = frame_timestamp;
      }
    }
  }

  // We no longer hold the |lock_|. Don't access members other than |host_| and
  // |current_frame_|.
  if (video_frame) {
    if (need_to_convert_frame) {
      CopyToCurrentFrame(video_frame);
    }
    video_frame->Release();
    SkMatrix matrix;
    matrix.setTranslate(static_cast<SkScalar>(rect.x()),
                        static_cast<SkScalar>(rect.y()));
    // TODO(ralphl): I have no idea what's going on here.  How are these
    // rects related to eachother?  What does SetRect() mean?
    matrix.preScale(static_cast<SkScalar>(rect.width() / rect_.width()),
                    static_cast<SkScalar>(rect.height() / rect_.height()));
    canvas->drawBitmapMatrix(current_frame_, matrix, NULL);
  }
  host_->ScheduleTimeUpdateCallback(time_of_next_frame);
}

void VideoRendererImpl::CopyToCurrentFrame(VideoFrame* video_frame) {
  media::VideoSurface frame_in;
  if (video_frame->Lock(&frame_in)) {
    // TODO(ralphl):  Actually do the color space conversion here!
    // This is temporary code to set the bits of the current_frame_ to
    // blue.
    current_frame_.eraseRGB(0x00, 0x00, 0xFF);
    video_frame->Unlock();
  } else {
    NOTREACHED();
  }
}

// Assumes |lock_| has been acquired!
bool VideoRendererImpl::UpdateQueue(base::TimeDelta time,
                                    VideoFrame* new_frame,
                                    VideoFrame** front_frame_out,
                                    base::TimeDelta* time_of_next_frame) {
  bool updated_front = false;

  // If a new frame is passed in then put it at the back of the queue.  If the
  // queue was empty, then we've updated the front too.
  if (new_frame) {
    updated_front = queue_.empty();
    new_frame->AddRef();
    queue_.push_back(new_frame);
  }

  // Now make sure that the front of the queue is the correct frame to display
  // right now.  Discard any frames that are past the current time.  If any
  // frames are discarded then increment the |number_of_reads_needed_| member.
  while (queue_.size() > 1 &&
         queue_.front()->GetTimestamp() +
         base::TimeDelta::FromMicroseconds(kFrameSkipAheadLimit) >= time) {
    queue_.front()->Release();
    queue_.pop_front();
    updated_front = true;
    ++number_of_reads_needed_;
  }

  // If the caller wants the front frame then return it, with the reference
  // count incremented.  The caller must call Release() on the returned frame.
  if (front_frame_out) {
    if (queue_.empty()) {
      *front_frame_out = NULL;
    } else {
      *front_frame_out = queue_.front();
      (*front_frame_out)->AddRef();
    }
  }

  // If the caller wants the time of the next frame, return our best guess:
  //   If no frame, then wait for a while
  //   If only one frame, then use the duration of the front frame
  //   If there is more than one frame, return the time of the next frame.
  if (time_of_next_frame) {
    if (queue_.empty()) {
      *time_of_next_frame = host_->GetPipelineStatus()->GetInterpolatedTime() +
                            base::TimeDelta::FromMicroseconds(kSleepIfNoFrame);
    } else {
      if (queue_.size() == 1) {
        *time_of_next_frame = queue_.front()->GetTimestamp() +
                              queue_.front()->GetDuration();
      } else {
        *time_of_next_frame = queue_[1]->GetTimestamp();
      }
    }
  }

  // If any frames have been removed we need to call the decoder again.  Note
  // that the PostSubmitReadsTask method will only post the task if there are
  // pending reads.
  PostSubmitReadsTask();

  // True if the front of the queue is a new frame.
  return updated_front;
}

// Assumes |lock_| has been acquired!
void VideoRendererImpl::DiscardAllFrames() {
  while (!queue_.empty()) {
    queue_.front()->Release();
    queue_.pop_front();
    ++number_of_reads_needed_;
  }
  current_frame_timestamp_ = base::TimeDelta::FromMicroseconds(kNoCurrentFrame);
}

// Assumes |lock_| has been acquired!
void VideoRendererImpl::PostSubmitReadsTask() {
  if (number_of_reads_needed_ > 0 && !submit_reads_task_) {
    submit_reads_task_ = NewRunnableMethod(this,
                                           &VideoRendererImpl::SubmitReads);
    host_->PostTask(submit_reads_task_);
  }
}

void VideoRendererImpl::TimeUpdateCallback(base::TimeDelta time) {
  AutoLock auto_lock(lock_);
  if (IsRunning() && UpdateQueue(time, NULL, NULL, NULL)) {
    delegate_->PostRepaintTask();
  }
}

void VideoRendererImpl::OnAssignment(VideoFrame* video_frame) {
  bool call_initialized = false;
  {
    AutoLock auto_lock(lock_);
    if (IsRunning()) {
      // TODO(ralphl): if (!preroll_complete_ && EndOfStream) call_init = true
      // and preroll_complete_ = true.
      // TODO(ralphl): If(Seek()) then discard but we don't have SeekFrame().
      if (false) {
        // TODO(ralphl): this is the seek() logic.
        DiscardAllFrames();
        ++number_of_reads_needed_;
        PostSubmitReadsTask();
      } else {
        if (UpdateQueue(host_->GetPipelineStatus()->GetInterpolatedTime(),
                        video_frame, NULL, NULL)) {
          delegate_->PostRepaintTask();
        }
        if (!preroll_complete_ && queue_.size() == kDefaultNumberOfFrames) {
          preroll_complete_ = true;
          call_initialized = true;
        }
      }
    }
  }
  // |lock_| no longer held.  Call the pipeline if we've just entered a
  // completed preroll state.
  if (call_initialized) {
    host_->InitializationComplete();
  }
}

