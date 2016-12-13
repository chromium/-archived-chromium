// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_TRANSACTION_FACTORY_H__
#define NET_HTTP_HTTP_TRANSACTION_FACTORY_H__

namespace net {

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

  // Suspends the creation of new transactions. If |suspend| is false, creation
  // of new transactions is resumed.
  virtual void Suspend(bool suspend) = 0;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_TRANSACTION_FACTORY_H__
