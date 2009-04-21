// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_INSPECTABLE_TAB_PROXY_H_
#define CHROME_BROWSER_DEBUGGER_INSPECTABLE_TAB_PROXY_H_

#include <string>

#include "base/basictypes.h"
#include "base/hash_tables.h"

class DebuggerRemoteService;
class DevToolsClientHost;
class NavigationController;

// Proxies debugged tabs' NavigationControllers using their UIDs.
class InspectableTabProxy {
 public:
  typedef base::hash_map<int32, NavigationController*> ControllersMap;

  InspectableTabProxy() {}
  virtual ~InspectableTabProxy() {}

  // Returns a map of NavigationControllerKeys to NavigationControllers
  // for all Browser instances.
  const ControllersMap& controllers_map();

  // Creates a new DevToolsClientHost implementor instance.
  // |id| is the UID of the tab to debug.
  // |service| is the DebuggerRemoteService instance the DevToolsClient
  //         messages shall be dispatched to.
  static DevToolsClientHost* NewClientHost(int32 id,
                                           DebuggerRemoteService* service);

 private:
  ControllersMap controllers_map_;
  DISALLOW_COPY_AND_ASSIGN(InspectableTabProxy);
};

#endif  // CHROME_BROWSER_DEBUGGER_INSPECTABLE_TAB_PROXY_H_
