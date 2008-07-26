// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
