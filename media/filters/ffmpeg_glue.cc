// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "media/base/filters.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"

namespace {

// FFmpeg protocol interface.
int OpenContext(URLContext* h, const char* filename, int flags) {
  scoped_refptr<media::DataSource> data_source;
  media::FFmpegGlue::get()->GetDataSource(filename, &data_source);
  if (!data_source)
    return AVERROR_IO;

  data_source->AddRef();
  h->priv_data = data_source;
  h->flags = URL_RDONLY;
  // TODO(scherkus): data source should be able to tell us if we're streaming.
  h->is_streamed = false;
  return 0;
}

int ReadContext(URLContext* h, unsigned char* buf, int size) {
  media::DataSource* data_source =
      reinterpret_cast<media::DataSource*>(h->priv_data);
  int result = data_source->Read(buf, size);
  if (result < 0)
    result = AVERROR_IO;
  return result;
}

int WriteContext(URLContext* h, unsigned char* buf, int size) {
  // We don't support writing.
  return AVERROR_IO;
}

offset_t SeekContext(URLContext* h, offset_t offset, int whence) {
  media::DataSource* data_source =
      reinterpret_cast<media::DataSource*>(h->priv_data);
  offset_t new_offset = AVERROR_IO;
  switch (whence) {
    case SEEK_SET:
      if (data_source->SetPosition(offset))
        data_source->GetPosition(&new_offset);
      break;

    case SEEK_CUR:
      int64 pos;
      if (!data_source->GetPosition(&pos))
        break;
      if (data_source->SetPosition(pos + offset))
        data_source->GetPosition(&new_offset);
      break;

    case SEEK_END:
      int64 size;
      if (!data_source->GetSize(&size))
        break;
      if (data_source->SetPosition(size + offset))
        data_source->GetPosition(&new_offset);
      break;

    case AVSEEK_SIZE:
      data_source->GetSize(&new_offset);
      break;

    default:
      NOTREACHED();
  }
  if (new_offset < 0)
    new_offset = AVERROR_IO;
  return new_offset;
}

int CloseContext(URLContext* h) {
  media::DataSource* data_source =
      reinterpret_cast<media::DataSource*>(h->priv_data);
  data_source->Release();
  h->priv_data = NULL;
  return 0;
}

}  // namespace

//------------------------------------------------------------------------------

namespace media {

// Use the HTTP protocol to avoid any file path separator issues.
static const char kProtocol[] = "http";

// Fill out our FFmpeg protocol definition.
static URLProtocol kFFmpegProtocol = {
  kProtocol,
  &OpenContext,
  &ReadContext,
  &WriteContext,
  &SeekContext,
  &CloseContext,
};

FFmpegGlue::FFmpegGlue() {
  // Register our protocol glue code with FFmpeg.
  avcodec_init();
  register_protocol(&kFFmpegProtocol);

  // Now register the rest of FFmpeg.
  av_register_all();
}

FFmpegGlue::~FFmpegGlue() {
}

std::string FFmpegGlue::AddDataSource(DataSource* data_source) {
  AutoLock auto_lock(lock_);
  std::string key = GetDataSourceKey(data_source);
  if (data_sources_.find(key) == data_sources_.end()) {
    data_sources_[key] = data_source;
  }
  return key;
}

void FFmpegGlue::RemoveDataSource(DataSource* data_source) {
  AutoLock auto_lock(lock_);
  for (DataSourceMap::iterator cur, iter = data_sources_.begin();
       iter != data_sources_.end();) {
    cur = iter;
    iter++;

    if (cur->second == data_source)
      data_sources_.erase(cur);
  }
}

void FFmpegGlue::GetDataSource(const std::string& key,
                               scoped_refptr<DataSource>* data_source) {
  AutoLock auto_lock(lock_);
  DataSourceMap::iterator iter = data_sources_.find(key);
  if (iter == data_sources_.end()) {
    *data_source = NULL;
    return;
  }
  *data_source = iter->second;
}

std::string FFmpegGlue::GetDataSourceKey(DataSource* data_source) {
  // Use the DataSource's memory address to generate the unique string.  This
  // also has the nice property that adding the same DataSource reference will
  // not generate duplicate entries.
  return StringPrintf("%s://0x%lx", kProtocol, static_cast<void*>(data_source));
}

}  // namespace media
