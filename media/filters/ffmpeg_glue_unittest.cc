// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filters.h"
#include "media/base/mock_media_filters.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"
#include "testing/gtest/include/gtest/gtest.h"

// FFmpeg mocks to remove dependency on having the DLLs present.
extern "C" {
static bool g_avcodec_init = false;
static URLProtocol* g_protocol = NULL;
static bool g_av_register_all = false;

void avcodec_init() {
  EXPECT_FALSE(g_avcodec_init);
  g_avcodec_init = true;
}

int register_protocol(URLProtocol* protocol) {
  EXPECT_FALSE(g_protocol);
  g_protocol = protocol;
  return 0;
}

void av_register_all() {
  EXPECT_FALSE(g_av_register_all);
  g_av_register_all = true;
}
}  // extern "C"

TEST(FFmpegGlueTest, InitializeFFmpeg) {
  // Singleton should initialize FFmpeg.
  media::FFmpegGlue* glue = media::FFmpegGlue::get();
  EXPECT_TRUE(glue);
  EXPECT_TRUE(g_avcodec_init);
  EXPECT_TRUE(g_protocol);
  EXPECT_TRUE(g_av_register_all);

  // Make sure URLProtocol was filled out correctly.
  EXPECT_STREQ("http", g_protocol->name);
  EXPECT_TRUE(g_protocol->url_close);
  EXPECT_TRUE(g_protocol->url_open);
  EXPECT_TRUE(g_protocol->url_read);
  EXPECT_TRUE(g_protocol->url_seek);
  EXPECT_TRUE(g_protocol->url_write);
}

TEST(FFmpegGlueTest, AddRemoveGetDataSource) {
  // Prepare testing data.
  media::FFmpegGlue* glue = media::FFmpegGlue::get();

  // Create our data sources and add them to the glue layer.
  bool deleted_a = false;
  bool deleted_b = false;
  media::MockFilterConfig config_a;
  media::MockFilterConfig config_b;
  scoped_refptr<media::MockDataSource> data_source_a
      = new media::MockDataSource(&config_a, &deleted_a);
  scoped_refptr<media::MockDataSource> data_source_b
      = new media::MockDataSource(&config_b, &deleted_b);

  // Make sure the keys are unique.
  std::string key_a = glue->AddDataSource(data_source_a);
  std::string key_b = glue->AddDataSource(data_source_b);
  EXPECT_EQ(0, key_a.find("http://"));
  EXPECT_EQ(0, key_b.find("http://"));
  EXPECT_NE(key_a, key_b);

  // Our keys should return our data sources.
  scoped_refptr<media::DataSource> data_source_c;
  scoped_refptr<media::DataSource> data_source_d;
  glue->GetDataSource(key_a, &data_source_c);
  glue->GetDataSource(key_b, &data_source_d);
  EXPECT_EQ(data_source_a, data_source_c);
  EXPECT_EQ(data_source_b, data_source_d);

  // Adding the same DataSource should create the same key and not add an extra
  // reference.
  std::string key_a2 = glue->AddDataSource(data_source_a);
  EXPECT_EQ(key_a, key_a2);
  glue->GetDataSource(key_a2, &data_source_c);
  EXPECT_EQ(data_source_a, data_source_c);

  // Removes the data sources and then releases our references.  They should be
  // deleted.
  glue->RemoveDataSource(data_source_a);
  glue->GetDataSource(key_a, &data_source_c);
  EXPECT_FALSE(data_source_c);
  glue->GetDataSource(key_b, &data_source_d);
  EXPECT_EQ(data_source_b, data_source_d);
  glue->RemoveDataSource(data_source_b);
  glue->GetDataSource(key_b, &data_source_d);
  EXPECT_FALSE(data_source_d);
  EXPECT_FALSE(deleted_a);
  EXPECT_FALSE(deleted_b);
  data_source_a = NULL;
  data_source_b = NULL;
  EXPECT_TRUE(deleted_a);
  EXPECT_TRUE(deleted_b);
}

TEST(FFmpegGlueTest, OpenClose) {
  // Prepare testing data.
  media::FFmpegGlue* glue = media::FFmpegGlue::get();

  // Create our data source and add them to the glue layer.
  bool deleted = false;
  media::MockFilterConfig config;
  scoped_refptr<media::MockDataSource> data_source
      = new media::MockDataSource(&config, &deleted);
  std::string key = glue->AddDataSource(data_source);

  // Prepare FFmpeg URLContext structure.
  URLContext context;
  memset(&context, 0, sizeof(context));

  // Test opening a URLContext with a data source that doesn't exist.
  EXPECT_EQ(AVERROR_IO, g_protocol->url_open(&context, "foobar", 0));

  // Test opening a URLContext with our data source.
  EXPECT_EQ(0, g_protocol->url_open(&context, key.c_str(), 0));
  EXPECT_EQ(URL_RDONLY, context.flags);
  EXPECT_EQ(data_source, context.priv_data);
  EXPECT_FALSE(context.is_streamed);

  // Remove the data source from the glue layer, releasing a reference.
  glue->RemoveDataSource(data_source);
  EXPECT_FALSE(deleted);

  // Remove our own reference -- URLContext should maintain a reference.
  data_source = NULL;
  EXPECT_FALSE(deleted);

  // Close the URLContext, which should release the final reference.
  EXPECT_EQ(0, g_protocol->url_close(&context));
  EXPECT_TRUE(deleted);
}

TEST(FFmpegGlueTest, ReadingWriting) {
  // Prepare testing data.
  media::FFmpegGlue* glue = media::FFmpegGlue::get();
  const size_t kBufferSize = 16;
  unsigned char buffer[kBufferSize];

  // Configure MockDataSource to be 8 characters long and fill reads with
  // periods.  Therefore our expected string should be a character of 8 periods.
  const size_t kExpectedSize = 8;
  media::MockFilterConfig config;
  config.media_total_bytes = kExpectedSize;
  config.data_source_value = '.';
  const char kExpected[] = "........";
  COMPILE_ASSERT(kExpectedSize == (arraysize(kExpected) - 1), string_length);

  // Create our data source and add them to the glue layer.
  bool deleted = false;
  scoped_refptr<media::MockDataSource> data_source
      = new media::MockDataSource(&config, &deleted);
  std::string key = glue->AddDataSource(data_source);

  // Open our data source and then remove it from the glue layer.
  URLContext context;
  memset(&context, 0, sizeof(context));
  EXPECT_EQ(0, g_protocol->url_open(&context, key.c_str(), 0));
  glue->RemoveDataSource(data_source);
  EXPECT_FALSE(deleted);

  // Writing should always fail.
  EXPECT_EQ(AVERROR_IO, g_protocol->url_write(&context, NULL, 0));
  EXPECT_EQ(AVERROR_IO, g_protocol->url_write(&context, buffer, 0));
  EXPECT_EQ(AVERROR_IO, g_protocol->url_write(&context, buffer, -1));
  EXPECT_EQ(AVERROR_IO, g_protocol->url_write(&context, buffer, kBufferSize));
  EXPECT_EQ(0, data_source->position());

  // Reading should return same amount of bytes if <= kExpectedSize.
  EXPECT_EQ(0, g_protocol->url_read(&context, buffer, 0));
  EXPECT_EQ(kExpectedSize / 2,
            g_protocol->url_read(&context, buffer, kExpectedSize / 2));
  EXPECT_EQ(kExpectedSize,
            g_protocol->url_read(&context, buffer, kExpectedSize));
  buffer[kExpectedSize] = '\0';
  EXPECT_STREQ(kExpected, reinterpret_cast<char*>(buffer));

  // Test reading more than kExpectedSize for simulating EOF.
  EXPECT_EQ(kExpectedSize, g_protocol->url_read(&context, buffer, kBufferSize));
  buffer[kExpectedSize] = '\0';
  EXPECT_STREQ(kExpected, reinterpret_cast<char*>(buffer));

  // Close our data source.
  EXPECT_EQ(0, g_protocol->url_close(&context));
  EXPECT_FALSE(deleted);

  // Remove our own reference, which should release the final reference.
  data_source = NULL;
  EXPECT_TRUE(deleted);
}

TEST(FFmpegGlueTest, Seeking) {
  // Prepare testing data.
  media::FFmpegGlue* glue = media::FFmpegGlue::get();
  const int64 kSize = 32;

  // Create our data source and add them to the glue layer.
  bool deleted = false;
  media::MockFilterConfig config;
  config.media_total_bytes = kSize;
  scoped_refptr<media::MockDataSource> data_source
      = new media::MockDataSource(&config, &deleted);
  std::string key = glue->AddDataSource(data_source);

  // Open our data source and then remove it from the glue layer.
  URLContext context;
  memset(&context, 0, sizeof(context));
  EXPECT_EQ(0, g_protocol->url_open(&context, key.c_str(), 0));
  glue->RemoveDataSource(data_source);
  EXPECT_FALSE(deleted);

  // Test SEEK_SET operations.
  config.media_total_bytes = -1;
  EXPECT_EQ(AVERROR_IO, g_protocol->url_seek(&context, 0, SEEK_SET));

  config.media_total_bytes = kSize;
  EXPECT_TRUE(data_source->SetPosition(0));
  EXPECT_EQ(0, g_protocol->url_seek(&context, 0, SEEK_SET));
  EXPECT_TRUE(data_source->SetPosition(5));
  EXPECT_EQ(0, g_protocol->url_seek(&context, 0, SEEK_SET));
  EXPECT_EQ(0, data_source->position());
  EXPECT_EQ(AVERROR_IO, g_protocol->url_seek(&context, -5, SEEK_SET));
  EXPECT_EQ(0, data_source->position());
  EXPECT_EQ(kSize, g_protocol->url_seek(&context, kSize, SEEK_SET));
  EXPECT_EQ(kSize, data_source->position());
  EXPECT_EQ(AVERROR_IO, g_protocol->url_seek(&context, kSize+1, SEEK_SET));
  EXPECT_EQ(kSize, data_source->position());

  // Test SEEK_CUR operations.
  config.media_total_bytes = -1;
  EXPECT_EQ(AVERROR_IO, g_protocol->url_seek(&context, 0, SEEK_CUR));

  config.media_total_bytes = kSize;
  EXPECT_TRUE(data_source->SetPosition(0));
  EXPECT_EQ(0, g_protocol->url_seek(&context, 0, SEEK_CUR));
  EXPECT_TRUE(data_source->SetPosition(5));
  EXPECT_EQ(5, g_protocol->url_seek(&context, 0, SEEK_CUR));
  EXPECT_EQ(0, g_protocol->url_seek(&context, -5, SEEK_CUR));
  EXPECT_EQ(0, data_source->position());
  EXPECT_EQ(AVERROR_IO, g_protocol->url_seek(&context, -1, SEEK_CUR));
  EXPECT_EQ(kSize, g_protocol->url_seek(&context, kSize, SEEK_CUR));
  EXPECT_EQ(kSize, data_source->position());
  EXPECT_EQ(AVERROR_IO, g_protocol->url_seek(&context, 1, SEEK_CUR));
  EXPECT_EQ(kSize, data_source->position());

  // Test SEEK_END operations.
  config.media_total_bytes = -1;
  EXPECT_EQ(AVERROR_IO, g_protocol->url_seek(&context, 0, SEEK_END));

  config.media_total_bytes = kSize;
  EXPECT_TRUE(data_source->SetPosition(0));
  EXPECT_EQ(kSize, g_protocol->url_seek(&context, 0, SEEK_END));
  EXPECT_EQ(kSize, data_source->position());
  EXPECT_EQ(kSize-5, g_protocol->url_seek(&context, -5, SEEK_END));
  EXPECT_EQ(kSize-5, data_source->position());
  EXPECT_EQ(0, g_protocol->url_seek(&context, -kSize, SEEK_END));
  EXPECT_EQ(0, data_source->position());
  EXPECT_EQ(AVERROR_IO, g_protocol->url_seek(&context, 1, SEEK_END));
  EXPECT_EQ(0, data_source->position());

  // Test AVSEEK_SIZE operation.
  config.media_total_bytes = -1;
  EXPECT_EQ(AVERROR_IO, g_protocol->url_seek(&context, 0, AVSEEK_SIZE));

  config.media_total_bytes = kSize;
  EXPECT_TRUE(data_source->SetPosition(0));
  EXPECT_EQ(kSize, g_protocol->url_seek(&context, 0, AVSEEK_SIZE));

  // Close our data source.
  EXPECT_EQ(0, g_protocol->url_close(&context));
  EXPECT_FALSE(deleted);

  // Remove our own reference, which should release the final reference.
  data_source = NULL;
  EXPECT_TRUE(deleted);
}

TEST(FFmpegGlueTest, Destructor) {
  // Prepare testing data.
  media::FFmpegGlue* glue = media::FFmpegGlue::get();

  // We use a static bool since ~FFmpegGlue() will set it to true sometime
  // after this function exits.
  static bool deleted = false;

  // Create our data source and add them to the glue layer.
  media::MockFilterConfig config;
  scoped_refptr<media::MockDataSource> data_source
      = new media::MockDataSource(&config, &deleted);
  std::string key = glue->AddDataSource(data_source);

  // Remove our own reference.
  data_source = NULL;
  EXPECT_FALSE(deleted);

  // ~FFmpegGlue() will be called when this unit test finishes execution.  By
  // leaving something inside FFmpegGlue's map we get to test our cleanup code.
  //
  // MockDataSource will be holding onto a bad MockFilterConfig pointer at this
  // point but since no one is calling it everything will be ok.
}
