// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of Pipeline.

#ifndef MEDIA_BASE_PIPELINE_IMPL_H_
#define MEDIA_BASE_PIPELINE_IMPL_H_

#include "base/scoped_ptr.h"
#include "media/base/pipeline.h"

namespace media {

class PipelineImpl : public Pipeline {
 public:
  PipelineImpl();

  // Pipeline implementation.
  virtual bool Initialize(FilterFactoryInterface* filter_factory,
                          const std::string& uri);
  virtual bool Play();
  virtual bool Pause();
  virtual bool Seek(int64 seek_position);
  virtual void Shutdown();
  virtual int64 GetTime() const;
  virtual int64 GetDuration() const;
  virtual void SetStateChangedCallback(
      Callback1<PipelineState>::Type* callback);
  virtual void SetTimeChangedCallback(Callback1<int64>::Type* callback);

 protected:
  virtual ~PipelineImpl();

 private:
  int64 time_;
  int64 duration_;
  scoped_ptr<Callback1<PipelineState>::Type> state_changed_callback_;
  scoped_ptr<Callback1<int64>::Type> time_changed_callback_;

  DISALLOW_COPY_AND_ASSIGN(PipelineImpl);
};

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_IMPL_H_
