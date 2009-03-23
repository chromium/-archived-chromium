// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBWORKER_H_
#define WEBKIT_GLUE_WEBWORKER_H_

#include "base/string16.h"

class GURL;
class WebWorkerClient;

// This is a version of the WebCore::WorkerContextProxy interface that uses
// Chrome data types.
class WebWorker {
 public:
  virtual ~WebWorker() { }

  // Creates a WebWorker object that wraps around the WebKit code that implements
  // web workers.
  static WebWorker* Create(WebWorkerClient* client);

  virtual void StartWorkerContext(const GURL& script_url,
                                  const string16& user_agent,
                                  const string16& source_code) = 0;
  virtual void TerminateWorkerContext() = 0;
  virtual void PostMessageToWorkerContext(const string16& message) = 0;
  virtual void WorkerObjectDestroyed() = 0;
};

#endif  // #ifndef WEBKIT_GLUE_WEBWORKER_H_
