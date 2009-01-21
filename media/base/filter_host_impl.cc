// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task.h"
#include "media/base/filter_host_impl.h"

namespace media {

FilterHostImpl::FilterHostImpl() {
  // TODO(ralphl): implement FilterHostImpl constructor.
  NOTIMPLEMENTED();
}

const PipelineStatus* FilterHostImpl::GetPipelineStatus() const {
  // TODO(ralphl): implement GetPipelineStatus.
  NOTIMPLEMENTED();
  return NULL;
}

void FilterHostImpl::SetTimeUpdateCallback(Callback1<int64>::Type* callback) {
  // TODO(ralphl): implement SetTimeUpdateCallback.
  NOTIMPLEMENTED();
}

void FilterHostImpl::InitializationComplete() {
  // TODO(ralphl): implement InitializationComplete.
  NOTIMPLEMENTED();
}

void FilterHostImpl::PostTask(Task* task) {
  // TODO(ralphl): implement PostTask.
  NOTIMPLEMENTED();
}

void FilterHostImpl::Error(PipelineError error) {
  // TODO(ralphl): implement Error.
  NOTIMPLEMENTED();
}

void FilterHostImpl::SetTime(int64 time) {
  // TODO(ralphl): implement SetTime.
  NOTIMPLEMENTED();
}

void FilterHostImpl::SetDuration(int64 duration) {
  // TODO(ralphl): implement SetDuration.
  NOTIMPLEMENTED();
}

void FilterHostImpl::SetBufferedTime(int64 buffered_time) {
  // TODO(ralphl): implement SetBufferedTime.
  NOTIMPLEMENTED();
}

void FilterHostImpl::SetTotalBytes(int64 total_bytes) {
  // TODO(ralphl): implement.
  NOTIMPLEMENTED();
}

void FilterHostImpl::SetBufferedBytes(int64 buffered_bytes) {
  // TODO(ralphl): implement.
  NOTIMPLEMENTED();
}

void SetVideoSize(size_t width, size_t height) {
  // TODO(ralphl): implement.
  NOTIMPLEMENTED();
}

}  // namespace media
