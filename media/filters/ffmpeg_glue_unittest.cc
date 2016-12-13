// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_ffmpeg.h"
#include "media/base/mock_filters.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;

namespace media {

class FFmpegGlueTest : public ::testing::Test {
 public:
  FFmpegGlueTest() {
    MockFFmpeg::set(&mock_ffmpeg_);
  }

  virtual ~FFmpegGlueTest() {
    MockFFmpeg::set(NULL);
  }

  // Helper to open a URLContext pointing to the given mocked data source.
  // Callers are expected to close the context at the end of their test.
  virtual void OpenContext(MockDataSource* data_source, URLContext* context) {
    // IsSeekable() is called when opening.
    EXPECT_CALL(*data_source, IsSeekable()).WillOnce(Return(false));

    // Add the data source to the glue layer and open a context.
    std::string key = FFmpegGlue::get()->AddDataSource(data_source);
    memset(context, 0, sizeof(*context));
    EXPECT_EQ(0, protocol_->url_open(context, key.c_str(), 0));
    FFmpegGlue::get()->RemoveDataSource(data_source);
  }

 protected:
  // Fixture members.
  MockFFmpeg mock_ffmpeg_;
  static URLProtocol* protocol_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FFmpegGlueTest);
};

URLProtocol* FFmpegGlueTest::protocol_ = NULL;

TEST_F(FFmpegGlueTest, InitializeFFmpeg) {
  // Singleton should initialize FFmpeg.
  FFmpegGlue* glue = FFmpegGlue::get();
  EXPECT_TRUE(glue);

  // Assign our static copy of URLProtocol for the rest of the tests.
  protocol_ = MockFFmpeg::protocol();

  // Make sure URLProtocol was filled out correctly.
  EXPECT_STREQ("http", protocol_->name);
  EXPECT_TRUE(protocol_->url_close);
  EXPECT_TRUE(protocol_->url_open);
  EXPECT_TRUE(protocol_->url_read);
  EXPECT_TRUE(protocol_->url_seek);
  EXPECT_TRUE(protocol_->url_write);
}

TEST_F(FFmpegGlueTest, AddRemoveGetDataSource) {
  // Prepare testing data.
  FFmpegGlue* glue = FFmpegGlue::get();

  // Create our data sources and add them to the glue layer.
  scoped_refptr<StrictMock<Destroyable<MockDataSource> > > data_source_a
      = new StrictMock<Destroyable<MockDataSource> >();
  scoped_refptr<StrictMock<Destroyable<MockDataSource> > > data_source_b
      = new StrictMock<Destroyable<MockDataSource> >();

  // Make sure the keys are unique.
  std::string key_a = glue->AddDataSource(data_source_a);
  std::string key_b = glue->AddDataSource(data_source_b);
  EXPECT_EQ(0u, key_a.find("http://"));
  EXPECT_EQ(0u, key_b.find("http://"));
  EXPECT_NE(key_a, key_b);

  // Our keys should return our data sources.
  scoped_refptr<DataSource> data_source_c;
  scoped_refptr<DataSource> data_source_d;
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

  // Removes the data sources then releases our references.  They should be
  // destroyed.
  InSequence s;
  EXPECT_CALL(*data_source_a, OnDestroy());
  EXPECT_CALL(*data_source_b, OnDestroy());
  EXPECT_CALL(mock_ffmpeg_, CheckPoint(0));

  glue->RemoveDataSource(data_source_a);
  glue->GetDataSource(key_a, &data_source_c);
  EXPECT_FALSE(data_source_c);
  glue->GetDataSource(key_b, &data_source_d);
  EXPECT_EQ(data_source_b, data_source_d);
  glue->RemoveDataSource(data_source_b);
  glue->GetDataSource(key_b, &data_source_d);
  EXPECT_FALSE(data_source_d);
  data_source_a = NULL;
  data_source_b = NULL;

  // Data sources should be deleted by this point.
  mock_ffmpeg_.CheckPoint(0);
}

TEST_F(FFmpegGlueTest, OpenClose) {
  // Prepare testing data.
  FFmpegGlue* glue = FFmpegGlue::get();

  // Create our data source and add them to the glue layer.
  scoped_refptr<StrictMock<Destroyable<MockDataSource> > > data_source
      = new StrictMock<Destroyable<MockDataSource> >();
  EXPECT_CALL(*data_source, IsSeekable()).WillOnce(Return(false));
  std::string key = glue->AddDataSource(data_source);

  // Prepare FFmpeg URLContext structure.
  URLContext context;
  memset(&context, 0, sizeof(context));

  // Test opening a URLContext with a data source that doesn't exist.
  EXPECT_EQ(AVERROR_IO, protocol_->url_open(&context, "foobar", 0));

  // Test opening a URLContext with our data source.
  EXPECT_EQ(0, protocol_->url_open(&context, key.c_str(), 0));
  EXPECT_EQ(URL_RDONLY, context.flags);
  EXPECT_EQ(data_source, context.priv_data);
  EXPECT_TRUE(context.is_streamed);

  // We're going to remove references one by one until the last reference is
  // held by FFmpeg.  Once we close the URLContext, the data source should be
  // destroyed.
  InSequence s;
  EXPECT_CALL(mock_ffmpeg_, CheckPoint(0));
  EXPECT_CALL(mock_ffmpeg_, CheckPoint(1));
  EXPECT_CALL(*data_source, OnDestroy());
  EXPECT_CALL(mock_ffmpeg_, CheckPoint(2));

  // Remove the data source from the glue layer, releasing a reference.
  glue->RemoveDataSource(data_source);
  mock_ffmpeg_.CheckPoint(0);

  // Remove our own reference -- URLContext should maintain a reference.
  data_source = NULL;
  mock_ffmpeg_.CheckPoint(1);

  // Close the URLContext, which should release the final reference.
  EXPECT_EQ(0, protocol_->url_close(&context));
  mock_ffmpeg_.CheckPoint(2);
}

TEST_F(FFmpegGlueTest, Write) {
  scoped_refptr<StrictMock<MockDataSource> > data_source
      = new StrictMock<MockDataSource>();
  URLContext context;
  OpenContext(data_source, &context);

  const int kBufferSize = 16;
  uint8 buffer[kBufferSize];

  // Writing should always fail and never call the data source.
  EXPECT_EQ(AVERROR_IO, protocol_->url_write(&context, NULL, 0));
  EXPECT_EQ(AVERROR_IO, protocol_->url_write(&context, buffer, 0));
  EXPECT_EQ(AVERROR_IO, protocol_->url_write(&context, buffer, kBufferSize));

  // Destroy the data source.
  protocol_->url_close(&context);
}

TEST_F(FFmpegGlueTest, Read) {
  scoped_refptr<StrictMock<MockDataSource> > data_source
      = new StrictMock<MockDataSource>();
  URLContext context;
  OpenContext(data_source, &context);

  const int kBufferSize = 16;
  uint8 buffer[kBufferSize];

  // Reads are for the most part straight-through calls to Read().
  InSequence s;
  EXPECT_CALL(*data_source, Read(buffer, 0))
      .WillOnce(Return(0));
  EXPECT_CALL(*data_source, Read(buffer, kBufferSize))
      .WillOnce(Return(kBufferSize));
  EXPECT_CALL(*data_source, Read(buffer, kBufferSize))
      .WillOnce(Return(DataSource::kReadError));

  EXPECT_EQ(0, protocol_->url_read(&context, buffer, 0));
  EXPECT_EQ(kBufferSize, protocol_->url_read(&context, buffer, kBufferSize));
  EXPECT_EQ(AVERROR_IO, protocol_->url_read(&context, buffer, kBufferSize));

  // Destroy the data source.
  protocol_->url_close(&context);
}

TEST_F(FFmpegGlueTest, Seek) {
  scoped_refptr<StrictMock<MockDataSource> > data_source
      = new StrictMock<MockDataSource>();
  URLContext context;
  OpenContext(data_source, &context);

  // SEEK_SET should be a straight-through call to SetPosition(), which when
  // successful will return the result from GetPosition().
  InSequence s;
  EXPECT_CALL(*data_source, SetPosition(-16))
      .WillOnce(Return(false));

  EXPECT_CALL(*data_source, SetPosition(16))
      .WillOnce(Return(true));
  EXPECT_CALL(*data_source, GetPosition(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(8), Return(true)));

  EXPECT_EQ(AVERROR_IO, protocol_->url_seek(&context, -16, SEEK_SET));
  EXPECT_EQ(8, protocol_->url_seek(&context, 16, SEEK_SET));

  // SEEK_CUR should call GetPosition() first, and if it succeeds add the offset
  // to the result then call SetPosition()+GetPosition().
  EXPECT_CALL(*data_source, GetPosition(_))
      .WillOnce(Return(false));

  EXPECT_CALL(*data_source, GetPosition(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(8), Return(true)));
  EXPECT_CALL(*data_source, SetPosition(16))
      .WillOnce(Return(false));

  EXPECT_CALL(*data_source, GetPosition(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(8), Return(true)));
  EXPECT_CALL(*data_source, SetPosition(16))
      .WillOnce(Return(true));
  EXPECT_CALL(*data_source, GetPosition(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(16), Return(true)));

  EXPECT_EQ(AVERROR_IO, protocol_->url_seek(&context, 8, SEEK_CUR));
  EXPECT_EQ(AVERROR_IO, protocol_->url_seek(&context, 8, SEEK_CUR));
  EXPECT_EQ(16, protocol_->url_seek(&context, 8, SEEK_CUR));

  // SEEK_END should call GetSize() first, and if it succeeds add the offset
  // to the result then call SetPosition()+GetPosition().
  EXPECT_CALL(*data_source, GetSize(_))
      .WillOnce(Return(false));

  EXPECT_CALL(*data_source, GetSize(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(16), Return(true)));
  EXPECT_CALL(*data_source, SetPosition(8))
      .WillOnce(Return(false));

  EXPECT_CALL(*data_source, GetSize(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(16), Return(true)));
  EXPECT_CALL(*data_source, SetPosition(8))
      .WillOnce(Return(true));
  EXPECT_CALL(*data_source, GetPosition(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(8), Return(true)));

  EXPECT_EQ(AVERROR_IO, protocol_->url_seek(&context, -8, SEEK_END));
  EXPECT_EQ(AVERROR_IO, protocol_->url_seek(&context, -8, SEEK_END));
  EXPECT_EQ(8, protocol_->url_seek(&context, -8, SEEK_END));

  // AVSEEK_SIZE should be a straight-through call to GetSize().
  EXPECT_CALL(*data_source, GetSize(_))
      .WillOnce(Return(false));

  EXPECT_CALL(*data_source, GetSize(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(16), Return(true)));

  EXPECT_EQ(AVERROR_IO, protocol_->url_seek(&context, 0, AVSEEK_SIZE));
  EXPECT_EQ(16, protocol_->url_seek(&context, 0, AVSEEK_SIZE));

  // Destroy the data source.
  protocol_->url_close(&context);
}

TEST_F(FFmpegGlueTest, Destroy) {
  // Create our data source and add them to the glue layer.
  scoped_refptr<StrictMock<Destroyable<MockDataSource> > > data_source
      = new StrictMock<Destroyable<MockDataSource> >();
  std::string key = FFmpegGlue::get()->AddDataSource(data_source);

  // We should expect the data source to get destroyed when the unit test
  // exits.
  InSequence s;
  EXPECT_CALL(mock_ffmpeg_, CheckPoint(0));
  EXPECT_CALL(*data_source, OnDestroy());

  // Remove our own reference, we shouldn't be destroyed yet.
  data_source = NULL;
  mock_ffmpeg_.CheckPoint(0);

  // ~FFmpegGlue() will be called when this unit test finishes execution.  By
  // leaving something inside FFmpegGlue's map we get to test our cleanup code.
}

}  // namespace media
