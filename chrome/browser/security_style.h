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

#ifndef CHROME_BROWSER_SECURITY_STYLE_H__
#define CHROME_BROWSER_SECURITY_STYLE_H__

// Various aspects of the UI change their appearance according to the security
// context in which they are displayed.  For example, the location bar displays
// a lock icon when it is displayed during a valid SSL connection.
// SecuirtySyle enumerates these styles, but it is up to the UI elements to
// adjust their display appropriately.
enum SecurityStyle {
  // SECURITY_STYLE_UNKNOWN indicates that we do not know the proper security
  // style for this object.
  SECURITY_STYLE_UNKNOWN,

  // SECURITY_STYLE_UNAUTHENTICATED means the authenticity of this object can
  // not be determined, either because it was retrieved using an unauthenticated
  // protocol, such as HTTP or FTP, or it was retrieved using a protocol that
  // supports authentication, such as HTTPS, but there were errors during
  // transmission that render us uncertain to the object's authenticity.
  SECURITY_STYLE_UNAUTHENTICATED,

  // SECURITY_STYLE_AUTHENTICATION_BROKEN indicates that we tried to retrieve
  // this object in an authenticated manner but were unable to do so.
  SECURITY_STYLE_AUTHENTICATION_BROKEN,

  // SECURITY_STYLE_AUTHENTICATED indicates that we successfully retrieved this
  // object over an authenticated protocol, such as HTTPS.
  SECURITY_STYLE_AUTHENTICATED,
};

#endif // CHROME_BROWSER_SECURITY_STYLE_H__
