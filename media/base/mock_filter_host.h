// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// A FilterHost implementation based on gmock.  Combined with setting a message
// loop on a filter, permits single-threaded testing of filters without
// requiring a pipeline.
//
// TODO(scherkus): Remove old_mocks::MockFilterHost as soon as other tests have
// transitioned over to the new gmock-based MockFilterHost.

#ifndef MEDIA_BASE_MOCK_FILTER_HOST_H_
#define MEDIA_BASE_MOCK_FILTER_HOST_H_

#include <string>

#include "base/scoped_ptr.h"
#include "base/waitable_event.h"
#include "media/base/factory.h"
#include "media/base/filter_host.h"
#include "media/base/filters.h"
#include "media/base/media_format.h"
#include "media/base/mock_pipeline.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MockFilterHost : public FilterHost {
 public:
  MockFilterHost() {}

  // FilterHost implementation.
  MOCK_METHOD0(InitializationComplete, void());
  MOCK_METHOD1(Error, void(PipelineError error));
  MOCK_CONST_METHOD0(GetTime, base::TimeDelta());
  MOCK_METHOD1(SetTime, void(base::TimeDelta time));
  MOCK_METHOD1(SetDuration, void(base::TimeDelta duration));
  MOCK_METHOD1(SetBufferedTime, void(base::TimeDelta buffered_time));
  MOCK_METHOD1(SetTotalBytes, void(int64 total_bytes));
  MOCK_METHOD1(SetBufferedBytes, void(int64 buffered_bytes));
  MOCK_METHOD2(SetVideoSize, void(size_t width, size_t height));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFilterHost);
};

namespace old_mocks {

// This version is deprecated.
template <class Filter>
class MockFilterHost : public FilterHost {
 public:
  MockFilterHost(MockPipeline* mock_pipeline, Filter* filter)
      : mock_pipeline_(mock_pipeline),
        filter_(filter),
        initialized_(false),
        error_(PIPELINE_OK),
        wait_for_initialized_(false, false),
        wait_for_error_(false, false) {
    EXPECT_TRUE(mock_pipeline_);
    EXPECT_TRUE(filter_);
    filter_->SetFilterHost(this);
  }

  virtual ~MockFilterHost() {}

  virtual void InitializationComplete() {
    EXPECT_FALSE(initialized_);
    initialized_ = true;
    wait_for_initialized_.Signal();
  }

  virtual void Error(PipelineError error) {
    error_ = error;
    mock_pipeline_->Error(error);
    wait_for_error_.Signal();
  }

  virtual base::TimeDelta GetTime() const {
    return mock_pipeline_->GetTime();
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

  bool IsInitialized() const {
    return initialized_;
  }

  bool WaitForInitialized() {
    const base::TimeDelta kTimedWait = base::TimeDelta::FromMilliseconds(500);
    while (!initialized_) {
      if (!wait_for_initialized_.TimedWait(kTimedWait)) {
        return false;
      }
    }
    return true;
  }

  bool WaitForError(PipelineError error) {
    const base::TimeDelta kTimedWait = base::TimeDelta::FromMilliseconds(500);
    while (error_ != error) {
      if (!wait_for_error_.TimedWait(kTimedWait)) {
        return false;
      }
    }
    return true;
  }

 private:
  MockPipeline* mock_pipeline_;
  scoped_refptr<Filter> filter_;

  // Tracks if the filter has executed InitializationComplete().
  bool initialized_;

  // Tracks the last pipeline error set by the filter.
  PipelineError error_;

  // Allows unit tests to wait for particular conditions before asserting.
  base::WaitableEvent wait_for_initialized_;
  base::WaitableEvent wait_for_error_;

  DISALLOW_COPY_AND_ASSIGN(MockFilterHost);
};

}  // namespace old_mocks

}  // namespace media

#endif  // MEDIA_BASE_MOCK_FILTER_HOST_H_
