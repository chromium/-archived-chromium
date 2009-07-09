// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/string_util.h"
#include "media/base/mock_filter_host.h"
#include "media/filters/file_data_source.h"

using ::testing::NiceMock;
using ::testing::StrictMock;

namespace media {

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

// Test that FileDataSource call the appropriate methods on its filter host.
TEST(FileDataSourceTest, OpenFile) {
  StrictMock<MockFilterHost> host;
  EXPECT_CALL(host, SetTotalBytes(10));
  EXPECT_CALL(host, SetBufferedBytes(10));
  EXPECT_CALL(host, InitializationComplete());

  scoped_refptr<FileDataSource> filter = new FileDataSource();
  filter->SetFilterHost(&host);
  EXPECT_TRUE(filter->Initialize(TestFileURL()));
}

// Use the mock filter host to directly call the Read and GetPosition methods.
TEST(FileDataSourceTest, ReadData) {
  int64 position;
  int64 size;
  uint8 ten_bytes[10];

  // Create our mock filter host and initialize the data source.
  NiceMock<MockFilterHost> host;
  scoped_refptr<FileDataSource> filter = new FileDataSource();
  filter->SetFilterHost(&host);
  EXPECT_TRUE(filter->Initialize(TestFileURL()));

  EXPECT_TRUE(filter->GetSize(&size));
  EXPECT_EQ(10, size);

  EXPECT_TRUE(filter->GetPosition(&position));
  EXPECT_EQ(0, position);

  EXPECT_EQ(10u, filter->Read(ten_bytes, sizeof(ten_bytes)));
  EXPECT_EQ('0', ten_bytes[0]);
  EXPECT_EQ('5', ten_bytes[5]);
  EXPECT_EQ('9', ten_bytes[9]);
  EXPECT_TRUE(filter->GetPosition(&position));
  EXPECT_EQ(10, position);
  EXPECT_EQ(0u, filter->Read(ten_bytes, sizeof(ten_bytes)));

  EXPECT_TRUE(filter->SetPosition(5));
  EXPECT_EQ(5u, filter->Read(ten_bytes, sizeof(ten_bytes)));
  EXPECT_EQ('5', ten_bytes[0]);
  EXPECT_TRUE(filter->GetPosition(&position));
  EXPECT_EQ(10, position);
}

}  // namespace media
