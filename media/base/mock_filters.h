// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// A new breed of mock media filters, this time using gmock!  Feel free to add
// actions if you need interesting side-effects (i.e., copying data to the
// buffer passed into MockDataSource::Read()).
//
// Don't forget you can use StrictMock<> and NiceMock<> if you want the mock
// filters to fail the test or do nothing when an unexpected method is called.
// http://code.google.com/p/googlemock/wiki/CookBook#Nice_Mocks_and_Strict_Mocks

#ifndef MEDIA_BASE_MOCK_FILTERS_H_
#define MEDIA_BASE_MOCK_FILTERS_H_

#include "media/base/filters.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockDataSource : public DataSource {
 public:
  MockDataSource() {}

  // MediaFilter implementation.
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD1(Seek, void(base::TimeDelta time));

  // DataSource implementation.
  MOCK_METHOD1(Initialize, bool(const std::string& url));
  MOCK_METHOD0(media_format, const MediaFormat&());
  MOCK_METHOD2(Read, size_t(uint8* data, size_t size));
  MOCK_METHOD1(GetPosition, bool(int64* position_out));
  MOCK_METHOD1(SetPosition, bool(int64 position));
  MOCK_METHOD1(GetSize, bool(int64* size_out));
  MOCK_METHOD0(IsSeekable, bool());

 protected:
  virtual ~MockDataSource() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDataSource);
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_FILTERS_H_
