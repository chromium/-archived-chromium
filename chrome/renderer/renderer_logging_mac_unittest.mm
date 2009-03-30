// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_logging.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

typedef PlatformTest RendererLoggingTest;

namespace {

// Class to mock breakpad's setkeyvalue/clearkeyvalue functions needed for
// SetActiveRendererURLImpl.
// The Keys are stored in a static dictionary and methods are provided to
// verify correctness.
class MockBreakpadKeyValueStore {
 public:
  MockBreakpadKeyValueStore() {
    // Only one of these objects can be active at once.
    DCHECK(dict == NULL);
    dict = [[NSMutableDictionary alloc] init];
  }

  ~MockBreakpadKeyValueStore() {
    // Only one of these objects can be active at once.
    DCHECK(dict != NULL);
    [dict release];
    dict = NULL;
  }

  static void SetKeyValue(NSString* key, NSString* value) {
    DCHECK(dict != NULL);
    [dict setObject:value forKey:key];
  }

  static void ClearKeyValue(NSString *key) {
    DCHECK(dict != NULL);
    [dict removeObjectForKey:key];
  }

  int CountDictionaryEntries() {
    return [dict count];
  }

  bool VerifyDictionaryContents(const std::string &url) {
    using renderer_logging::kMaxNumCrashURLChunks;
    using renderer_logging::kMaxNumURLChunkValueLength;
    using renderer_logging::kUrlChunkFormatStr;

    int num_url_chunks = CountDictionaryEntries();
    EXPECT_TRUE(num_url_chunks <= kMaxNumCrashURLChunks);

    NSString *kUrlChunkFormatStr_utf8 = [NSString
        stringWithUTF8String:kUrlChunkFormatStr];

    NSString *accumulated_url = @"";
    for (int i = 0; i < num_url_chunks; ++i) {
      // URL chunk names are 1-based.
      NSString *key = [NSString stringWithFormat:kUrlChunkFormatStr_utf8, i+1];
      EXPECT_TRUE(key != NULL);
      NSString *value = [dict objectForKey:key];
      EXPECT_TRUE([value length] > 0);
      EXPECT_TRUE([value length] <= (unsigned)kMaxNumURLChunkValueLength);
      accumulated_url = [accumulated_url stringByAppendingString:value];
    }

    NSString *expected_url = [NSString stringWithUTF8String:url.c_str()];
    return([accumulated_url isEqualToString:expected_url]);
  }

 private:
  static NSMutableDictionary* dict;
  DISALLOW_COPY_AND_ASSIGN(MockBreakpadKeyValueStore);
};

}  // namespace

// Call through to SetActiveRendererURLImpl using the functions from
// MockBreakpadKeyValueStore.
void SetActiveRendererURLWithMock(const GURL& url) {
  using renderer_logging::SetActiveRendererURLImpl;

  SetCrashKeyValueFuncPtr setFunc = MockBreakpadKeyValueStore::SetKeyValue;
  ClearCrashKeyValueFuncPtr clearFunc =
      MockBreakpadKeyValueStore::ClearKeyValue;

  SetActiveRendererURLImpl(url, setFunc, clearFunc);
}

TEST_F(RendererLoggingTest, TestUrlSplitting) {
  using renderer_logging::kMaxNumCrashURLChunks;
  using renderer_logging::kMaxNumURLChunkValueLength;

  const std::string short_url("http://abc/");
  std::string long_url("http://");
  std::string overflow_url("http://");

  long_url += std::string(kMaxNumURLChunkValueLength * 2, 'a');
  long_url += "/";

  int max_num_chars_stored_in_dump = kMaxNumURLChunkValueLength *
      kMaxNumCrashURLChunks;
  overflow_url += std::string(max_num_chars_stored_in_dump + 1, 'a');
  overflow_url += "/";

  // Check that Clearing NULL URL works.
  MockBreakpadKeyValueStore mock;
  SetActiveRendererURLWithMock(GURL());
  EXPECT_EQ(mock.CountDictionaryEntries(), 0);

  // Check that we can set a URL.
  SetActiveRendererURLWithMock(GURL(short_url.c_str()));
  EXPECT_TRUE(mock.VerifyDictionaryContents(short_url));
  EXPECT_EQ(mock.CountDictionaryEntries(), 1);
  SetActiveRendererURLWithMock(GURL());
  EXPECT_EQ(mock.CountDictionaryEntries(), 0);

  // Check that we can replace a long url with a short url.
  SetActiveRendererURLWithMock(GURL(long_url.c_str()));
  EXPECT_TRUE(mock.VerifyDictionaryContents(long_url));
  SetActiveRendererURLWithMock(GURL(short_url.c_str()));
  EXPECT_TRUE(mock.VerifyDictionaryContents(short_url));
  SetActiveRendererURLWithMock(GURL());
  EXPECT_EQ(mock.CountDictionaryEntries(), 0);


  // Check that overflow works correctly.
  SetActiveRendererURLWithMock(GURL(overflow_url.c_str()));
  EXPECT_TRUE(mock.VerifyDictionaryContents(
      overflow_url.substr(0, max_num_chars_stored_in_dump)));
  SetActiveRendererURLWithMock(GURL());
  EXPECT_EQ(mock.CountDictionaryEntries(), 0);
}
