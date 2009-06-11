// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_interfaces.h"

namespace media {

AVStreamProvider::AVStreamProvider() {}

AVStreamProvider::~AVStreamProvider() {}

// static
const char* AVStreamProvider::interface_id() {
  return "AVStreamProvider";
}

}  // namespace media
