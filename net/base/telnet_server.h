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

#ifndef NET_BASE_TELNET_SERVER_H_
#define NET_BASE_TELNET_SERVER_H_

#include "net/base/listen_socket.h"

// Implements the telnet protocol on top of the raw socket interface.
// DidRead calls to the delegate are buffered on a line by line basis.
// (for now this means that basic line editing is handled in this object)
class TelnetServer : public ListenSocket {
public:
  static TelnetServer* Listen(std::string ip, int port,
                              ListenSocketDelegate *del,
                              MessageLoop* loop);
  virtual ~TelnetServer();

protected:
  void Listen() { ListenSocket::Listen(); }
  virtual void Read();
  virtual void Accept();
  virtual void SendInternal(const char* bytes, int len);

private:
  enum TelnetInputState {
    NOT_IN_IAC_OR_ESC_SEQUENCE,  // Currently not processing any IAC or ESC sequence.
    EXPECTING_NEW_LINE,  // Received carriage return (CR) expecting new line (LF).
    EXPECTING_COMMAND,  // Processing IAC expecting command.
    EXPECTING_OPTION,  // Processing IAC expecting option.
    SUBNEGOTIATION_EXPECTING_IAC,  // Inside subnegoation IAC,SE will end it.
    SUBNEGOTIATION_EXPECTING_SE,  // Ending subnegoation expecting SE.
    EXPECTING_FIRST_ESC_CHARACTER,  // Processing ESC sequence.
    EXPECTING_SECOND_ESC_CHARACTER,  // Processing ESC sequence with two characters
    EXPECTING_NUMBER_SEMICOLON_OR_END  // Processing "ESC [" sequence.
  };

  TelnetServer(SOCKET s, ListenSocketDelegate* del, MessageLoop* loop);

  // telnet commands
  void SendIAC(int command, int option);
  void StateMachineStep(unsigned char c);

  TelnetInputState input_state_;
  int iac_command_;  // Last command read.
  int iac_option_;  // Last option read.
  std::string command_line_;

  DISALLOW_EVIL_CONSTRUCTORS(TelnetServer);
};

#endif // BASE_TELNET_SERVER_H_