// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test of FieldTrial class

#include "base/field_trial.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

class FieldTrialTest : public testing::Test {
 public:
  FieldTrialTest() : trial_list_() { }

 private:
  FieldTrialList trial_list_;
};

// Test registration, and also check that destructors are called for trials
// (and that Purify doesn't catch us leaking).
TEST_F(FieldTrialTest, Registration) {
  const wchar_t* name1 = L"name 1 test";
  const wchar_t* name2 = L"name 2 test";
  EXPECT_FALSE(FieldTrialList::Find(name1));
  EXPECT_FALSE(FieldTrialList::Find(name2));

  FieldTrial* trial1 = new FieldTrial(name1, 0.7);

  EXPECT_EQ(trial1, FieldTrialList::Find(name1));
  EXPECT_FALSE(FieldTrialList::Find(name2));

  FieldTrial* trial2 = new FieldTrial(name2, 0.7);

  EXPECT_EQ(trial1, FieldTrialList::Find(name1));
  EXPECT_EQ(trial2, FieldTrialList::Find(name2));
  // Note: FieldTrialList should delete the objects at shutdown.
}

TEST_F(FieldTrialTest, AbsoluteProbabilities) {
  wchar_t always_true[] = L" always true";
  wchar_t always_false[] = L" always false";
  for (int i = 1; i < 250; ++i) {
    // Try lots of names, by changing the first character of the name.
    always_true[0] = i;
    always_false[0] = i;
    FieldTrial* trial_true = new FieldTrial(always_true, 1.0);
    EXPECT_TRUE(trial_true->boolean_value());
    FieldTrial* trial_false = new FieldTrial(always_false, 0.0);
    EXPECT_FALSE(trial_false->boolean_value());
  }
}

TEST_F(FieldTrialTest, MiddleProbabalities) {
  wchar_t name[] = L" same name";
  bool false_event_seen = false;
  bool true_event_seen = false;
  for (int i = 1; i < 250; ++i) {
    name[0] = i;
    FieldTrial* trial = new FieldTrial(name, 0.5);
    if (trial->boolean_value()) {
      true_event_seen = true;
    } else {
      false_event_seen = true;
    }
    if (false_event_seen && true_event_seen)
      return;  // Successful test!!!
  }
  // Very surprising to get here. Probability should be around 1 in 2 ** 250.
  // One of the following will fail.
  EXPECT_TRUE(false_event_seen);
  EXPECT_TRUE(true_event_seen);
}
