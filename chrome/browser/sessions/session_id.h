// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_ID_H_
#define CHROME_BROWSER_SESSIONS_SESSION_ID_H_

#include "base/basictypes.h"

class SessionService;

// Uniquely identifies a tab or window for the duration of a session.
class SessionID {
 public:
  typedef int32 id_type;

  SessionID();
  ~SessionID() {}

  // Returns the underlying id.
  id_type id() const { return id_; }

 private:
  friend class SessionService;

  explicit SessionID(id_type id) : id_(id) {}

  // Resets the id. This is used when restoring a session
  void set_id(id_type id) { id_ = id; }

  id_type id_;
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_ID_H_
