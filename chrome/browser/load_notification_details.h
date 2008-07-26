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

// The LoadNotificationDetails object contains additional details about a
// page load that has been completed.  It was created to let the MetricsService
// log page load metrics.

#ifndef CHROME_BROWSER_LOAD_NOTIFICATION_DETAILS_H__
#define CHROME_BROWSER_LOAD_NOTIFICATION_DETAILS_H__

#include "base/basictypes.h"
#include "base/time.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/common/page_transition_types.h"
#include "googleurl/src/gurl.h"

class LoadNotificationDetails {
 public:
  LoadNotificationDetails(const GURL& url,
                          PageTransition::Type origin,
                          TimeDelta load_time,
                          NavigationController* controller,
                          int session_index)
      : url_(url),
        origin_(origin),
        load_time_(load_time),
        controller_(controller),
        session_index_(session_index) {}

  ~LoadNotificationDetails() {}

  const GURL& url() const { return url_; }
  PageTransition::Type origin() const { return origin_; }
  TimeDelta load_time() const { return load_time_; }
  int session_index() const { return session_index_; }
  NavigationController* controller() const { return controller_; }

 private:
  GURL url_;           // the URL loaded
  TimeDelta load_time_;               // length of time the page load took
  int session_index_;  // index of the load within the tab session
  PageTransition::Type origin_;       // type of action that caused the load
  NavigationController* controller_;  // tells us which tab the load was in

  LoadNotificationDetails() {}

  DISALLOW_EVIL_CONSTRUCTORS(LoadNotificationDetails);
};

#endif  // CHROME_BROWSER_LOAD_NOTIFICATION_DETAILS_H__
