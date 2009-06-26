// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/webcursor.h"
#include "webkit/tools/test_shell/test_shell_test.h"

TEST(WebCursorTest, CursorSerialization) {
  WebCursor custom_cursor;
  // This is a valid custom cursor.
  Pickle ok_custom_pickle;
  // Type and hotspots.
  ok_custom_pickle.WriteInt(0);
  ok_custom_pickle.WriteInt(0);
  ok_custom_pickle.WriteInt(0);
  // X & Y
  ok_custom_pickle.WriteInt(1);
  ok_custom_pickle.WriteInt(1);
  // Data len including enough data for a 1x1 image.
  ok_custom_pickle.WriteInt(4);
  ok_custom_pickle.WriteUInt32(0);
  // Custom Windows message.
  ok_custom_pickle.WriteIntPtr(NULL);
  void* iter = NULL;
  EXPECT_TRUE(custom_cursor.Deserialize(&ok_custom_pickle, &iter));

  // This custom cursor has not been send with enough data.
  Pickle short_custom_pickle;
  // Type and hotspots.
  short_custom_pickle.WriteInt(0);
  short_custom_pickle.WriteInt(0);
  short_custom_pickle.WriteInt(0);
  // X & Y
  short_custom_pickle.WriteInt(1);
  short_custom_pickle.WriteInt(1);
  // Data len not including enough data for a 1x1 image.
  short_custom_pickle.WriteInt(3);
  short_custom_pickle.WriteUInt32(0);
  // Custom Windows message.
  ok_custom_pickle.WriteIntPtr(NULL);
  iter = NULL;
  EXPECT_FALSE(custom_cursor.Deserialize(&short_custom_pickle, &iter));

  // This custom cursor has enough data but is too big.
  Pickle large_custom_pickle;
  // Type and hotspots.
  large_custom_pickle.WriteInt(0);
  large_custom_pickle.WriteInt(0);
  large_custom_pickle.WriteInt(0);
  // X & Y
  static const int kTooBigSize = 4096 + 1;
  large_custom_pickle.WriteInt(kTooBigSize);
  large_custom_pickle.WriteInt(1);
  // Data len including enough data for a 4097x1 image.
  large_custom_pickle.WriteInt(kTooBigSize * 4);
  for (int i = 0; i < kTooBigSize; ++i)
    large_custom_pickle.WriteUInt32(0);
  // Custom Windows message.
  ok_custom_pickle.WriteIntPtr(NULL);
  iter = NULL;
  EXPECT_FALSE(custom_cursor.Deserialize(&large_custom_pickle, &iter));

  // This custom cursor uses negative lengths.
  Pickle neg_custom_pickle;
  // Type and hotspots.
  neg_custom_pickle.WriteInt(0);
  neg_custom_pickle.WriteInt(0);
  neg_custom_pickle.WriteInt(0);
  // X & Y
  neg_custom_pickle.WriteInt(-1);
  neg_custom_pickle.WriteInt(-1);
  // Data len including enough data for a 1x1 image.
  neg_custom_pickle.WriteInt(4);
  neg_custom_pickle.WriteUInt32(0);
  // Custom Windows message.
  neg_custom_pickle.WriteIntPtr(NULL);
  iter = NULL;
  EXPECT_FALSE(custom_cursor.Deserialize(&neg_custom_pickle, &iter));
}

