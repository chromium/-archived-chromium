// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSION_SERVICE_TEST_HELPER_H__
#define CHROME_BROWSER_SESSION_SERVICE_TEST_HELPER_H__

#include <vector>

#include "base/ref_counted.h"

class SessionBackend;
class SessionCommand;
class SessionID;
class SessionService;
struct SessionTab;
struct SessionWindow;
struct TabNavigation;

// A simple class that makes writing SessionService related tests easier.

class SessionServiceTestHelper {
 public:
  explicit SessionServiceTestHelper() {}

  explicit SessionServiceTestHelper(SessionService* service)
      : service_(service) {}

  void RestoreSessionFromCommands(const std::vector<SessionCommand*>& commands,
                                  std::vector<SessionWindow*>* valid_windows);

  void PrepareTabInWindow(const SessionID& window_id,
                          const SessionID& tab_id,
                          int visual_index,
                          bool select);

  // Reads the contents of the last session.
  void ReadWindows(std::vector<SessionWindow*>* windows);

  void AssertTabEquals(SessionID& window_id,
                       SessionID& tab_id,
                       int visual_index,
                       int nav_index,
                       int nav_count,
                       const SessionTab& session_tab);

  void AssertTabEquals(int visual_index,
                       int nav_index,
                       int nav_count,
                       const SessionTab& session_tab);

  void AssertNavigationEquals(const TabNavigation& expected,
                              const TabNavigation& actual);

  void AssertSingleWindowWithSingleTab(
      const std::vector<SessionWindow*>& windows,
      int nav_count);

  void set_service(SessionService* service) { service_ = service; }
  SessionService* service() { return service_.get(); }

  SessionBackend* backend();

 private:
  scoped_refptr<SessionService> service_;

  DISALLOW_EVIL_CONSTRUCTORS(SessionServiceTestHelper);
};

#endif  // CHROME_BROWSER_SESSION_SERVICE_TEST_HELPER_H__

