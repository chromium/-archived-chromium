// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScheduledAction_h
#define ScheduledAction_h

namespace WebCore {

class DOMWindow;

class ScheduledAction {
public:
  virtual ~ScheduledAction() { }
  virtual void execute(DOMWindow* window) = 0;
  virtual void execute(ScriptExecutionContext*) {}
};

}

#endif

