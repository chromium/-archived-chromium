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

// Winsock initialization must happen before any Winsock calls are made.  This
// class provides a wrapper for WSAStartup and WSACleanup. There are 3 ways to
// use it: either allocate a new WinsockInit object at startup and delete when
// shutting down, manually call Init and Cleanup, or use the EnsureWinsockInit
// method, which may be called multiple times.  In the second case, Cleanup
// should only be called if Init was successful.

#ifndef NET_BASE_WINSOCK_INIT_H_
#define NET_BASE_WINSOCK_INIT_H_

class WinsockInit {
 public:
  WinsockInit();
  ~WinsockInit();

  static bool Init();
  static void Cleanup();

 private:
  bool did_init_;
};

// Force there to be a global WinsockInit object that gets created once and
// destroyed at application exit.  This may be called multiple times.
void EnsureWinsockInit();

#endif  // NET_BASE_WINSOCK_INIT_H_
