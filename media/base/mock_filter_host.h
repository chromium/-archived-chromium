// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// The corresponding FilterHost implementation for MockPipeline.  Maintains a
// reference to the parent MockPipeline and a reference to the Filter its
// hosting.  Common usage is to check if the hosted filter has initialized by
// calling IsInitialized().

#ifndef MEDIA_BASE_MOCK_FILTER_HOST_H_
#define MEDIA_BASE_MOCK_FILTER_HOST_H_

#include <string>

#include "media/base/factory.h"
#include "media/base/filter_host.h"
#include "media/base/filters.h"
#include "media/base/media_format.h"
#include "media/base/mock_pipeline.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

template <class Filter>
class MockFilterHost : public FilterHost {
 public:
  MockFilterHost(MockPipeline* mock_pipeline, Filter* filter)
      : mock_pipeline_(mock_pipeline),
        filter_(filter),
        initialized_(false) {
    EXPECT_TRUE(mock_pipeline_);
    EXPECT_TRUE(filter_);
    filter_->SetFilterHost(this);
  }

  virtual ~MockFilterHost() {}

  virtual const PipelineStatus* GetPipelineStatus() const {
    return mock_pipeline_;
  }

  virtual void SetTimeUpdateCallback(Callback1<base::TimeDelta>::Type* cb) {
    time_update_callback_.reset(cb);
  }

  virtual void ScheduleTimeUpdateCallback(base::TimeDelta time) {
    scheduled_callback_time_ = time;
  }

  virtual void InitializationComplete() {
    EXPECT_FALSE(initialized_);
    initialized_ = true;
  }

  virtual void PostTask(Task* task) {
    mock_pipeline_->PostTask(task);
  }

  virtual void Error(PipelineError error) {
    mock_pipeline_->Error(error);
  }

  virtual void SetTime(base::TimeDelta time) {
    mock_pipeline_->SetTime(time);
  }

  virtual void SetDuration(base::TimeDelta duration) {
    mock_pipeline_->SetDuration(duration);
  }

  virtual void SetBufferedTime(base::TimeDelta buffered_time) {
    mock_pipeline_->SetBufferedTime(buffered_time);
  }

  virtual void SetTotalBytes(int64 total_bytes) {
    mock_pipeline_->SetTotalBytes(total_bytes);
  }

  virtual void SetBufferedBytes(int64 buffered_bytes) {
    mock_pipeline_->SetBufferedBytes(buffered_bytes);
  }

  virtual void SetVideoSize(size_t width, size_t height) {
    mock_pipeline_->SetVideoSize(width, height);
  }

  // Used by unit tests to manipulate the filter.
  base::TimeDelta GetScheduledCallbackTime() const {
    return scheduled_callback_time_;
  }

  Callback1<base::TimeDelta>::Type* GetTimeUpdateCallback() const {
    return time_update_callback_.get();
  }

  bool IsInitialized() const {
    return initialized_;
  }

 private:
  MockPipeline* mock_pipeline_;
  scoped_refptr<Filter> filter_;
  scoped_ptr<Callback1<base::TimeDelta>::Type> time_update_callback_;

  // Keeps track of the time passed into ScheduleTimeUpdateCallback().
  base::TimeDelta scheduled_callback_time_;

  // Tracks if the filter has executed InitializationComplete().
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(MockFilterHost);
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_FILTER_HOST_H_
