// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test of FieldTrial class

#include "base/field_trial.h"

#include "base/string_util.h"
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
  const char* name1 = "name 1 test";
  const char* name2 = "name 2 test";
  EXPECT_FALSE(FieldTrialList::Find(name1));
  EXPECT_FALSE(FieldTrialList::Find(name2));

  FieldTrial* trial1 = new FieldTrial(name1, 10);
  EXPECT_EQ(trial1->group(), FieldTrial::kNotParticipating);
  EXPECT_EQ(trial1->name(), name1);
  EXPECT_EQ(trial1->group_name(), "");

  trial1->AppendGroup("", 7);

  EXPECT_EQ(trial1, FieldTrialList::Find(name1));
  EXPECT_FALSE(FieldTrialList::Find(name2));

  FieldTrial* trial2 = new FieldTrial(name2, 10);
  EXPECT_EQ(trial2->group(), FieldTrial::kNotParticipating);
  EXPECT_EQ(trial2->name(), name2);
  EXPECT_EQ(trial2->group_name(), "");

  trial2->AppendGroup("a first group", 7);

  EXPECT_EQ(trial1, FieldTrialList::Find(name1));
  EXPECT_EQ(trial2, FieldTrialList::Find(name2));
  // Note: FieldTrialList should delete the objects at shutdown.
}

TEST_F(FieldTrialTest, AbsoluteProbabilities) {
  char always_true[] = " always true";
  char always_false[] = " always false";
  for (int i = 1; i < 250; ++i) {
    // Try lots of names, by changing the first character of the name.
    always_true[0] = i;
    always_false[0] = i;

    FieldTrial* trial_true = new FieldTrial(always_true, 10);
    const std::string winner = "_TheWinner";
    int winner_group = trial_true->AppendGroup(winner, 10);

    EXPECT_EQ(trial_true->group(), winner_group);
    EXPECT_EQ(trial_true->group_name(), winner);

    FieldTrial* trial_false = new FieldTrial(always_false, 10);
    int loser_group = trial_false->AppendGroup("ALoser", 0);

    EXPECT_NE(trial_false->group(), loser_group);
  }
}

TEST_F(FieldTrialTest, RemainingProbability) {
  // First create a test that hasn't had a winner yet.
  const std::string winner = "Winner";
  const std::string loser = "Loser";
  scoped_refptr<FieldTrial> trial;
  int counter = 0;
  do {
    std::string name = StringPrintf("trial%d", ++counter);
    trial = new FieldTrial(name, 10);
    trial->AppendGroup(loser, 5);  // 50% chance of not being chosen.
  } while (trial->group() != FieldTrial::kNotParticipating);

  // Now add a winner with all remaining probability.
  trial->AppendGroup(winner, FieldTrial::kAllRemainingProbability);

  // And that winner should ALWAYS win.
  EXPECT_EQ(winner, trial->group_name());
}

TEST_F(FieldTrialTest, MiddleProbabilities) {
  char name[] = " same name";
  bool false_event_seen = false;
  bool true_event_seen = false;
  for (int i = 1; i < 250; ++i) {
    name[0] = i;
    FieldTrial* trial = new FieldTrial(name, 10);
    int might_win = trial->AppendGroup("MightWin", 5);

    if (trial->group() == might_win) {
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

TEST_F(FieldTrialTest, OneWinner) {
  char name[] = "Some name";
  int group_count(10);

  FieldTrial* trial = new FieldTrial(name, group_count);
  int winner_index(-2);
  std::string winner_name;

  for (int i = 1; i <= group_count; ++i) {
    int might_win = trial->AppendGroup("", 1);

    if (trial->group() == might_win) {
      EXPECT_EQ(winner_index, -2);
      winner_index = might_win;
      StringAppendF(&winner_name, "_%d", might_win);
      EXPECT_EQ(winner_name, trial->group_name());
    }
  }
  EXPECT_GE(winner_index, 0);
  EXPECT_EQ(trial->group(), winner_index);
  EXPECT_EQ(winner_name, trial->group_name());
}

TEST_F(FieldTrialTest, Save) {
  std::string save_string;

  FieldTrial* trial = new FieldTrial("Some name", 10);
  // There is no winner yet, so no textual group name is associated with trial.
  EXPECT_EQ(trial->group_name(), "");
  FieldTrialList::StatesToString(&save_string);
  EXPECT_EQ(save_string, "");
  save_string.clear();

  // Create a winning group.
  trial->AppendGroup("Winner", 10);
  FieldTrialList::StatesToString(&save_string);
  EXPECT_EQ(save_string, "Some name/Winner/");
  save_string.clear();

  // Create a second trial and winning group.
  FieldTrial* trial2 = new FieldTrial("xxx", 10);
  trial2->AppendGroup("yyyy", 10);

  FieldTrialList::StatesToString(&save_string);
  // We assume names are alphabetized... though this is not critical.
  EXPECT_EQ(save_string, "Some name/Winner/xxx/yyyy/");
}

TEST_F(FieldTrialTest, Restore) {
  EXPECT_EQ(NULL, FieldTrialList::Find("Some_name"));
  EXPECT_EQ(NULL, FieldTrialList::Find("xxx"));

  FieldTrialList::StringAugmentsState("Some_name/Winner/xxx/yyyy/");

  FieldTrial* trial = FieldTrialList::Find("Some_name");
  ASSERT_NE(static_cast<FieldTrial*>(NULL), trial);
  EXPECT_EQ(trial->group_name(), "Winner");
  EXPECT_EQ(trial->name(), "Some_name");

  trial = FieldTrialList::Find("xxx");
  ASSERT_NE(static_cast<FieldTrial*>(NULL), trial);
  EXPECT_EQ(trial->group_name(), "yyyy");
  EXPECT_EQ(trial->name(), "xxx");
}

TEST_F(FieldTrialTest, BogusRestore) {
  EXPECT_FALSE(FieldTrialList::StringAugmentsState("MissingSlash"));
  EXPECT_FALSE(FieldTrialList::StringAugmentsState("MissingGroupName/"));
  EXPECT_FALSE(FieldTrialList::StringAugmentsState("MissingFinalSlash/gname"));
  EXPECT_FALSE(FieldTrialList::StringAugmentsState("/noname, only group/"));
}

TEST_F(FieldTrialTest, DuplicateRestore) {
  FieldTrial* trial = new FieldTrial("Some name", 10);
  trial->AppendGroup("Winner", 10);
  std::string save_string;
  FieldTrialList::StatesToString(&save_string);
  EXPECT_EQ("Some name/Winner/", save_string);

  // It is OK if we redundantly specify a winner.
  EXPECT_TRUE(FieldTrialList::StringAugmentsState(save_string));

  // But it is an error to try to change to a different winner.
  EXPECT_FALSE(FieldTrialList::StringAugmentsState("Some name/Loser/"));
}
