// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBWORKERCLIENT_H_
#define WEBKIT_GLUE_WEBWORKERCLIENT_H_

#include "base/string16.h"

// This is a version of the WebCore::WorkerObjectProxy interface that uses
// Chrome data types.
class WebWorkerClient {
 public:
  virtual ~WebWorkerClient() { }

  virtual void PostMessageToWorkerObject(const string16& message) = 0;
  virtual void PostExceptionToWorkerObject(
      const string16& error_message,
      int line_number,
      const string16& source_url) = 0;
  // destination, source, and level are the int values of the corresponding
  // WebKit enums.  This avoids duplicating the enums and having to stay up to
  // date.
  virtual void PostConsoleMessageToWorkerObject(
      int destination,
      int source,
      int level,
      const string16& message,
      int line_number,
      const string16& source_url) = 0;
  virtual void ConfirmMessageFromWorkerObject(bool has_pending_activity) = 0;
  virtual void ReportPendingActivity(bool has_pending_activity) = 0;
  virtual void WorkerContextDestroyed() = 0;
};

#endif  // #ifndef WEBKIT_GLUE_WEBWORKERCLIENT_H_
