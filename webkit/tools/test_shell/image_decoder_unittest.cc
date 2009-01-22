// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/tools/test_shell/image_decoder_unittest.h"

#include "base/file_util.h"
#include "base/md5.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/time.h"

using base::Time;

void ReadFileToVector(const std::wstring& path, Vector<char>* contents) {
  std::string contents_str;
  file_util::ReadFileToString(path, &contents_str);
  contents->resize(contents_str.size());
  memcpy(&contents->first(), contents_str.data(), contents_str.size());
}

std::wstring GetMD5SumPath(const std::wstring& path) {
  static const std::wstring kDecodedDataExtension(L".md5sum");
  return path + kDecodedDataExtension;
}

#ifdef CALCULATE_MD5_SUMS
void SaveMD5Sum(const std::wstring& path, WebCore::RGBA32Buffer* buffer) {
  // Create the file to write.
  ScopedHandle handle(CreateFile(path.c_str(), GENERIC_WRITE, 0,
                                 NULL, CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, NULL));
  ASSERT_TRUE(handle.IsValid());

  // Calculate MD5 sum.
  MD5Digest digest;
  SkAutoLockPixels bmp_lock(buffer->bitmap()); 
  MD5Sum(buffer->bitmap().getPixels(),
         buffer->rect().width() * buffer->rect().height() * sizeof(unsigned),
         &digest);

  // Write sum to disk.
  int bytes_written = file_util::WriteFile(path, &digest, sizeof digest);
  ASSERT_EQ(sizeof digest, bytes_written);
}
#else
void VerifyImage(WebCore::ImageDecoder* decoder,
                 const std::wstring& path,
                 const std::wstring& md5_sum_path) {
  // Make sure decoding can complete successfully.
  EXPECT_TRUE(decoder->isSizeAvailable()) << path;
  WebCore::RGBA32Buffer* image_buffer = decoder->frameBufferAtIndex(0);
  ASSERT_NE(static_cast<WebCore::RGBA32Buffer*>(NULL), image_buffer) << path;
  EXPECT_EQ(WebCore::RGBA32Buffer::FrameComplete, image_buffer->status()) <<
      path;
  EXPECT_FALSE(decoder->failed()) << path;

  // Calculate MD5 sum.
  MD5Digest actual_digest;
  SkAutoLockPixels bmp_lock(image_buffer->bitmap()); 
  MD5Sum(image_buffer->bitmap().getPixels(), image_buffer->rect().width() *
      image_buffer->rect().height() * sizeof(unsigned), &actual_digest);

  // Read the MD5 sum off disk.
  std::string file_bytes;
  file_util::ReadFileToString(md5_sum_path, &file_bytes);
  MD5Digest expected_digest;
  ASSERT_EQ(sizeof expected_digest, file_bytes.size()) << path;
  memcpy(&expected_digest, file_bytes.data(), sizeof expected_digest);

  // Verify that the sums are the same.
  EXPECT_EQ(0, memcmp(&expected_digest, &actual_digest, sizeof(MD5Digest))) <<
      path;
}
#endif

void ImageDecoderTest::SetUp() {
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_dir_));
  file_util::AppendToPath(&data_dir_, L"webkit");
  file_util::AppendToPath(&data_dir_, L"data");
  file_util::AppendToPath(&data_dir_, format_ + L"_decoder");
  ASSERT_TRUE(file_util::PathExists(data_dir_)) << data_dir_;
}

std::vector<std::wstring> ImageDecoderTest::GetImageFiles() const {
  std::wstring pattern = L"*." + format_;

  file_util::FileEnumerator enumerator(FilePath::FromWStringHack(data_dir_),
                                       false,
                                       file_util::FileEnumerator::FILES);

  std::vector<std::wstring> image_files;
  std::wstring next_file_name;
  while ((next_file_name = enumerator.Next().ToWStringHack()) != L"") {
    if (!MatchPattern(next_file_name, pattern)) {
      continue;
    }
    image_files.push_back(next_file_name);
  }

  return image_files;
}

bool ImageDecoderTest::ShouldImageFail(const std::wstring& path) const {
  static const std::wstring kBadSuffix(L".bad.");
  return (path.length() > (kBadSuffix.length() + format_.length()) &&
          !path.compare(path.length() - format_.length() - kBadSuffix.length(),
                        kBadSuffix.length(), kBadSuffix));
}

void ImageDecoderTest::TestDecoding() const {
  const std::vector<std::wstring> image_files(GetImageFiles());
  for (std::vector<std::wstring>::const_iterator i(image_files.begin());
       i != image_files.end(); ++i) {
    Vector<char> image_contents;
    ReadFileToVector(*i, &image_contents);

    scoped_ptr<WebCore::ImageDecoder> decoder(CreateDecoder());
    RefPtr<WebCore::SharedBuffer> shared_contents(WebCore::SharedBuffer::create());
    shared_contents->append(image_contents.data(),
                            static_cast<int>(image_contents.size()));
    decoder->setData(shared_contents.get(), true);

    if (ShouldImageFail(*i)) {
      // We should always get a non-NULL frame buffer, but when the decoder
      // tries to produce it, it should fail, and the frame buffer shouldn't
      // complete.
      WebCore::RGBA32Buffer* const image_buffer =
          decoder->frameBufferAtIndex(0);
      ASSERT_NE(static_cast<WebCore::RGBA32Buffer*>(NULL), image_buffer) <<
          (*i);
      EXPECT_NE(image_buffer->status(), WebCore::RGBA32Buffer::FrameComplete) <<
          (*i);
      EXPECT_TRUE(decoder->failed()) << (*i);
      continue;
    }

#ifdef CALCULATE_MD5_SUMS
    SaveMD5Sum(GetMD5SumPath(*i), decoder->frameBufferAtIndex(0));
#else
    VerifyImage(decoder.get(), *i, GetMD5SumPath(*i));
#endif
  }
}

#ifndef CALCULATE_MD5_SUMS
void ImageDecoderTest::TestChunkedDecoding() const {
  // Init random number generator with current day, so a failing case will fail
  // consistently over the course of a whole day.
  const Time today = Time::Now().LocalMidnight();
  srand(static_cast<unsigned int>(today.ToInternalValue()));

  const std::vector<std::wstring> image_files(GetImageFiles());
  for (std::vector<std::wstring>::const_iterator i(image_files.begin());
       i != image_files.end(); ++i) {
    if (ShouldImageFail(*i))
      continue;

    // Read the file and split it at an arbitrary point.
    Vector<char> image_contents;
    ReadFileToVector(*i, &image_contents);
    const int partial_size = static_cast<int>(
      (static_cast<double>(rand()) / RAND_MAX) * image_contents.size());
    RefPtr<WebCore::SharedBuffer> partial_contents(WebCore::SharedBuffer::create());
    partial_contents->append(image_contents.data(), partial_size);

    // Make sure the image decoder doesn't fail when we ask for the frame buffer
    // for this partial image.
    scoped_ptr<WebCore::ImageDecoder> decoder(CreateDecoder());
    decoder->setData(partial_contents.get(), false);
    EXPECT_NE(static_cast<WebCore::RGBA32Buffer*>(NULL),
              decoder->frameBufferAtIndex(0)) << (*i);
    EXPECT_FALSE(decoder->failed()) << (*i);

    // Make sure passing the complete image results in successful decoding.
    partial_contents->append(
        &image_contents.data()[partial_size],
        static_cast<int>(image_contents.size() - partial_size));
    decoder->setData(partial_contents.get(), true);
    VerifyImage(decoder.get(), *i, GetMD5SumPath(*i));
  }
}
#endif

