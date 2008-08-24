// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CLIENT_SOCKET_FACTORY_H_
#define NET_BASE_CLIENT_SOCKET_FACTORY_H_

#include <string>

namespace net {

class AddressList;
class ClientSocket;

// An interface used to instantiate ClientSocket objects.  Used to facilitate
// testing code with mock socket implementations.
class ClientSocketFactory {
 public:
  virtual ~ClientSocketFactory() {}

  virtual ClientSocket* CreateTCPClientSocket(
      const AddressList& addresses) = 0;

  virtual ClientSocket* CreateSSLClientSocket(
      ClientSocket* transport_socket,
      const std::string& hostname) = 0;

  // Returns the default ClientSocketFactory.
  static ClientSocketFactory* GetDefaultFactory();
};

}  // namespace net

#endif  // NET_BASE_CLIENT_SOCKET_FACTORY_H_

