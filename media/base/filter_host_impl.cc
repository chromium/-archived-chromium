// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task.h"
#include "media/base/filter_host_impl.h"

namespace media {

FilterHostImpl::FilterHostImpl() {
}

int64 FilterHostImpl::GetTime() const {
  // TODO(scherkus): implement GetTime.
  return 0;
}

void FilterHostImpl::SetTime(int64 time) {
  // TODO(scherkus): implement SetTime.
}

int64 FilterHostImpl::GetDuration() const {
  // TODO(scherkus): implement GetDuration.
  return 0;
}

void FilterHostImpl::SetDuration(int64 duration) {
  // TODO(scherkus): implement SetDuration.
}

void FilterHostImpl::PostTask(Task* task) {
  // TODO(scherkus): implement PostTask.
}

bool FilterHostImpl::PlayComplete() {
  // TODO(scherkus): implement PlayComplete.
  return false;
}

bool FilterHostImpl::PauseComplete() {
  // TODO(scherkus): implement PauseComplete.
  return false;
}

bool FilterHostImpl::SeekComplete() {
  // TODO(scherkus): implement SeekComplete.
  return false;
}

bool FilterHostImpl::ShutdownComplete() {
  // TODO(scherkus): implement ShutdownComplete.
  return false;
}

void FilterHostImpl::Error(int error) {
  // TODO(scherkus): implement Error.
}

void FilterHostImpl::EndOfStream() {
  // TODO(scherkus): implement EndOfStream.
}

void FilterHostImpl::SetPlayCallback(Callback0::Type* callback) {
  play_callback_.reset(callback);
}

void FilterHostImpl::SetPauseCallback(Callback1<bool>::Type* callback) {
  pause_callback_.reset(callback);
}

void FilterHostImpl::SetSeekCallback(Callback1<int64>::Type* callback) {
  seek_callback_.reset(callback);
}

void FilterHostImpl::SetShutdownCallback(Callback0::Type* callback) {
  shutdown_callback_.reset(callback);
}

void FilterHostImpl::SetClockCallback(Callback1<int64>::Type* callback) {
  clock_callback_.reset(callback);
}

void FilterHostImpl::SetErrorCallback(Callback1<int>::Type* callback) {
  error_callback_.reset(callback);
}

}  // namespace media
