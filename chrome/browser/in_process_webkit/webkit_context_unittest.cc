// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(WebKitContextTest, BasicsTest1) {
  FilePath file_path;
  scoped_refptr<WebKitContext> context = new WebKitContext(file_path, true);
  ASSERT_TRUE(file_path == context->data_path());
  ASSERT_EQ(true, context->is_incognito());
}

TEST(WebKitContextTest, BasicsTest2) {
  FilePath file_path;
  scoped_refptr<WebKitContext> context = new WebKitContext(file_path, false);
  ASSERT_TRUE(file_path == context->data_path());
  ASSERT_EQ(false, context->is_incognito());
}
