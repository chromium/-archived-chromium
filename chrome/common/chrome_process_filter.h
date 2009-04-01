// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_PROCESS_FILTER_H__
#define CHROME_COMMON_CHROME_PROCESS_FILTER_H__

#include "base/process_util.h"

// Filter all chrome browser processes that run with the same user data
// directory.
class BrowserProcessFilter : public base::ProcessFilter {
 public:
  // Create the filter for the given user_data_dir.
  // If user_data_dir is an empty string, will use the PathService
  // user_data_dir (e.g. chrome::DIR_USER_DATA).
  explicit BrowserProcessFilter(const std::wstring user_data_dir);

  uint32 browser_process_id() const { return browser_process_id_; }

  virtual bool Includes(base::ProcessId pid, base::ProcessId parent_pid) const;

 private:
  std::wstring user_data_dir_;
  DWORD browser_process_id_;

  DISALLOW_EVIL_CONSTRUCTORS(BrowserProcessFilter);
};

#endif  // CHROME_COMMON_CHROME_PROCESS_FILTER_H__
