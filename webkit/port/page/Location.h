// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


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
