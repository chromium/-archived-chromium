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
  static PassRefPtr<Location> create(Frame* frame)
  {
    return adoptRef(new Location(frame));
  }

  Frame* frame() { return m_frame; }

  String protocol() const;
  String host() const;
  String hostname() const;
  String port() const;
  String pathname() const;
  String search() const;
  String hash() const;
  String href() const;

  String toString() const;  

#if USE(V8)
  void setHash(const String& str);
  void setHost(const String& str);
  void setHostname(const String&);
  void setHref(const String&);
  void setPathname(const String&);
  void setPort(const String&);
  void setProtocol(const String&);
  void setSearch(const String&);

  void reload(bool forceget);
  void replace(const String& url);
  void assign(const String& url);
#endif

  void disconnectFrame() { m_frame = 0; }

 private:
  Location(Frame* frame) : m_frame(frame) { }

  Frame* m_frame;

#if USE(V8)
  friend class WindowV8;
#endif
};

}  // namespace WebCore

#endif // Location_h

