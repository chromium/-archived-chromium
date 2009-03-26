// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_common.h"

namespace media {

const char kFFmpegCodecID[] = "FFmpegCodecID";

namespace mime_type {

const char kFFmpegAudio[] = "audio/x-ffmpeg";
const char kFFmpegVideo[] = "video/x-ffmpeg";

}  // namespace mime_type


namespace interface_id {

const char kFFmpegDemuxerStream[] = "FFmpegDemuxerStream";

}  // namespace interface_id

}  // namespace media
