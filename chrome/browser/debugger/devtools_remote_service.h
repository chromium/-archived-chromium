// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_REMOTE_SERVICE_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_REMOTE_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/debugger/devtools_remote.h"

class DevToolsRemoteMessage;
class DevToolsProtocolHandler;
class DictionaryValue;
class Value;

// Contains constants for DevToolsRemoteService tool protocol commands.
struct DevToolsRemoteServiceCommand {
  static const std::string kPing;
  static const std::string kVersion;
  static const std::string kListTabs;
};

// Handles Chrome remote debugger protocol service commands.
class DevToolsRemoteService : public DevToolsRemoteListener {
 public:
  explicit DevToolsRemoteService(DevToolsProtocolHandler* delegate);
  virtual ~DevToolsRemoteService();

  // DevToolsRemoteListener interface
  virtual void HandleMessage(const DevToolsRemoteMessage& message);
  virtual void OnConnectionLost() {}

  static const std::string kToolName;

 private:
  // Operation result returned in the "result" field.
  struct Result {
    static const int kOk = 0;
    static const int kUnknownCommand = 1;
  };
  void ProcessJson(DictionaryValue* json, const DevToolsRemoteMessage& message);
  static const std::wstring kCommandWide;
  static const std::wstring kDataWide;
  static const std::wstring kResultWide;
  DevToolsProtocolHandler* delegate_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsRemoteService);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_REMOTE_SERVICE_H_
