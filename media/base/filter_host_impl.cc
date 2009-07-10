// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filter_host_impl.h"

namespace media {

void FilterHostImpl::InitializationComplete() {
  pipeline_internal_->InitializationComplete(this);
}

void FilterHostImpl::Error(PipelineError error) {
  pipeline_internal_->Error(error);
}

base::TimeDelta FilterHostImpl::GetTime() const {
  return pipeline()->GetTime();
}

void FilterHostImpl::SetTime(base::TimeDelta time) {
  pipeline_internal_->SetTime(time);
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

void FilterHostImpl::Stop() {
  if (!stopped_) {
    filter_->Stop();
    AutoLock auto_lock(time_update_lock_);
    stopped_ = true;
  }
}

}  // namespace media
