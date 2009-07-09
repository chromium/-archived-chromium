// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// A FilterHost implementation based on gmock.  Combined with setting a message
// loop on a filter, permits single-threaded testing of filters without
// requiring a pipeline.

#ifndef MEDIA_BASE_MOCK_FILTER_HOST_H_
#define MEDIA_BASE_MOCK_FILTER_HOST_H_

#include <string>

#include "base/scoped_ptr.h"
#include "base/waitable_event.h"
#include "media/base/factory.h"
#include "media/base/filter_host.h"
#include "media/base/filters.h"
#include "media/base/media_format.h"
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

}  // namespace media

#endif  // MEDIA_BASE_MOCK_FILTER_HOST_H_
