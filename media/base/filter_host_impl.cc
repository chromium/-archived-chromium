// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filter_host_impl.h"

namespace media {

void FilterHostImpl::SetTimeUpdateCallback(
      Callback1<base::TimeDelta>::Type* callback) {
  time_update_callback_.reset(callback);
}

void FilterHostImpl::RunTimeUpdateCallback(base::TimeDelta time) {
  if (time_update_callback_.get()) {
    time_update_callback_->Run(time);
  }
}

// To understand why this method takes the |caller| parameter, see the comments
// of the TimeUpdatedTask in the file filter_host_impl.h.
void FilterHostImpl::RunScheduledTimeUpdateCallback(TimeUpdateTask* caller) {
  time_update_lock_.Acquire();
  if (caller == scheduled_time_update_task_) {
    scheduled_time_update_task_ = NULL;
  }
  time_update_lock_.Release();
  RunTimeUpdateCallback(pipeline()->GetInterpolatedTime());
}

void FilterHostImpl::ScheduleTimeUpdateCallback(base::TimeDelta time) {
  time_update_lock_.Acquire();
  if (stopped_) {
    time_update_lock_.Release();
  } else {
    DCHECK(time_update_callback_.get());
    if (scheduled_time_update_task_) {
      scheduled_time_update_task_->Cancel();
    }
    scheduled_time_update_task_ = new TimeUpdateTask(this);
    Task* task = scheduled_time_update_task_;
    time_update_lock_.Release();

    // Here, we compute the delta from now & adjust it by the playback rate.
    time -= pipeline()->GetInterpolatedTime();
    int delay = static_cast<int>(time.InMilliseconds());
    float rate = pipeline()->GetPlaybackRate();
    if (rate != 1.0f && rate != 0.0f) {
      delay = static_cast<int>(delay / rate);
    }
    if (delay > 0) {
      pipeline_thread_->message_loop()->PostDelayedTask(FROM_HERE, task, delay);
    } else {
      pipeline_thread_->message_loop()->PostTask(FROM_HERE, task);
    }
  }
}

void FilterHostImpl::InitializationComplete() {
  pipeline_thread_->InitializationComplete(this);
}

void FilterHostImpl::PostTask(Task* task) {
  if (stopped_) {
    delete task;
  } else {
    pipeline_thread_->PostTask(task);
  }
}

void FilterHostImpl::Error(PipelineError error) {
  pipeline_thread_->Error(error);
}

void FilterHostImpl::SetTime(base::TimeDelta time) {
  pipeline_thread_->SetTime(time);
}

void FilterHostImpl::SetDuration(base::TimeDelta duration) {
  pipeline()->SetDuration(duration);
}

void FilterHostImpl::SetBufferedTime(base::TimeDelta buffered_time) {
  pipeline()->SetBufferedTime(buffered_time);
}

void FilterHostImpl::SetTotalBytes(int64 total_bytes) {
  pipeline()->SetTotalBytes(total_bytes);
}

void FilterHostImpl::SetBufferedBytes(int64 buffered_bytes) {
  pipeline()->SetBufferedBytes(buffered_bytes);
}

void FilterHostImpl::SetVideoSize(size_t width, size_t height) {
  pipeline()->SetVideoSize(width, height);
}

const PipelineStatus* FilterHostImpl::GetPipelineStatus() const {
  return pipeline();
}

void FilterHostImpl::Stop() {
  if (!stopped_) {
    filter_->Stop();
    AutoLock auto_lock(time_update_lock_);
    stopped_ = true;
    if (scheduled_time_update_task_) {
      scheduled_time_update_task_->Cancel();
    }
  }
}

}  // namespace media
