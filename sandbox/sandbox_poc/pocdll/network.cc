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

#include "sandbox/sandbox_poc/pocdll/exports.h"
#include "sandbox/sandbox_poc/pocdll/utils.h"

// This file contains the tests used to verify the security of the network.

void POCDLL_API TestNetworkListen(HANDLE log) {
  HandleToFile handle2file;
  FILE *output = handle2file.Translate(log, "w");
#if DONT_WANT_INTERCEPTIONS_JUST_WANT_NETWORK
  // Initialize Winsock
  WSADATA wsa_data;
  int result = ::WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (result != NO_ERROR) {
    fprintf(output, "[ERROR] Cannot initialize winsock. Error%d\r\n", result);
    return;
  }

  // Create a SOCKET for listening for
  // incoming connection requests.
  SOCKET listen_socket;
  listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listen_socket == INVALID_SOCKET) {
    fprintf(output, "[ERROR] Failed to create socket. Error %ld\r\n",
           ::WSAGetLastError());
    ::WSACleanup();
    return;
  }

  // The sockaddr_in structure specifies the address family,
  // IP address, and port for the socket that is being bound.
  sockaddr_in service;
  service.sin_family = AF_INET;
  service.sin_addr.s_addr = inet_addr("127.0.0.1");
  service.sin_port = htons(88);

  if (bind(listen_socket, reinterpret_cast<SOCKADDR*>(&service),
           sizeof(service)) == SOCKET_ERROR) {
    fprintf(output, "[BLOCKED] Bind socket on port 88. Error %ld\r\n",
            ::WSAGetLastError());
    closesocket(listen_socket);
    ::WSACleanup();
    return;
  }

  // Listen for incoming connection requests
  // on the created socket
  if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
    fprintf(output, "[BLOCKED] Listen socket on port 88. Error %ld\r\n",
            ::WSAGetLastError());

  } else {
    fprintf(output, "[GRANTED] Listen socket on port 88.\r\n",
            ::WSAGetLastError());
  }

  ::WSACleanup();
  return;
#else  // DONT_WANT_INTERCEPTIONS_JUST_WANT_NETWORK
  // Just print out that this test is not running.
  fprintf(output, "[ERROR] No network tests.\r\n");
#endif  // DONT_WANT_INTERCEPTIONS_JUST_WANT_NETWORK
}
