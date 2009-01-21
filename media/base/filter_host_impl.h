// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
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

  // FilterHost interface.
  virtual const PipelineStatus* GetPipelineStatus() const;
  virtual void SetTimeUpdateCallback(Callback1<int64>::Type* callback);
  virtual void InitializationComplete();
  virtual void PostTask(Task* task);
  virtual void Error(PipelineError error);
  virtual void SetTime(int64 time);
  virtual void SetDuration(int64 duration);
  virtual void SetBufferedTime(int64 buffered_time);
  virtual void SetTotalBytes(int64 total_bytes);
  virtual void SetBufferedBytes(int64 buffered_bytes);
  virtual void SetVideoSize(size_t width, size_t height);

 protected:
  virtual ~FilterHostImpl() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FilterHostImpl);
};

}  // namespace media

#endif  // MEDIA_BASE_FILTER_HOST_IMPL_H_
