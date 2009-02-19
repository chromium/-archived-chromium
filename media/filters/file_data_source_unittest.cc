// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/waitable_event.h"
#include "media/base/buffers.h"
#include "media/base/pipeline_impl.h"
#include "media/base/media_format.h"
#include "media/base/filters.h"
#include "media/base/factory.h"
#include "media/base/filter_host.h"
#include "media/base/mock_filter_host.h"
#include "media/filters/file_data_source.h"
#include "media/base/mock_media_filters.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::FileDataSource;
using media::FilterFactoryCollection;
using media::InitializationHelper;
using media::MediaFormat;
using media::MockDemuxer;
using media::MockAudioDecoder;
using media::MockAudioRenderer;
using media::MockFilterConfig;
using media::MockFilterHost;
using media::PipelineImpl;

namespace {

// Returns a path to the test file which contains the string "0123456789"
// without the quotes or any trailing space or null termination.  The file lives
// under the media/test/data directory.  Under Windows, strings for the
// FilePath class are unicode, and the pipeline wants char strings.  Convert
// the string to UTF8 under Windows.  For Mac and Linux, file paths are already
// chars so just return the string from the FilePath.
std::string TestFileURL() {
  FilePath data_dir;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_dir));
  data_dir = data_dir.Append(FILE_PATH_LITERAL("media"))
                     .Append(FILE_PATH_LITERAL("test"))
                     .Append(FILE_PATH_LITERAL("data"))
                     .Append(FILE_PATH_LITERAL("ten_byte_file"));
#if defined (OS_WIN)
  return WideToUTF8(data_dir.value());
#else
  return data_dir.value();
#endif
}

}  // namespace

// Use the "real" pipeline to open the file.
TEST(FileDataSourceTest, OpenFile) {
  PipelineImpl pipeline;
  MockFilterConfig config;
  config.has_video = false;
  scoped_refptr<FilterFactoryCollection> c = new FilterFactoryCollection();
  c->AddFactory(FileDataSource::CreateFactory());
  c->AddFactory(MockDemuxer::CreateFactory(&config));
  c->AddFactory(MockAudioDecoder::CreateFactory(&config));
  c->AddFactory(MockAudioRenderer::CreateFactory(&config));
  InitializationHelper h;
  h.Start(&pipeline, c, TestFileURL());
  h.Wait();
  EXPECT_EQ(pipeline.GetError(), media::PIPELINE_OK);
  EXPECT_EQ(pipeline.GetTotalBytes(), 10);
  EXPECT_EQ(pipeline.GetBufferedBytes(), 10);
  pipeline.Stop();
}

// Use the mock filter host to directly call the Read and GetPosition methods.
TEST(FileDataSourceTest, ReadData) {
  MediaFormat url_format;
  int64 position;
  int64 size;
  char ten_bytes[10];
  std::string url = TestFileURL();
  url_format.SetAsString(MediaFormat::kMimeType, media::mime_type::kURL);
  url_format.SetAsString(MediaFormat::kURL, url);
  MockFilterHost<FileDataSource, std::string> mock_host(&url_format, url);

  EXPECT_TRUE(mock_host.filter()->GetSize(&size));
  EXPECT_EQ(10, size);

  EXPECT_TRUE(mock_host.filter()->GetPosition(&position));
  EXPECT_EQ(0, position);

  EXPECT_EQ(10u, mock_host.filter()->Read(ten_bytes, sizeof(ten_bytes)));
  EXPECT_EQ('0', ten_bytes[0]);
  EXPECT_EQ('5', ten_bytes[5]);
  EXPECT_EQ('9', ten_bytes[9]);
  EXPECT_TRUE(mock_host.filter()->GetPosition(&position));
  EXPECT_EQ(10, position);
  EXPECT_EQ(0u, mock_host.filter()->Read(ten_bytes, sizeof(ten_bytes)));

  EXPECT_TRUE(mock_host.filter()->SetPosition(5));
  EXPECT_EQ(5u, mock_host.filter()->Read(ten_bytes, sizeof(ten_bytes)));
  EXPECT_EQ('5', ten_bytes[0]);
  EXPECT_TRUE(mock_host.filter()->GetPosition(&position));
  EXPECT_EQ(10, position);
}
