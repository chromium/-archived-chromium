// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/base/buffers.h"
#include "media/base/filter_host.h"
#include "media/base/pipeline.h"
#include "media/filters/video_renderer_base.h"

namespace media {

const size_t VideoRendererBase::kDefaultNumberOfFrames = 4;

const base::TimeDelta VideoRendererBase::kDefaultSkipFrameDelta =
    base::TimeDelta::FromMilliseconds(2);

const base::TimeDelta VideoRendererBase::kDefaultEmptyQueueSleep =
    base::TimeDelta::FromMilliseconds(15);

//------------------------------------------------------------------------------

VideoRendererBase::VideoRendererBase()
    : number_of_frames_(kDefaultNumberOfFrames),
      skip_frame_delta_(kDefaultSkipFrameDelta),
      empty_queue_sleep_(kDefaultEmptyQueueSleep),
      number_of_reads_needed_(kDefaultNumberOfFrames),
      submit_reads_task_(NULL),
      preroll_complete_(false) {
}

VideoRendererBase::VideoRendererBase(size_t number_of_frames,
                                     base::TimeDelta skip_frame_delta,
                                     base::TimeDelta empty_queue_sleep)
    : number_of_frames_(number_of_frames),
      skip_frame_delta_(skip_frame_delta),
      empty_queue_sleep_(empty_queue_sleep),
      number_of_reads_needed_(number_of_frames),
      submit_reads_task_(NULL),
      preroll_complete_(false) {
}

VideoRendererBase::~VideoRendererBase() {
  Stop();
}

// static
bool VideoRendererBase::IsMediaFormatSupported(
    const MediaFormat& media_format) {
  int width;
  int height;
  return ParseMediaFormat(media_format, &width, &height);
}

// static
bool VideoRendererBase::ParseMediaFormat(const MediaFormat& media_format,
                                         int* width_out,
                                         int* height_out) {
  DCHECK(width_out && height_out);
  std::string mime_type;
  return (media_format.GetAsString(MediaFormat::kMimeType, &mime_type) &&
          mime_type.compare(mime_type::kUncompressedVideo) == 0 &&
          media_format.GetAsInteger(MediaFormat::kWidth, width_out) &&
          media_format.GetAsInteger(MediaFormat::kHeight, height_out));
}

void VideoRendererBase::Stop() {
  OnStop();
  AutoLock auto_lock(lock_);
  DiscardAllFrames();
  if (submit_reads_task_) {
    // The task is owned by the message loop, so we don't delete it here.  We
    // know the task won't call us because we canceled it, and we know we are
    // on the pipeline thread, since we're in the filter's Stop method, so there
    // is no threading problem.  Just let the task be run by the message loop
    // and then be killed.
    submit_reads_task_->Cancel();
    submit_reads_task_ = NULL;
  }
  decoder_ = NULL;
}

bool VideoRendererBase::Initialize(VideoDecoder* decoder) {
  int width, height;
  decoder_ = decoder;
  if (ParseMediaFormat(decoder_->media_format(), &width, &height) &&
      OnInitialize(width, height)) {
    host_->SetVideoSize(width, height);
    host_->SetTimeUpdateCallback(
        NewCallback(this, &VideoRendererBase::TimeUpdateCallback));
    SubmitReads();
    return true;
  }
  decoder_ = NULL;
  return false;
}

void VideoRendererBase::SubmitReads() {
  int number_to_read;
  {
    AutoLock auto_lock(lock_);
    submit_reads_task_ = NULL;
    number_to_read = number_of_reads_needed_;
    number_of_reads_needed_ = 0;
  }
  while (number_to_read > 0) {
    decoder_->Read(NewCallback(this, &VideoRendererBase::ReadComplete));
    --number_to_read;
  }
}

bool VideoRendererBase::UpdateQueue(base::TimeDelta time,
                                    VideoFrame* new_frame) {
  lock_.AssertAcquired();
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
  if (preroll_complete_) {
    while (queue_.size() > 1 &&
           queue_[1]->GetTimestamp() - skip_frame_delta_ <= time) {
      queue_.front()->Release();
      queue_.pop_front();
      updated_front = true;
      ++number_of_reads_needed_;
    }
  }

  // If any frames have been removed we need to call the decoder again.  Note
  // that the PostSubmitReadsTask method will only post the task if there are
  // pending reads.
  PostSubmitReadsTask();

  // True if the front of the queue is a new frame.
  return updated_front;
}

void VideoRendererBase::DiscardAllFrames() {
  lock_.AssertAcquired();
  while (!queue_.empty()) {
    queue_.front()->Release();
    queue_.pop_front();
    ++number_of_reads_needed_;
  }
}

// Assumes |lock_| has been acquired!
void VideoRendererBase::PostSubmitReadsTask() {
  if (number_of_reads_needed_ > 0 && !submit_reads_task_) {
    submit_reads_task_ = NewRunnableMethod(this,
                                           &VideoRendererBase::SubmitReads);
    host_->PostTask(submit_reads_task_);
  }
}

void VideoRendererBase::TimeUpdateCallback(base::TimeDelta time) {
  bool request_repaint;
  {
    AutoLock auto_lock(lock_);
    request_repaint = (IsRunning() && UpdateQueue(time, NULL));
  }
  if (request_repaint) {
    OnPaintNeeded();
  }
}

void VideoRendererBase::ReadComplete(VideoFrame* video_frame) {
  bool call_initialized = false;
  bool request_repaint = false;
  {
    AutoLock auto_lock(lock_);
    if (IsRunning()) {
      if (video_frame->IsDiscontinuous()) {
        DiscardAllFrames();
      }
      if (UpdateQueue(host_->GetPipelineStatus()->GetInterpolatedTime(),
                      video_frame)) {
        request_repaint = preroll_complete_;
      }
      if (!preroll_complete_ && (queue_.size() == number_of_frames_ ||
                                 video_frame->IsEndOfStream())) {
        preroll_complete_ = true;
        call_initialized = true;
        request_repaint = true;
      }
    }
  }
  // |lock_| no longer held.  Call the pipeline if we've just entered a
  // completed preroll state.
  if (call_initialized) {
    host_->InitializationComplete();
  }
  if (request_repaint) {
    OnPaintNeeded();
  }
}

void VideoRendererBase::GetCurrentFrame(scoped_refptr<VideoFrame>* frame_out) {
  AutoLock auto_lock(lock_);
  *frame_out = NULL;
  if (IsRunning()) {
    base::TimeDelta time = host_->GetPipelineStatus()->GetInterpolatedTime();
    base::TimeDelta time_next_frame;
    UpdateQueue(time, NULL);
    if (queue_.empty()) {
      time_next_frame = time + empty_queue_sleep_;
    } else {
      *frame_out = queue_.front();
      if (queue_.size() == 1) {
        time_next_frame = (*frame_out)->GetTimestamp() +
                          (*frame_out)->GetDuration();
      } else {
        time_next_frame = queue_[1]->GetTimestamp();
      }
    }
    host_->ScheduleTimeUpdateCallback(time_next_frame);
  }
}

}  // namespace
