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

#ifndef NET_HTTP_HTTP_TRANSACTION_FACTORY_H__
#define NET_HTTP_HTTP_TRANSACTION_FACTORY_H__

namespace net {

class AuthCache;
class HttpCache;
class HttpTransaction;

// An interface to a class that can create HttpTransaction objects.
class HttpTransactionFactory {
 public:
  virtual ~HttpTransactionFactory() {}

  // Creates a HttpTransaction object.
  virtual HttpTransaction* CreateTransaction() = 0;

  // Returns the associated cache if any (may be NULL).
  virtual HttpCache* GetCache() = 0;

  // Returns the associated HTTP auth cache if any (may be NULL).
  virtual AuthCache* GetAuthCache() = 0;

  // Suspends the creation of new transactions. If |suspend| is false, creation
  // of new transactions is resumed.
  virtual void Suspend(bool suspend) = 0;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_TRANSACTION_FACTORY_H__
