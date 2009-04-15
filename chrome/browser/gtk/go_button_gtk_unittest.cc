// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/go_button_gtk.h"
#include "base/task.h"
#include "testing/gtest/include/gtest/gtest.h"

class GoButtonGtkPeer {
 public:
  explicit GoButtonGtkPeer(GoButtonGtk* go) : go_(go) { }

  // const accessors for internal state
  GoButtonGtk::Mode intended_mode() const { return go_->intended_mode_; }
  GoButtonGtk::Mode visible_mode() const { return go_->visible_mode_; }

  // mutable accessors for internal state
  ScopedRunnableMethodFactory<GoButtonGtk>* stop_timer() {
    return &go_->stop_timer_;
  }

  // mutators for internal state
  void set_state(GoButtonGtk::ButtonState state) { go_->state_ = state; }
  void set_intended_mode(GoButtonGtk::Mode mode) { go_->intended_mode_ = mode; }
  void set_visible_mode(GoButtonGtk::Mode mode) { go_->visible_mode_ = mode; }

  // forwarders to private methods
  Task* CreateButtonTimerTask() { return go_->CreateButtonTimerTask(); }
  gboolean OnLeave() {
    return GoButtonGtk::OnLeave(GTK_BUTTON(go_->widget()), go_);
  }

  gboolean OnClicked() {
    return GoButtonGtk::OnClicked(GTK_BUTTON(go_->widget()), go_);
  }

 private:
  GoButtonGtk* const go_;
};

namespace {

class GoButtonGtkTest : public testing::Test {
 protected:
  GoButtonGtkTest() : go_(NULL, NULL), peer_(&go_) { }

 protected:
  GoButtonGtk go_;
  GoButtonGtkPeer peer_;
};

TEST_F(GoButtonGtkTest, ChangeModeGo) {
  go_.ChangeMode(GoButtonGtk::MODE_GO, true);
  EXPECT_EQ(GoButtonGtk::MODE_GO, peer_.intended_mode());
  EXPECT_EQ(GoButtonGtk::MODE_GO, peer_.visible_mode());
}

TEST_F(GoButtonGtkTest, ChangeModeStop) {
  go_.ChangeMode(GoButtonGtk::MODE_STOP, true);
  EXPECT_EQ(GoButtonGtk::MODE_STOP, peer_.intended_mode());
  EXPECT_EQ(GoButtonGtk::MODE_STOP, peer_.visible_mode());
}

TEST_F(GoButtonGtkTest, ScheduleChangeModeNormalGo) {
  peer_.set_visible_mode(GoButtonGtk::MODE_STOP);
  peer_.set_state(GoButtonGtk::BS_NORMAL);
  go_.ChangeMode(GoButtonGtk::MODE_GO, false);
  EXPECT_EQ(GoButtonGtk::MODE_GO, peer_.intended_mode());
  EXPECT_EQ(GoButtonGtk::MODE_GO, peer_.visible_mode());
}

TEST_F(GoButtonGtkTest, ScheduleChangeModeHotGo) {
  peer_.set_visible_mode(GoButtonGtk::MODE_STOP);
  peer_.set_state(GoButtonGtk::BS_HOT);
  go_.ChangeMode(GoButtonGtk::MODE_GO, false);
  EXPECT_EQ(GoButtonGtk::MODE_GO, peer_.intended_mode());
  EXPECT_EQ(GoButtonGtk::MODE_STOP, peer_.visible_mode());
}

TEST_F(GoButtonGtkTest, ScheduleChangeModeNormalStop) {
  peer_.set_visible_mode(GoButtonGtk::MODE_GO);
  peer_.set_state(GoButtonGtk::BS_NORMAL);
  go_.ChangeMode(GoButtonGtk::MODE_STOP, false);
  EXPECT_EQ(GoButtonGtk::MODE_STOP, peer_.intended_mode());
  EXPECT_EQ(GoButtonGtk::MODE_STOP, peer_.visible_mode());
}

TEST_F(GoButtonGtkTest, ScheduleChangeModeHotStop) {
  peer_.set_visible_mode(GoButtonGtk::MODE_GO);
  peer_.set_state(GoButtonGtk::BS_HOT);
  go_.ChangeMode(GoButtonGtk::MODE_STOP, false);
  EXPECT_EQ(GoButtonGtk::MODE_STOP, peer_.intended_mode());
  EXPECT_EQ(GoButtonGtk::MODE_STOP, peer_.visible_mode());
}

TEST_F(GoButtonGtkTest, ScheduleChangeModeTimerHotStop) {
  peer_.set_visible_mode(GoButtonGtk::MODE_GO);
  peer_.set_state(GoButtonGtk::BS_HOT);
  scoped_ptr<Task> task(peer_.CreateButtonTimerTask());
  go_.ChangeMode(GoButtonGtk::MODE_STOP, false);
  EXPECT_EQ(GoButtonGtk::MODE_STOP, peer_.intended_mode());
  EXPECT_EQ(GoButtonGtk::MODE_GO, peer_.visible_mode());
}

TEST_F(GoButtonGtkTest, OnLeaveIntendedStop) {
  peer_.set_state(GoButtonGtk::BS_HOT);
  peer_.set_visible_mode(GoButtonGtk::MODE_GO);
  peer_.set_intended_mode(GoButtonGtk::MODE_STOP);
  EXPECT_TRUE(peer_.OnLeave());
  EXPECT_EQ(GoButtonGtk::MODE_STOP, peer_.visible_mode());
  EXPECT_EQ(GoButtonGtk::MODE_STOP, peer_.intended_mode());
}

TEST_F(GoButtonGtkTest, OnLeaveIntendedGo) {
  peer_.set_state(GoButtonGtk::BS_HOT);
  peer_.set_visible_mode(GoButtonGtk::MODE_STOP);
  peer_.set_intended_mode(GoButtonGtk::MODE_GO);
  EXPECT_TRUE(peer_.OnLeave());
  EXPECT_EQ(GoButtonGtk::MODE_GO, peer_.visible_mode());
  EXPECT_EQ(GoButtonGtk::MODE_GO, peer_.intended_mode());
}

TEST_F(GoButtonGtkTest, OnClickedStop) {
  peer_.set_visible_mode(GoButtonGtk::MODE_STOP);
  EXPECT_TRUE(peer_.OnClicked());
  EXPECT_EQ(GoButtonGtk::MODE_GO, peer_.visible_mode());
  EXPECT_EQ(GoButtonGtk::MODE_GO, peer_.intended_mode());
}

}  // namespace
