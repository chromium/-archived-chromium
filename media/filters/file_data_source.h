// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FILE_DATA_SOURCE_H_
#define MEDIA_FILTERS_FILE_DATA_SOURCE_H_

#include <string>

#include "base/lock.h"
#include "media/base/filters.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace media {

// Basic data source that treats the URL as a file path, and uses the file
// system to read data for a media pipeline.
class FileDataSource : public DataSource {
 public:
  // Public method to get a filter factory for the FileDataSource.
  static FilterFactory* CreateFactory() {
    return new FilterFactoryImpl0<FileDataSource>();
  }

  // Implementation of MediaFilter.
  virtual void Stop();

  // Implementation of DataSource.
  virtual bool Initialize(const std::string& url);
  virtual const MediaFormat& media_format();
  virtual size_t Read(uint8* data, size_t size);
  virtual bool GetPosition(int64* position_out);
  virtual bool SetPosition(int64 position);
  virtual bool GetSize(int64* size_out);
  virtual bool IsSeekable();

 private:
  // Only allow factories and tests to create this object.
  //
  // TODO(scherkus): I'm getting tired of these factories getting in the way
  // of my tests!!!
  FRIEND_TEST(FileDataSourceTest, OpenFile);
  FRIEND_TEST(FileDataSourceTest, ReadData);
  friend class FilterFactoryImpl0<FileDataSource>;
  FileDataSource();
  virtual ~FileDataSource();

  // File handle.  NULL if not initialized or an error occurs.
  FILE* file_;

  // Size of the file in bytes.
  int64 file_size_;

  // Media format handed out by the DataSource::GetMediaFormat method.
  MediaFormat media_format_;

  // Critical section that protects all of the DataSource methods to prevent
  // a Stop from happening while in the middle of a file I/O operation.
  // TODO(ralphl): Ideally this would use asynchronous I/O or we will know
  // that we will block for a short period of time in reads.  Otherwise, we can
  // hang the pipeline Stop.
  Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(FileDataSource);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FILE_DATA_SOURCE_H_
