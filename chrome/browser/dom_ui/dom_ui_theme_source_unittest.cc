// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/png_encoder.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/dom_ui/dom_ui_theme_source.h"
#include "chrome/browser/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/testing_profile.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

// A mock ThemeSource (so we can override SendResponse to get at its data).
class MockThemeSource : public DOMUIThemeSource {
 public:
  explicit MockThemeSource(Profile* profile)
      : DOMUIThemeSource(profile),
        result_request_id_(-1),
        result_data_size_(0) { 
  }

  virtual void SendResponse(int request_id, RefCountedBytes* data) {
    result_data_size_ = data ? data->data.size() : 0;
    result_request_id_ = request_id;
  }

  int result_request_id_;
  size_t result_data_size_;
};

class DOMUISourcesTest : public testing::Test {
 public:
  TestingProfile* profile() const { return profile_.get(); }
  MockThemeSource* theme_source() const { return theme_source_.get(); }
 private:
  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    profile_.get()->CreateThemeProvider();
    theme_source_ = new MockThemeSource(profile_.get());
  }

  virtual void TearDown() {
    theme_source_ = NULL;
    profile_.reset(NULL);
  }

  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<MockThemeSource> theme_source_;
};

TEST_F(DOMUISourcesTest, ThemeSourceMimeTypes) {
  EXPECT_EQ(theme_source()->GetMimeType("css/newtab.css"), "text/css");
  EXPECT_EQ(theme_source()->GetMimeType("css/newtab.css?foo"), "text/css");
  EXPECT_EQ(theme_source()->GetMimeType("WRONGURL"), "image/png");
}

TEST_F(DOMUISourcesTest, ThemeSourceImages) {
  // Our test data. Rather than comparing the data itself, we just compare
  // its size.
  SkBitmap* image = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_THEME_FRAME_INCOGNITO);
  std::vector<unsigned char> png_bytes;
  PNGEncoder::EncodeBGRASkBitmap(*image, false, &png_bytes);

  theme_source()->StartDataRequest("theme_frame_incognito", 1);
  EXPECT_EQ(theme_source()->result_request_id_, 1);
  EXPECT_EQ(theme_source()->result_data_size_, png_bytes.size());

  theme_source()->StartDataRequest("theme_toolbar", 2);
  EXPECT_EQ(theme_source()->result_request_id_, 2);
  EXPECT_NE(theme_source()->result_data_size_, png_bytes.size());
}

TEST_F(DOMUISourcesTest, ThemeSourceCSS) {
  // Generating the test data for the NTP CSS would just involve copying the
  // method, or being super brittle and hard-coding the result (requiring
  // an update to the unittest every time the CSS template changes), so we
  // just check for a successful request and data that is non-null.
  size_t empty_size = 0;

  theme_source()->StartDataRequest("css/newtab.css", 1);
  EXPECT_EQ(theme_source()->result_request_id_, 1);
  EXPECT_NE(theme_source()->result_data_size_, empty_size);

  theme_source()->StartDataRequest("css/newtab.css?pie", 3);
  EXPECT_EQ(theme_source()->result_request_id_, 3);
  EXPECT_NE(theme_source()->result_data_size_, empty_size);

  // Check that we send NULL back when we can't find what we're looking for.
  theme_source()->StartDataRequest("css/WRONGURL", 7);
  EXPECT_EQ(theme_source()->result_request_id_, 7);
  EXPECT_EQ(theme_source()->result_data_size_, empty_size);
}
