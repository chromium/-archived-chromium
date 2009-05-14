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
  explicit MockThemeSource(Profile* profile) : DOMUIThemeSource(profile) { }

  void SendResponse(int request_id, RefCountedBytes* data) {
    result_data_ = data ? data : new RefCountedBytes();
    result_request_id_ = request_id;
  }

  int result_request_id_;
  RefCountedBytes* result_data_;
};

// A mock profile
class MockProfile : public TestingProfile {
 public:
  ThemeProvider* GetThemeProvider() { return new BrowserThemeProvider(); }
};


TEST(DOMUISources, ThemeSourceMimeTypes) {
  MockProfile* profile = new MockProfile();
  DOMUIThemeSource* theme_source = new DOMUIThemeSource(profile);

  EXPECT_EQ(theme_source->GetMimeType("css/newtab.css"), "text/css");
  EXPECT_EQ(theme_source->GetMimeType("css/newtab.css?foo"), "text/css");
  EXPECT_EQ(theme_source->GetMimeType("WRONGURL"), "image/png");
}

TEST(DOMUISources, ThemeSourceImages) {
  MockProfile* profile = new MockProfile();
  MockThemeSource* theme_source = new MockThemeSource(profile);

  // Our test data.
  ThemeProvider* tp = profile->GetThemeProvider();
  SkBitmap* image = tp->GetBitmapNamed(IDR_THEME_FRAME);
  std::vector<unsigned char> png_bytes;
  PNGEncoder::EncodeBGRASkBitmap(*image, false, &png_bytes);
  RefCountedBytes* image_data = new RefCountedBytes(png_bytes);

  theme_source->StartDataRequest("theme_frame", 1);
  EXPECT_EQ(theme_source->result_request_id_, 1);
  EXPECT_EQ(theme_source->result_data_, image_data);

  theme_source->StartDataRequest("theme_toolbar", 2);
  EXPECT_EQ(theme_source->result_request_id_, 2);
  EXPECT_NE(theme_source->result_data_, image_data);
}

TEST(DOMUISources, ThemeSourceCSS) {
  MockProfile* profile = new MockProfile();
  MockThemeSource* theme_source = new MockThemeSource(profile);

  // Generating the test data for the NTP CSS would just involve copying the
  // method, or being super brittle and hard-coding the result (requiring
  // an update to the unittest every time the CSS template changes), so we
  // just check for a successful request.
  theme_source->StartDataRequest("css/newtab.css", 1);
  EXPECT_EQ(theme_source->result_request_id_, 1);
  EXPECT_NE(theme_source->result_data_, new RefCountedBytes());

  theme_source->StartDataRequest("css/newtab.css?pipe", 3);
  EXPECT_EQ(theme_source->result_request_id_, 3);
  EXPECT_NE(theme_source->result_data_, new RefCountedBytes());

  // Check that we send NULL back when we can't find what we're looking for.
  theme_source->StartDataRequest("css/WRONGURL", 7);
  EXPECT_EQ(theme_source->result_request_id_, 7);
  EXPECT_EQ(theme_source->result_data_, new RefCountedBytes());
}
