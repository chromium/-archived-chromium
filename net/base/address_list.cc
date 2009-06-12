// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_list.h"

#ifdef OS_WIN
#include <ws2tcpip.h>
#include <wspiapi.h>  // Needed for Win2k compat.
#else
#include <netdb.h>
#endif
#include <stdlib.h>

#include "base/logging.h"

namespace net {

namespace {

// Make a deep copy of |info|. This copy should be deleted using
// DeleteCopyOfAddrinfo(), and NOT freeaddrinfo().
struct addrinfo* CreateCopyOfAddrinfo(const struct addrinfo* info) {
  struct addrinfo* copy = new struct addrinfo;

  // Copy all the fields (some of these are pointers, we will fix that next).
  memcpy(copy, info, sizeof(addrinfo));

  // ai_canonname is a NULL-terminated string.
  if (info->ai_canonname) {
#ifdef OS_WIN
    copy->ai_canonname = _strdup(info->ai_canonname);
#else
    copy->ai_canonname = strdup(info->ai_canonname);
#endif
  }

  // ai_addr is a buffer of length ai_addrlen.
  if (info->ai_addr) {
    copy->ai_addr = reinterpret_cast<sockaddr *>(new char[info->ai_addrlen]);
    memcpy(copy->ai_addr, info->ai_addr, info->ai_addrlen);
  }

  // Recursive copy.
  if (info->ai_next)
    copy->ai_next = CreateCopyOfAddrinfo(info->ai_next);

  return copy;
}

// Free an addrinfo that was created by CreateCopyOfAddrinfo().
void FreeMyAddrinfo(struct addrinfo* info) {
  if (info->ai_canonname)
    free(info->ai_canonname);  // Allocated by strdup.

  if (info->ai_addr)
    delete [] reinterpret_cast<char*>(info->ai_addr);

  struct addrinfo* next = info->ai_next;

  delete info;

  // Recursive free.
  if (next)
    FreeMyAddrinfo(next);
}

// Returns the address to port field in |info|.
uint16* GetPortField(const struct addrinfo* info) {
  if (info->ai_family == AF_INET) {
    DCHECK_EQ(sizeof(sockaddr_in), info->ai_addrlen);
    struct sockaddr_in* sockaddr =
        reinterpret_cast<struct sockaddr_in*>(info->ai_addr);
    return &sockaddr->sin_port;
  } else if (info->ai_family == AF_INET6) {
    DCHECK_EQ(sizeof(sockaddr_in6), info->ai_addrlen);
    struct sockaddr_in6* sockaddr =
        reinterpret_cast<struct sockaddr_in6*>(info->ai_addr);
    return &sockaddr->sin6_port;
  } else {
    NOTREACHED();
    return NULL;
  }
}

// Assign the port for all addresses in the list.
void SetPortRecursive(struct addrinfo* info, int port) {
  uint16* port_field = GetPortField(info);
  *port_field = htons(port);

  // Assign recursively.
  if (info->ai_next)
    SetPortRecursive(info->ai_next, port);
}

}  // namespace

void AddressList::Adopt(struct addrinfo* head) {
  data_ = new Data(head, true /*is_system_created*/);
}

void AddressList::Copy(const struct addrinfo* head) {
  data_ = new Data(CreateCopyOfAddrinfo(head), false /*is_system_created*/);
}

void AddressList::SetPort(int port) {
  SetPortRecursive(data_->head, port);
}

int AddressList::GetPort() const {
  uint16* port_field = GetPortField(data_->head);
  return ntohs(*port_field);
}

void AddressList::SetFrom(const AddressList& src, int port) {
  if (src.GetPort() == port) {
    // We can reference the data from |src| directly.
    *this = src;
  } else {
    // Otherwise we need to make a copy in order to change the port number.
    Copy(src.head());
    SetPort(port);
  }
}

void AddressList::Reset() {
  data_ = NULL;
}

AddressList::Data::~Data() {
  // Call either freeaddrinfo(head), or FreeMyAddrinfo(head), depending who
  // created the data.
  if (is_system_created)
    freeaddrinfo(head);
  else
    FreeMyAddrinfo(head);
}

}  // namespace net
