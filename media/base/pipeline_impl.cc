// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline_impl.h"

namespace media {

PipelineImpl::PipelineImpl() : time_(0), duration_(0) {}

PipelineImpl::~PipelineImpl() {}

bool PipelineImpl::Initialize(FilterFactoryInterface* filter_factory,
                              const std::string& uri) {
  // TODO(scherkus): implement Initialize.
  NOTIMPLEMENTED();
  return false;
}

bool PipelineImpl::Play() {
  // TODO(scherkus): implement Play.
  NOTIMPLEMENTED();
  return false;
}

bool PipelineImpl::Pause() {
  // TODO(scherkus): implement Pause.
  NOTIMPLEMENTED();
  return false;
}

bool PipelineImpl::Seek(int64 seek_position) {
  // TODO(scherkus): implement Seek.
  NOTIMPLEMENTED();
  return false;
}

void PipelineImpl::Shutdown() {
  // TODO(scherkus): implement Shutdown.
  NOTIMPLEMENTED();
}

int64 PipelineImpl::GetTime() const {
  return time_;
}

int64 PipelineImpl::GetDuration() const {
  return duration_;
}

void PipelineImpl::SetStateChangedCallback(
    Callback1<PipelineState>::Type* callback) {
  state_changed_callback_.reset(callback);
}

void PipelineImpl::SetTimeChangedCallback(Callback1<int64>::Type* callback) {
  time_changed_callback_.reset(callback);
}

}  // namespace media
