// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ADDRESS_LIST_H_
#define NET_BASE_ADDRESS_LIST_H_

#include "base/ref_counted.h"

struct addrinfo;

namespace net {

// An AddressList object contains a linked list of addrinfo structures.  This
// class is designed to be copied around by value.
class AddressList {
 public:
  // Adopt the given addrinfo list in place of the existing one if any.  This
  // hands over responsibility for freeing the addrinfo list to the AddressList
  // object.
  void Adopt(struct addrinfo* head);

  // Get access to the head of the addrinfo list.
  const struct addrinfo* head() const { return data_->head; }

 private:
  struct Data : public base::RefCountedThreadSafe<Data> {
    explicit Data(struct addrinfo* ai) : head(ai) {}
    ~Data();
    struct addrinfo* head;
   private:
    Data();
  };
  scoped_refptr<Data> data_;
};

}  // namespace net

#endif  // NET_BASE_ADDRESS_LIST_H_
