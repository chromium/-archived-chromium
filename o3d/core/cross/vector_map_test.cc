/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains tests for vector_map.

#include <map>
#include "tests/common/win/testing_common.h"
#include "core/cross/vector_map.h"

namespace o3d {

// Tests basic map behavior.
TEST(VectorMapTest, General) {
  vector_map<int, int> the_map;

  EXPECT_TRUE(the_map.empty());

  the_map[0] = 5;

  EXPECT_FALSE(the_map.empty());
  EXPECT_EQ(the_map.size(), 1);

  the_map[9] = 2;

  EXPECT_FALSE(the_map.empty());
  EXPECT_EQ(the_map.size(), 2);

  EXPECT_EQ(the_map[9], 2);
  EXPECT_EQ(the_map[0], 5);

  vector_map<int, int>::iterator iter(the_map.begin());
  ASSERT_TRUE(iter != the_map.end());
  EXPECT_EQ(iter->first, 0);
  EXPECT_EQ(iter->second, 5);
  ++iter;
  ASSERT_TRUE(iter != the_map.end());
  EXPECT_EQ((*iter).first, 9);
  EXPECT_EQ((*iter).second, 2);
  ++iter;
  EXPECT_TRUE(iter == the_map.end());

  the_map[8] = 23;
  the_map[1234] = 90;
  the_map[-5] = 6;

  EXPECT_EQ(the_map[   9],  2);
  EXPECT_EQ(the_map[   0],  5);
  EXPECT_EQ(the_map[1234], 90);
  EXPECT_EQ(the_map[   8], 23);
  EXPECT_EQ(the_map[  -5],  6);
  EXPECT_EQ(the_map.size(), 5);
  EXPECT_FALSE(the_map.empty());

  iter = the_map.begin();
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(iter != the_map.end());
    ++iter;
  }
  EXPECT_TRUE(iter == the_map.end());

  const vector_map<int, int>& ref = the_map;
  EXPECT_TRUE(ref.find(1234) != the_map.end());
  EXPECT_TRUE(ref.find(5678) == the_map.end());
}

// Tests various insertion cases.
TEST(VectorMapTest, Insert) {
  vector_map<int, int> the_map;

  for (int i = 1; i <= 10; ++i) {
    // insert an element
    std::pair<vector_map<int, int>::iterator, bool> ret;
    ret = the_map.insert(std::make_pair(i, 100*i));
    EXPECT_TRUE(ret.second);
    EXPECT_TRUE(ret.first == the_map.find(i));
    EXPECT_EQ(ret.first->first, i);
    EXPECT_EQ(ret.first->second, 100*i);

    // try to insert it again with different value, fails, but we still get an
    // iterator back with the original value.
    ret = the_map.insert(std::make_pair(i, -i));
    EXPECT_FALSE(ret.second);
    EXPECT_TRUE(ret.first == the_map.find(i));
    EXPECT_EQ(ret.first->first, i);
    EXPECT_EQ(ret.first->second, 100*i);

    // check the state of the map.
    for (int j = 1; j <= i; ++j) {
      vector_map<int, int>::iterator it = the_map.find(j);
      EXPECT_TRUE(it != the_map.end());
      EXPECT_EQ(it->first, j);
      EXPECT_EQ(it->second, j * 100);
    }
    EXPECT_EQ(the_map.size(), i);
    EXPECT_FALSE(the_map.empty());
  }
}

// Tests insertion of a range of values.
TEST(VectorMapTest, InsertRange) {
  for (int elements = 0; elements <= 10; ++elements) {
    std::map<int, int> normal_map;
    for (int i = 1; i <= elements; ++i) {
      normal_map.insert(std::make_pair(i, 100*i));
    }

    vector_map<int, int> the_map;
    the_map.insert(normal_map.begin(), normal_map.end());
    EXPECT_EQ(normal_map.size(), the_map.size());
    for (int i = 1; i <= elements; ++i) {
      EXPECT_TRUE(the_map.find(i) != the_map.end());
      EXPECT_EQ(the_map.find(i)->first, i);
      EXPECT_EQ(the_map.find(i)->second, 100*i);
    }
  }
}

// Insert a new key and value into a map or hash_map.
// If the key is not present in the map the key and value are
// inserted, otherwise nothing happens. True indicates that an insert
// took place, false indicates the key was already present.
template <class Collection, class Key, class Value>
bool InsertIfNotPresent(Collection * const collection,
                        const Key& key, const Value& value) {
  std::pair<typename Collection::iterator, bool> ret =
      collection->insert(typename Collection::value_type(key, value));
  return ret.second;
}

// Tests InsertIfNotPresent map utility function.
TEST(VectorMapTest, InsertIfNotPresent) {
  vector_map<int, int> the_map;

  for (int i = 1; i <= 10; ++i) {
    EXPECT_TRUE(InsertIfNotPresent(&the_map, i, 100*i));
    EXPECT_FALSE(InsertIfNotPresent(&the_map, i, -i));

    // check the state of the map.
    for (int j = 1; j <= i; ++j) {
      vector_map<int, int>::iterator it = the_map.find(j);
      ASSERT_TRUE(it != the_map.end());
      EXPECT_EQ(it->first, j);
      EXPECT_EQ(it->second, j * 100);
    }
    EXPECT_EQ(the_map.size(), i);
    EXPECT_FALSE(the_map.empty());
  }
}

// Tests various erase cases.
TEST(VectorMapTest, Erase) {
  vector_map<std::string, int> the_map;
  vector_map<std::string, int>::iterator iter, iter1, iter2;

  the_map["monday"] = 1;
  the_map["tuesday"] = 2;
  the_map["wednesday"] = 3;

  EXPECT_EQ(the_map["monday"   ], 1);
  EXPECT_EQ(the_map["tuesday"  ], 2);
  EXPECT_EQ(the_map["wednesday"], 3);
  EXPECT_EQ(the_map.count("tuesday"), 1);

  iter = the_map.begin();
  ASSERT_TRUE(iter != the_map.end());
  EXPECT_EQ(iter->first, "monday");
  EXPECT_EQ(iter->second, 1);
  // Use the occasion to check that post-increment works correctly.
  iter1 = iter;
  iter2 = iter++;
  EXPECT_TRUE(iter1 == iter2);
  ASSERT_TRUE(iter != the_map.end());
  EXPECT_EQ(iter->first, "tuesday");
  EXPECT_EQ(iter->second, 2);
  iter1 = iter;
  iter2 = iter++;
  EXPECT_TRUE(iter1 == iter2);
  ASSERT_TRUE(iter != the_map.end());
  EXPECT_EQ(iter->first, "wednesday");
  EXPECT_EQ(iter->second, 3);
  iter1 = iter;
  iter2 = iter++;
  EXPECT_TRUE(iter1 == iter2);
  EXPECT_TRUE(iter == the_map.end());

  EXPECT_EQ(the_map.erase("tuesday"), 1);

  EXPECT_EQ(the_map["monday"   ], 1);
  EXPECT_EQ(the_map["wednesday"], 3);
  EXPECT_EQ(the_map.count("tuesday"), 0);
  EXPECT_EQ(the_map.erase("tuesday"), 0);

  iter = the_map.begin();
  ASSERT_TRUE(iter != the_map.end());
  EXPECT_EQ(iter->first, "monday");
  EXPECT_EQ(iter->second, 1);
  ++iter;
  ASSERT_TRUE(iter != the_map.end());
  EXPECT_EQ(iter->first, "wednesday");
  EXPECT_EQ(iter->second, 3);
  iter1 = iter;
  iter2 = iter++;
  EXPECT_TRUE(iter1 == iter2);
  EXPECT_TRUE(iter == the_map.end());

  the_map["thursday"] = 4;
  the_map["friday"] = 5;
  EXPECT_EQ(the_map.size(), 4);
  EXPECT_FALSE(the_map.empty());

  the_map["saturday"] = 6;

  EXPECT_EQ(the_map.count("friday"), 1);
  EXPECT_EQ(the_map.erase("friday"), 1);
  EXPECT_EQ(the_map.count("friday"), 0);
  EXPECT_EQ(the_map.erase("friday"), 0);

  EXPECT_EQ(the_map.size(), 4);
  EXPECT_FALSE(the_map.empty());
  EXPECT_EQ(the_map.erase("monday"), 1);
  EXPECT_EQ(the_map.size(), 3);
  EXPECT_FALSE(the_map.empty());

  the_map.clear();
  EXPECT_EQ(the_map.size(), 0);
  EXPECT_TRUE(the_map.empty());
}

}  // namespace o3d
