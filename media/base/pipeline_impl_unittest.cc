// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/platform_thread.h"
#include "base/time.h"
#include "base/waitable_event.h"
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
using media::FilterFactoryImpl1;

class TestDataSource : public media::DataSource {
 public:
  static FilterFactory* CreateFactory(bool hang_in_init) {
     return new FilterFactoryImpl1<TestDataSource, bool>(hang_in_init);
  }
  virtual void Stop() {}
  // This filter will hang in initialization because it never calls
  // FilterHost::InitializationComplete
  virtual bool Initialize(const std::string& uri) {
    if (!hang_in_init_) {
      host_->InitializationComplete();
    }
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

 protected:
  bool hang_in_init_;
  friend class media::FilterFactoryImpl1<TestDataSource, bool>;
  explicit TestDataSource(bool hang_in_init) : hang_in_init_(hang_in_init) {}
  ~TestDataSource() {}

  DISALLOW_COPY_AND_ASSIGN(TestDataSource);
};


class InitWaiter : public base::WaitableEvent {
 public:
  InitWaiter()
    : WaitableEvent(true, false),
      callback_success_status_(false) {}
  void InitializationComplete(bool success) {
    callback_success_status_ = success;
    Signal();
  }
  bool HasBeenCalled() { return IsSignaled(); }
  bool CallbackSuccessStatus() { return callback_success_status_; }
  Callback1<bool>::Type* NewInitCallback() {
    return NewCallback(this, &InitWaiter::InitializationComplete);
  }
  void Reset() {
    base::WaitableEvent::Reset();
    callback_success_status_ = false;
  }

 private:
  bool callback_success_status_;

  DISALLOW_COPY_AND_ASSIGN(InitWaiter);
};


TEST(PipelineImplTest, Basic) {
  std::string url("test.mov");
  PipelineImpl p;
  InitWaiter w;
  p.Start(TestDataSource::CreateFactory(true), url, w.NewInitCallback());
  w.TimedWait(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(w.HasBeenCalled());
  EXPECT_FALSE(p.IsInitialized());
  EXPECT_TRUE(media::PIPELINE_OK == p.GetError());
  p.Stop();

  w.Reset();
  p.Start(TestDataSource::CreateFactory(false), url, w.NewInitCallback());
  w.TimedWait(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(w.HasBeenCalled());
  EXPECT_FALSE(w.CallbackSuccessStatus());
  EXPECT_FALSE(p.IsInitialized());
  EXPECT_FALSE(media::PIPELINE_OK == p.GetError());
  p.Stop();
}
