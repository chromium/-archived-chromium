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


// BluescreenDetector attempts to identify cases where the machine bluescreened
// and was caused by o3d.

#ifndef CLIENT3D_BREAKPAD_WIN_BLUESCREENDETECTOR_H_
#define CLIENT3D_BREAKPAD_WIN_BLUESCREENDETECTOR_H_

#include <string.h>
#include <stdlib.h>
#include <vector>

#ifdef OS_WIN
#include <windows.h>
#endif

#include "base/basictypes.h"
#include "core/cross/types.h"

namespace o3d {

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class MarkerFileInfo {
 public:
  MarkerFileInfo(const String &name, uint64 creation_time)
   : name_(name), creation_time_(creation_time) { };

  MarkerFileInfo() : name_(""), creation_time_(0) { };

  // explicit copy constructor
  MarkerFileInfo(const MarkerFileInfo& info)
    : name_(info.name_),
      creation_time_(info.creation_time_) { }

  const String &GetName() const { return name_; }
  uint64 GetCreationTime() const { return creation_time_; }

 private:
  String name_;
  uint64 creation_time_;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// We create interfaces so that we can mock these elements in the unit tests

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// TimeManagerInterface deals with times:
//  - time since boot
//  - current time
//  - file creation time
//
// with this information, it helps determine if marker file is "new" or "old"
// "old" means the file was created before the last time the machine was booted
//

class TimeManagerInterface {
 public:
  // times are in units of 100-nanosecond intervals (FILETIME units)
   virtual uint64 GetCurrentTime() = 0;
   virtual uint64 GetUpTime() = 0;

  // Returns |true| if the marker file was created before the machine
  // was last re-booted
   bool IsMarkerFileOld(const MarkerFileInfo &file_info);
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Manages a directory where "marker" files will be written.  The presence or
// absence of a marker file, along with its creation date, current time, and
// time since boot can be used to help determine if a blue-screen has occurred.
class MarkerFileManagerInterface {
 public:
  MarkerFileManagerInterface(TimeManagerInterface *time_manager)
    : time_manager_(time_manager) {}

  virtual void CreateMarkerFile() = 0;
  virtual void RemoveMarkerFile() = 0;


  // By looking at the creation date, along with the current time,
  // and the "up time" (time since boot) we can tell if any "marker" files were
  // created before boot time.  If found, they will be considered as evidence for
  // a blue-screen event in the past.  Returns the number of such files found.
  int DetectStrayMarkerFiles();

 protected:
  virtual bool GetMarkerFileList(std::vector<MarkerFileInfo> *file_list) = 0;
  virtual void DeleteMarkerFile(const MarkerFileInfo &file_info) = 0;

  TimeManagerInterface *time_manager_;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class BluescreenLoggerInterface {
 public:
  virtual void LogBluescreen(int num_bluescreens) = 0;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Actual implementation

// We only actually implement the interfaces for Windows, but we can unit test
// (with mocks) on all platforms
#ifdef OS_WIN

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class MarkerFileManager : public MarkerFileManagerInterface {
 public:
  MarkerFileManager(TimeManagerInterface *time_manager)
    : MarkerFileManagerInterface(time_manager), marker_file_(NULL) { };

  // "marker" file management.  The marker file is used to check for future
  // blue-screens.
  virtual void CreateMarkerFile();
  virtual void RemoveMarkerFile();

 private:
  virtual bool GetMarkerFileList(std::vector<MarkerFileInfo> *file_list);
  virtual void DeleteMarkerFile(const MarkerFileInfo &file_info);

  const std::wstring GetMarkerDirectory();
  const std::wstring GetUUIDString();

  HANDLE marker_file_;
  std::wstring marker_file_name_;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class TimeManager : public TimeManagerInterface {
 public:
  virtual uint64 GetCurrentTime();
  virtual uint64 GetUpTime();

  static uint64 FileTimeToUInt64(FILETIME time);
};


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class BluescreenLogger : public BluescreenLoggerInterface {
 public:
  // Sends blue-screen info to Google
  virtual void LogBluescreen(int num_bluescreens);
};

#endif  // OS_WIN


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class BluescreenDetector {
 public:
  // Default constructor for real-world use
  BluescreenDetector();

  // For mocking/testing
  BluescreenDetector(TimeManagerInterface *time_manager,
                     MarkerFileManagerInterface *marker_file_manager,
                     BluescreenLoggerInterface *bluescreen_logger)
    : started_(false),
      time_manager_(time_manager),
      marker_file_manager_(marker_file_manager),
      bluescreen_logger_(bluescreen_logger) { }

  virtual ~BluescreenDetector();

  // Call Start() to check for blue-screens which may have occured and log them
  // if so.  Also, writes out a "marker" to be used to check for future
  // blue-screens.
  //
  // Should be called when the plugin first loads
  void Start();

  // Call when the plugin unloads - the marker file is deleted here
  // on Windows it's unnecessary to call Stop() since the file will be
  // automatically deleted by the system when the process exits (or crashes)
  void Stop();

 private:
  bool started_;

  TimeManagerInterface *time_manager_;
  MarkerFileManagerInterface  *marker_file_manager_;
  BluescreenLoggerInterface *bluescreen_logger_;

  DISALLOW_COPY_AND_ASSIGN(BluescreenDetector);
};

}  // namespace o3d

#endif  // CLIENT3D_BREAKPAD_WIN_BLUESCREENDETECTOR_H_
