// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to hold the parameters we get back from the
// ViewHostMsg_DomOperationResponse IPC call.  This is used when passing
// parameters through the notification service.

#ifndef CHROME_BROWSER_DOM_OPERATION_NOTIFICATION_DETAILS_H__
#define CHROME_BROWSER_DOM_OPERATION_NOTIFICATION_DETAILS_H__

class DomOperationNotificationDetails {
 public:
  DomOperationNotificationDetails(const std::string json, int automation_id)
    : json_(json), automation_id_(automation_id) { }

  ~DomOperationNotificationDetails() { }

  std::string json() const { return json_; }
  int automation_id() const { return automation_id_; }

 private:
  std::string json_;
  int automation_id_;
};

#endif  // CHROME_BROWSER_DOM_OPERATION_NOTIFICATION_DETAILS_H__
