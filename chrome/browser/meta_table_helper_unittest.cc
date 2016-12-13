// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for MetaTableHelper.

#include "chrome/browser/meta_table_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

class MetaTableHelperTest : public testing::Test {
 public:
  static void appendMetaTableName(const std::string& db_name,
                                  std::string* sql) {
    MetaTableHelper::appendMetaTableName(db_name, sql);
  }
};

TEST_F(MetaTableHelperTest, EmptyDbName) {
  std::string sql("select * from ");
  MetaTableHelperTest::appendMetaTableName(std::string(), &sql);
  EXPECT_EQ("select * from meta", sql);
}

TEST_F(MetaTableHelperTest, NonEmptyDbName) {
  std::string sql("select * from ");
  MetaTableHelperTest::appendMetaTableName(std::string("mydb"), &sql);
  EXPECT_EQ("select * from mydb.meta", sql);
}
