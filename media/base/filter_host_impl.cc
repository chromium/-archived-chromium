// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filter_host_impl.h"

namespace media {

void FilterHostImpl::SetTimeUpdateCallback(
      Callback1<base::TimeDelta>::Type* callback) {
  time_update_callback_.reset(callback);
}

void FilterHostImpl::RunTimeUpdateCallback(base::TimeDelta time) {
  if (time_update_callback_.get())
    time_update_callback_->Run(time);
}

void FilterHostImpl::InitializationComplete() {
  pipeline_thread_->InitializationComplete(this);
}

void FilterHostImpl::PostTask(Task* task) {
  DCHECK(!stopped_);
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
  pipeline()->duration_ = duration;
}

void FilterHostImpl::SetBufferedTime(base::TimeDelta buffered_time) {
  pipeline()->buffered_time_ = buffered_time;
}

void FilterHostImpl::SetTotalBytes(int64 total_bytes) {
  pipeline()->total_bytes_ = total_bytes;
}

void FilterHostImpl::SetBufferedBytes(int64 buffered_bytes) {
  pipeline()->buffered_bytes_ = buffered_bytes;
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
    stopped_ = true;
  }
}

}  // namespace media
