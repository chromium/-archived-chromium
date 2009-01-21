// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline_impl.h"

namespace media {

PipelineImpl::PipelineImpl() {
  // TODO(ralphl): implement PipelineImpl constructor.
  NOTIMPLEMENTED();
}

PipelineImpl::~PipelineImpl() {
  // TODO(ralphl): implement PipelineImpl destructor.
  NOTIMPLEMENTED();
}

bool PipelineImpl::IsInitialized() const {
  // TODO(ralphl): implement IsInitialized.
  NOTIMPLEMENTED();
  return false;
}

int64 PipelineImpl::GetDuration() const {
  // TODO(ralphl): implement GetDuration.
  NOTIMPLEMENTED();
  return 0;
}

int64 PipelineImpl::GetBufferedTime() const {
  // TODO(ralphl): implement GetBufferedTime.
  NOTIMPLEMENTED();
  return 0;
}

int64 PipelineImpl::GetTotalBytes() const {
  // TODO(ralphl): implement GetTotalBytes.
  NOTIMPLEMENTED();
  return 0;
}

int64 PipelineImpl::GetBufferedBytes() const {
  // TODO(ralphl): implement GetBufferedBytes.
  NOTIMPLEMENTED();
  return 0;
}

void PipelineImpl::GetVideoSize(size_t* width_out, size_t* height_out) const {
  // TODO(ralphl): implement GetVideoSize.
  NOTIMPLEMENTED();
  width_out = 0;
  height_out = 0;
}

float PipelineImpl::GetVolume() const {
  // TODO(ralphl): implement GetVolume.
  NOTIMPLEMENTED();
  return 0;
}

float PipelineImpl::GetPlaybackRate() const {
  // TODO(ralphl): implement GetPlaybackRate.
  NOTIMPLEMENTED();
  return 0;
}

int64 PipelineImpl::GetTime() const {
  // TODO(ralphl): implement GetTime.
  NOTIMPLEMENTED();
  return 0;
}

PipelineError PipelineImpl::GetError() const {
  // TODO(ralphl): implement GetError.
  NOTIMPLEMENTED();
  return PIPELINE_ERROR_INITIALIZATION_FAILED;
}

bool PipelineImpl::Start(FilterFactory* filter_factory,
                         const std::string& uri,
                         Callback1<bool>::Type* init_complete_callback) {
  // TODO(ralphl): implement Start.
  NOTIMPLEMENTED();
  return false;
}

void PipelineImpl::Stop() {
  // TODO(ralphl): implement Stop.
  NOTIMPLEMENTED();
}

bool PipelineImpl::SetPlaybackRate(float rate) {
  // TODO(ralphl): implement SetPlaybackRate.
  NOTIMPLEMENTED();
  return false;
}

bool PipelineImpl::Seek(int64 time) {
  // TODO(ralphl): implement Seek.
  NOTIMPLEMENTED();
  return false;
}

bool PipelineImpl::SetVolume(float volume) {
  // TODO(ralphl): implement SetVolume.
  NOTIMPLEMENTED();
  return false;
}

}  // namespace media
