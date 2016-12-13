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

  // Copies the given addrinfo rather than adopting it.
  void Copy(const struct addrinfo* head);

  // Sets the port of all addresses in the list to |port| (that is the
  // sin[6]_port field for the sockaddrs).
  void SetPort(int port);

  // Retrieves the port number of the first sockaddr in the list. (If SetPort()
  // was previously used on this list, then all the addresses will have this
  // same port number.)
  int GetPort() const;

  // Sets the address to match |src|, and have each sockaddr's port be |port|.
  // If |src| already has the desired port this operation is cheap (just adds
  // a reference to |src|'s data.) Otherwise we will make a copy.
  void SetFrom(const AddressList& src, int port);

  // Clears all data from this address list. This leaves the list in the same
  // empty state as when first constructed.
  void Reset();

  // Get access to the head of the addrinfo list.
  const struct addrinfo* head() const { return data_->head; }

 private:
  struct Data : public base::RefCountedThreadSafe<Data> {
    Data(struct addrinfo* ai, bool is_system_created)
        : head(ai), is_system_created(is_system_created) {}
    ~Data();
    struct addrinfo* head;

    // Indicates which free function to use for |head|.
    bool is_system_created;
  };
  scoped_refptr<Data> data_;
};

}  // namespace net

#endif  // NET_BASE_ADDRESS_LIST_H_
