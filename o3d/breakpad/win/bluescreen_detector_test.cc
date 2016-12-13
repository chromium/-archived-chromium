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


// Tests functionality of the BluescreenDetector class

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/error.h"
#include "breakpad/win/bluescreen_detector.h"
#include "plugin/cross/plugin_logging.h"

// This is defined in "main_win.cc" but not in unit tests
o3d::PluginLogging *g_logger = NULL;

namespace o3d {

// Test fixture for BluescreenDetector testing.
class BluescreenDetectorTest : public testing::Test {
};

using std::vector;
using o3d::MarkerFileInfo;

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Mock implementation

const uint64 kInitialCurrentTime = 20000;
const uint64 kInitialUpTime = 10000;

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class TimeManagerMock : public TimeManagerInterface {
 public:
  TimeManagerMock()
    : current_time_(kInitialCurrentTime),
      up_time_(kInitialUpTime) { }

  virtual uint64 GetCurrentTime() { return current_time_; }
  virtual uint64 GetUpTime() { return up_time_; }

  void AdvanceTime(uint64 n) {
    current_time_ += n;
    up_time_ += n;
  }

  void SetCurrentTime(uint64 t) { current_time_ = t; }
  void SetUpTime(uint64 t) { up_time_ = t; }

 private:
  uint64 current_time_;
  uint64 up_time_;
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class MarkerFileManagerMock : public MarkerFileManagerInterface {
 public:
  MarkerFileManagerMock(TimeManagerMock *time_manager)
    : MarkerFileManagerInterface(time_manager),
      mock_time_manager_(time_manager),
      created_count_(0),
      removed_count_(0) {
  };

  // "marker" file management.  The marker file is used to check for future
  // blue-screens.
  virtual void CreateMarkerFile();
  virtual void RemoveMarkerFile();

  void AddFileEntry(const String &name, uint64 creation_time) {
    file_list_.push_back(MarkerFileInfo(name, creation_time));
  }

  // Each should be 1 after the test is run
  int GetCreatedCount() const {
    return created_count_;
  }

  int GetRemovedCount() const {
    return removed_count_;
  }

  int GetMarkerFileCount() const {
    return file_list_.size();
  }

 private:
  virtual bool GetMarkerFileList(vector<MarkerFileInfo> *file_list);
  virtual void DeleteMarkerFile(const MarkerFileInfo &file_info);

  TimeManagerMock *mock_time_manager_;
  vector<MarkerFileInfo> file_list_;
  int created_count_;
  int removed_count_;
};


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class BluescreenLoggerMock : public BluescreenLoggerInterface {
 public:
  BluescreenLoggerMock() : bluescreen_count_(0), log_bluescreen_count_(0) { }

  // Pretends to send blue-screen info to Google
  virtual void LogBluescreen(int num_bluescreens) {
    ++log_bluescreen_count_;  // counts number of times this method is called
    bluescreen_count_ += num_bluescreens;
  }

  // Returns number of bluescreens logged
  int GetBluescreenCount() const {
    return bluescreen_count_;
  }

  // Returns total number of times LogBluescreen() was called
  // (it should only be called once)
  int GetLogBluescreenCount() const {
    return log_bluescreen_count_;
  }

 private:
  int bluescreen_count_;
  int log_bluescreen_count_;
};



const String kOurMarkerFileName("OurMarkerFile");

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void MarkerFileManagerMock::CreateMarkerFile() {
  AddFileEntry(kOurMarkerFileName, time_manager_->GetCurrentTime());
  mock_time_manager_->AdvanceTime(1);
  created_count_++;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void MarkerFileManagerMock::RemoveMarkerFile() {
  for (int i = 0; i < file_list_.size(); ++i) {
    if (file_list_[i].GetName() == kOurMarkerFileName) {
      file_list_.erase(file_list_.begin() + i);
      removed_count_++;
      break;
    }
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool MarkerFileManagerMock::GetMarkerFileList(
  vector<MarkerFileInfo> *file_list) {
  // Just duplicate our interval file list
  for (int i = 0; i < file_list_.size(); ++i) {
    file_list->push_back(file_list_[i]);
  }
  return true;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void MarkerFileManagerMock::DeleteMarkerFile(const MarkerFileInfo &file_info) {
  for (int i = 0; i < file_list_.size(); ++i) {
    if (file_list_[i].GetName() == file_info.GetName()) {
      file_list_.erase(file_list_.begin() + i);
      break;
    }
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Make sure marker file is written, then removed...
// This is also testing the case where there was a regular crash
// (such as "bus error" or "divide-by-zero") in which case the
// exception handler will call Stop() on the detector (instead of the plugin
// shutdown code calling it)
TEST_F(BluescreenDetectorTest, Basic) {
  TimeManagerMock time_manager;
  MarkerFileManagerMock marker_file_manager(&time_manager);
  BluescreenLoggerMock bluescreen_logger;

  BluescreenDetector detector(&time_manager,
                              &marker_file_manager,
                              &bluescreen_logger);

  detector.Start();

  // Make sure marker file was created (but not yet removed)
  EXPECT_EQ(1, marker_file_manager.GetCreatedCount());
  EXPECT_EQ(0, marker_file_manager.GetRemovedCount());

  detector.Stop();

  // Make sure marker file was created then removed
  EXPECT_EQ(1, marker_file_manager.GetCreatedCount());
  EXPECT_EQ(1, marker_file_manager.GetRemovedCount());

  // we didn't add any old marker files, so there should be no bluescreens
  EXPECT_EQ(0, bluescreen_logger.GetBluescreenCount());
  EXPECT_EQ(0, bluescreen_logger.GetLogBluescreenCount());
}

// Let's try simulating a simple blue-screen
TEST_F(BluescreenDetectorTest, SimulateBluescreen) {
  TimeManagerMock time_manager;
  MarkerFileManagerMock marker_file_manager(&time_manager);
  BluescreenLoggerMock bluescreen_logger;

  BluescreenDetector detector(&time_manager,
                              &marker_file_manager,
                              &bluescreen_logger);

  // Let's create a couple of stray marker files :)
  // and say they were created 100 time units before the machine was
  // booted
  uint64 kStrayCreationTime1 = kInitialCurrentTime - kInitialUpTime - 100;
  marker_file_manager.AddFileEntry("Stray1", kStrayCreationTime1);
  marker_file_manager.AddFileEntry("Stray2", kStrayCreationTime1);

  // Verify the two we just added
  EXPECT_EQ(2, marker_file_manager.GetMarkerFileCount());

  detector.Start();

  // Check that two bluescreens were detected (and reported)
  EXPECT_EQ(2, bluescreen_logger.GetBluescreenCount());

  // Check that LogBluescreen() was only called once (with two detections)
  EXPECT_EQ(1, bluescreen_logger.GetLogBluescreenCount());

  // Make sure the two "stray" marker files were removed
  // (so we won't report bluescreens multiple times)
  // the marker file added by |detector| should still be there
  EXPECT_EQ(1, marker_file_manager.GetMarkerFileCount());

  detector.Stop();

  // Now make sure the marker file added by |detector|
  // was also removed
  EXPECT_EQ(0, marker_file_manager.GetMarkerFileCount());

  // Make sure marker file was created then removed
  EXPECT_EQ(1, marker_file_manager.GetCreatedCount());
  EXPECT_EQ(1, marker_file_manager.GetRemovedCount());
}

// Let's make sure we don't detect a blue-screen from marker
// files written since boot time (these marker files may be
// written by o3d running in other browsers alongside ours)
TEST_F(BluescreenDetectorTest, OtherBrowsersRunning) {
  TimeManagerMock time_manager;
  MarkerFileManagerMock marker_file_manager(&time_manager);
  BluescreenLoggerMock bluescreen_logger;

  BluescreenDetector detector(&time_manager,
                              &marker_file_manager,
                              &bluescreen_logger);

  // Let's create a couple of other marker files :)
  // but this time 100 time units AFTER the machine was
  // booted, simulating other browsers which are still running
  // o3d
  uint64 kStrayCreationTime1 = kInitialCurrentTime - kInitialUpTime + 100;
  marker_file_manager.AddFileEntry("OtherBrowserMarker1", kStrayCreationTime1);
  marker_file_manager.AddFileEntry("OtherBrowserMarker2", kStrayCreationTime1);

  // Verify the two we just added
  EXPECT_EQ(2, marker_file_manager.GetMarkerFileCount());

  detector.Start();

  // Check that NO bluescreens were detected
  EXPECT_EQ(0, bluescreen_logger.GetBluescreenCount());

  // Check that NO bluescreen reports were logged/uploaded
  EXPECT_EQ(0, bluescreen_logger.GetLogBluescreenCount());

  // Make sure the two other marker files were NOT removed
  // because they were not created before boot time and
  // are owned by a different browser...
  // There should be the two we added, plus the one added
  // by |detector|
  EXPECT_EQ(3, marker_file_manager.GetMarkerFileCount());

  detector.Stop();

  // Now make sure the marker file added by |detector|
  // was removed, so we're left with the original two
  EXPECT_EQ(2, marker_file_manager.GetMarkerFileCount());

  // Make sure marker file was created then removed
  EXPECT_EQ(1, marker_file_manager.GetCreatedCount());
  EXPECT_EQ(1, marker_file_manager.GetRemovedCount());
}

}  // namespace o3d
