// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/platform_thread.h"
#include "media/base/pipeline_impl.h"
#include "media/base/media_format.h"
#include "media/base/filters.h"
#include "media/base/factory.h"
#include "media/base/filter_host.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::FilterFactory;
using media::FilterFactoryCollection;
using media::FilterHost;
using media::MediaFormat;
using media::PipelineImpl;
using media::TypeFilterFactory;

class TestDataSource : public media::DataSource {
 public:
  static bool Create(const MediaFormat* media_format,
                     TestDataSource** filter_out) {
    *filter_out = new TestDataSource();
    return true;
  }
  virtual void Stop() {}
  // This filter will hang in initialization because it never calls
  // FilterHost::InitializationComplete
  virtual bool Initialize(const std::string& uri) {
    return true;
  }
  virtual const MediaFormat* GetMediaFormat() {
    return NULL;  // TODO(ralphl):  Return octot thingie...
  }
  virtual size_t Read(char* data, size_t size) {
    return 0;
  }
  virtual bool GetPosition(int64* position_out) {
    return 0;
  }
  virtual bool SetPosition(int64 position) {
    return true;
  }
  virtual bool GetSize(int64* size_out) {
    return 0;
  }
};

TEST(PipelineImplTest, Basic) {
  std::string uri("test.mov");
  PipelineImpl pipeline;
  scoped_refptr<FilterFactoryCollection> f = new FilterFactoryCollection();
  f->AddFactory(new TypeFilterFactory<TestDataSource>);
  pipeline.Start(f, uri, NULL);
  PlatformThread::Sleep(10);
  pipeline.Stop();
}
