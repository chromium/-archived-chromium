// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_VERSION_H_
#define NET_HTTP_VERSION_H_

#include "base/basictypes.h"

namespace net {

// Wrapper for an HTTP (major,minor) version pair.
class HttpVersion {
 private:
  uint32 value; // Packed as <major>:<minor>

 public:
  // Default constructor (major=0, minor=0).
  HttpVersion() : value(0) { }

  // Build from unsigned major/minor pair.
  HttpVersion(uint16 major, uint16 minor) : value(major << 16 | minor) { }

  // Major version number.
  uint16 major() const {
    return value >> 16;
  }

  // Minor version number.
  uint16 minor() const {
    return value & 0xffff;
  }

  // Overloaded operators:
  
  bool operator==(const HttpVersion& v) const {
    return value == v.value;
  }
  bool operator!=(const HttpVersion& v) const {
    return value != v.value;
  }
  bool operator>(const HttpVersion& v) const {
    return value > v.value;
  }
  bool operator>=(const HttpVersion& v) const {
    return value >= v.value;
  }
  bool operator<(const HttpVersion& v) const {
    return value < v.value;
  }
  bool operator<=(const HttpVersion& v) const {
    return value <= v.value;
  }
};

}  // namespace net

#endif  // NET_HTTP_VERSION_H_

