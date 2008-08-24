// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef Location_h
#define Location_h

#include <wtf/RefCounted.h>
#include "Frame.h"

namespace WebCore {

class Location : public RefCounted<Location> {
 public:
  Location::Location(Frame* frame) : m_frame(frame) { }

  Frame* frame() { return m_frame; }

  String hash();
  void setHash(const String& str);

  String host();
  void setHost(const String& str);

  String hostname();
  void setHostname(const String&);

  String href();
  void setHref(const String&);

  String pathname();
  void setPathname(const String&);

  String port();
  void setPort(const String&);

  String protocol();
  void setProtocol(const String&);

  String search();
  void setSearch(const String&);

  void reload(bool forceget);
  void replace(const String& url);
  void assign(const String& url);

  String toString();  

  void disconnectFrame() { m_frame = 0; }

 private:
  Frame* m_frame;

  friend class WindowV8;

  void ChangeLocationTo(const KURL& url, bool lock_history);
};

}  // namespace WebCore

#endif // Location_h

