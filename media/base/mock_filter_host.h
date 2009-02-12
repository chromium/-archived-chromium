// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

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

template <class Filter, class Source>
class MockFilterHost : public FilterHost {
 public:
  MockFilterHost(const MediaFormat* media_format, Source source) {
    scoped_refptr<FilterFactory> factory = Filter::CreateFactory();
    filter_ = factory->Create<Filter>(media_format);
    EXPECT_TRUE(filter_.get());
    if (filter_.get()) {
      filter_->SetFilterHost(this);
      filter_->Initialize(source);
    }
  }

  virtual ~MockFilterHost() {
    filter_->Stop();
  }

  virtual const PipelineStatus* GetPipelineStatus() const {
    return &mock_pipeline_;
  }

  virtual void SetTimeUpdateCallback(Callback1<base::TimeDelta>::Type* cb) {
      time_update_callback_.reset(cb);
  }

  virtual void ScheduleTimeUpdateCallback(base::TimeDelta time) {
    // TODO(ralphl): Implement MockFilte::ScheduleTimeUpdateCallback.
    NOTIMPLEMENTED();
  }

  virtual void InitializationComplete() {
    EXPECT_FALSE(mock_pipeline_.IsInitialized());
    mock_pipeline_.SetInitialized(true);
  }

  virtual void PostTask(Task* task) {
    // TODO(ralphl): Implement MockPipeline::PostTask.
    NOTIMPLEMENTED();
  }

  virtual void Error(PipelineError error) {
    mock_pipeline_.Error(error);
  }

  virtual void SetTime(base::TimeDelta time) {
    mock_pipeline_.SetTime(time);
  }

  virtual void SetDuration(base::TimeDelta duration) {
    mock_pipeline_.SetDuration(duration);
  }

  virtual void SetBufferedTime(base::TimeDelta buffered_time) {
    mock_pipeline_.SetBufferedTime(buffered_time);
  }

  virtual void SetTotalBytes(int64 total_bytes) {
    mock_pipeline_.SetTotalBytes(total_bytes);
  }

  virtual void SetBufferedBytes(int64 buffered_bytes) {
    mock_pipeline_.SetBufferedBytes(buffered_bytes);
  }

  // Sets the size of the video output in pixel units.
  virtual void SetVideoSize(size_t width, size_t height) {
    mock_pipeline_.SetVideoSize(width, height);
  }

  // Used by unit tests to manipulate the filter.
  Filter* filter() const { return filter_; }

  MockPipeline* mock_pipeline() const { return &mock_pipeline_; }

 private:
  MockPipeline mock_pipeline_;
  scoped_refptr<Filter> filter_;
  scoped_ptr<Callback1<base::TimeDelta>::Type> time_update_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockFilterHost);
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_FILTER_HOST_H_
