// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_REMOTE_MESSAGE_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_REMOTE_MESSAGE_H_

#include <string>

#include "base/basictypes.h"
#include "base/hash_tables.h"

// Contains DevTools protocol message header names
// and the Flags header bit field constants.
struct DevToolsRemoteMessageHeaders {
  // The content length in decimal.
  static const char kContentLength[];
  // The tool that should handle the message.
  static const char kTool[];
  // The destination (inspected) object identifier (if any), like a TabID.
  static const char kDestination[];
};

// Represents a Chrome remote debugging protocol message transferred
// over the wire between the remote debugger and a Chrome instance.
// Consider using DevToolsRemoteMessageBuilder (see end of this file) for easy
// construction of outbound (Chrome -> remote debugger) messages.
class DevToolsRemoteMessage {
 public:
  typedef base::hash_map<std::string, std::string> HeaderMap;

  // Use this as the second parameter in a |GetHeader| call to use
  // an empty string as the default value.
  static const char kEmptyValue[];

  // Constructs an empty message with no content or headers.
  DevToolsRemoteMessage() {}
  DevToolsRemoteMessage(const HeaderMap& headers, const std::string& content)
      : header_map_(headers),
        content_(content) {}
  virtual ~DevToolsRemoteMessage() {}

  const HeaderMap& headers() const {
    return header_map_;
  }

  const std::string& content() const {
    return content_;
  }

  const int content_length() const {
    return content_.size();
  }

  const std::string tool() const {
    return GetHeaderWithEmptyDefault(DevToolsRemoteMessageHeaders::kTool);
  }

  const std::string destination() const {
    return GetHeaderWithEmptyDefault(
        DevToolsRemoteMessageHeaders::kDestination);
  }

  // Returns the header value providing default_value if the header is absent.
  const std::string GetHeader(const std::string& header_name,
                              const std::string& default_value) const;

  // Returns the header value providing an empty string if the header is absent.
  const std::string GetHeaderWithEmptyDefault(
      const std::string& header_name) const;

  // Returns a string representation of the message useful for the transfer to
  // the remote debugger.
  const std::string ToString() const;

 private:
  HeaderMap header_map_;
  std::string content_;
  // Cannot DISALLOW_COPY_AND_ASSIGN(DevToolsRemoteMessage) since it is passed
  // as an IPC message argument and needs to be copied.
};

// Facilitates easy construction of outbound (Chrome -> remote debugger)
// DevToolsRemote messages.
class DevToolsRemoteMessageBuilder {
 public:
  // A singleton instance getter.
  static DevToolsRemoteMessageBuilder& instance();
  // Creates a message given the certain header values and a payload.
  DevToolsRemoteMessage* Create(const std::string& tool,
                                const std::string& destination,
                                const std::string& payload);

 private:
  DevToolsRemoteMessageBuilder() {}
  virtual ~DevToolsRemoteMessageBuilder() {}
  DISALLOW_COPY_AND_ASSIGN(DevToolsRemoteMessageBuilder);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_REMOTE_MESSAGE_H_
