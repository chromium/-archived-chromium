// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of Pipeline.

#ifndef MEDIA_BASE_PIPELINE_IMPL_H_
#define MEDIA_BASE_PIPELINE_IMPL_H_

#include <string>

#include "media/base/pipeline.h"

namespace media {

class PipelineImpl : public Pipeline {
 public:
  PipelineImpl();
  ~PipelineImpl();

  // Implementation of PipelineStatus methods.
  virtual bool IsInitialized() const;
  virtual int64 GetDuration() const;
  virtual int64 GetBufferedTime() const;
  virtual int64 GetTotalBytes() const;
  virtual int64 GetBufferedBytes()const;
  virtual void GetVideoSize(size_t* width_out, size_t* height_out) const;
  virtual float GetVolume() const;
  virtual float GetPlaybackRate() const;
  virtual int64 GetTime() const;
  virtual PipelineError GetError() const;

  // Impementation of Pipeline methods.
  virtual bool Start(FilterFactory* filter_factory,
                     const std::string& uri,
                     Callback1<bool>::Type* init_complete_callback);
  virtual void Stop();
  virtual bool SetPlaybackRate(float rate);
  virtual bool Seek(int64 time);
  virtual bool SetVolume(float volume);

 private:
  DISALLOW_COPY_AND_ASSIGN(PipelineImpl);
};

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_IMPL_H_
