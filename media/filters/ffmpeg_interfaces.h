// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_INTERFACES_H_
#define MEDIA_FILTERS_FFMPEG_INTERFACES_H_

#include "base/basictypes.h"

struct AVStream;

namespace media {

// Queryable interface that provides an initialized AVStream object suitable for
// decoding.
class AVStreamProvider {
 public:
  AVStreamProvider();
  virtual ~AVStreamProvider();

  // AVStreamProvider interface.
  virtual AVStream* GetAVStream() = 0;

  // Required by QueryInterface.
  static const char* interface_id();

 private:
  DISALLOW_COPY_AND_ASSIGN(AVStreamProvider);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_INTERFACES_H_
