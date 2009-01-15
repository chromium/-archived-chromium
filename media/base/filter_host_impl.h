// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of FilterHost.

#ifndef MEDIA_BASE_FILTER_HOST_IMPL_H_
#define MEDIA_BASE_FILTER_HOST_IMPL_H_

#include "base/task.h"
#include "media/base/filter_host.h"

namespace media {

class FilterHostImpl : public FilterHost {
 public:
  FilterHostImpl();

  // FilterHost implementation.
  virtual int64 GetTime() const;
  virtual void SetTime(int64 time);
  virtual int64 GetDuration() const;
  virtual void SetDuration(int64 duration);
  virtual void PostTask(Task* task);
  virtual bool PlayComplete();
  virtual bool PauseComplete();
  virtual bool SeekComplete();
  virtual bool ShutdownComplete();
  virtual void Error(int error);
  virtual void EndOfStream();

  virtual void SetPlayCallback(Callback0::Type* callback);
  virtual void SetPauseCallback(Callback1<bool>::Type* callback);
  virtual void SetSeekCallback(Callback1<int64>::Type* callback);
  virtual void SetShutdownCallback(Callback0::Type* callback);
  virtual void SetClockCallback(Callback1<int64>::Type* callback);
  virtual void SetErrorCallback(Callback1<int>::Type* callback);

 protected:
  virtual ~FilterHostImpl() {}

 private:
  scoped_ptr<Callback0::Type> play_callback_;
  scoped_ptr<Callback1<bool>::Type> pause_callback_;
  scoped_ptr<Callback1<int64>::Type> seek_callback_;
  scoped_ptr<Callback0::Type> shutdown_callback_;
  scoped_ptr<Callback1<int64>::Type> clock_callback_;
  scoped_ptr<Callback1<int>::Type> error_callback_;
};

}  // namespace media

#endif  // MEDIA_BASE_FILTER_HOST_IMPL_H_
