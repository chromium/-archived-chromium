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

#include "chrome/common/ipc_message_macros.h"

IPC_BEGIN_MESSAGES(TestMsg, 8)
  // out1 is false
  IPC_SYNC_MESSAGE_CONTROL0_1(Msg_C_0_1, bool)

  // out1 is true, out2 is 2
  IPC_SYNC_MESSAGE_CONTROL0_2(Msg_C_0_2, bool, int)

  // out1 is false, out2 is 3, out3 is "0_3"
  IPC_SYNC_MESSAGE_CONTROL0_3(Msg_C_0_3, bool, int, std::string)

  // in1 must be 1, out1 is true
  IPC_SYNC_MESSAGE_CONTROL1_1(Msg_C_1_1, int, bool)

  // in1 must be false, out1 is true, out2 is 12
  IPC_SYNC_MESSAGE_CONTROL1_2(Msg_C_1_2, bool, bool, int)

  // in1 must be 3, out1 is "1_3", out2 is 13, out3 is false
  IPC_SYNC_MESSAGE_CONTROL1_3(Msg_C_1_3, int, std::string, int, bool)

  // in1 must be 1, in2 must be false, out1 is true
  IPC_SYNC_MESSAGE_CONTROL2_1(Msg_C_2_1, int, bool, bool)

  // in1 must be false, in2 must be 2, out1 is true, out2 is 22
  IPC_SYNC_MESSAGE_CONTROL2_2(Msg_C_2_2, bool, int, bool, int)

  // in1 must be 3, in2 must be true, out1 is "2_3", out2 is 23, out3 is false
  IPC_SYNC_MESSAGE_CONTROL2_3(Msg_C_2_3, int, bool, std::string, int, bool)

  // in1 must be 1, in2 must be false, in3 must be "3_1", out1 is true
  IPC_SYNC_MESSAGE_CONTROL3_1(Msg_C_3_1, int, bool, std::string, bool)

  // in1 must be "3_3", in2 must be false, in3 must be 2, out1 is true, out2 is 32
  IPC_SYNC_MESSAGE_CONTROL3_2(Msg_C_3_2, std::string, bool, int, bool, int)

  // in1 must be 3, in2 must be "3_3", in3 must be true, out1 is "3_3", out2 is 33, out3 is false
  IPC_SYNC_MESSAGE_CONTROL3_3(Msg_C_3_3, int, std::string, bool, std::string, int, bool)


  // NOTE: routed messages are just a copy of the above...

  // out1 is false
  IPC_SYNC_MESSAGE_ROUTED0_1(Msg_R_0_1, bool)

  // out1 is true, out2 is 2
  IPC_SYNC_MESSAGE_ROUTED0_2(Msg_R_0_2, bool, int)

  // out1 is false, out2 is 3, out3 is "0_3"
  IPC_SYNC_MESSAGE_ROUTED0_3(Msg_R_0_3, bool, int, std::string)

  // in1 must be 1, out1 is true
  IPC_SYNC_MESSAGE_ROUTED1_1(Msg_R_1_1, int, bool)

  // in1 must be false, out1 is true, out2 is 12
  IPC_SYNC_MESSAGE_ROUTED1_2(Msg_R_1_2, bool, bool, int)

  // in1 must be 3, out1 is "1_3", out2 is 13, out3 is false
  IPC_SYNC_MESSAGE_ROUTED1_3(Msg_R_1_3, int, std::string, int, bool)

  // in1 must be 1, in2 must be false, out1 is true
  IPC_SYNC_MESSAGE_ROUTED2_1(Msg_R_2_1, int, bool, bool)

  // in1 must be false, in2 must be 2, out1 is true, out2 is 22
  IPC_SYNC_MESSAGE_ROUTED2_2(Msg_R_2_2, bool, int, bool, int)

  // in1 must be 3, in2 must be true, out1 is "2_3", out2 is 23, out3 is false
  IPC_SYNC_MESSAGE_ROUTED2_3(Msg_R_2_3, int, bool, std::string, int, bool)

  // in1 must be 1, in2 must be false, in3 must be "3_1", out1 is true
  IPC_SYNC_MESSAGE_ROUTED3_1(Msg_R_3_1, int, bool, std::string, bool)

  // in1 must be "3_3", in2 must be false, in3 must be 2, out1 is true, out2 is 32
  IPC_SYNC_MESSAGE_ROUTED3_2(Msg_R_3_2, std::string, bool, int, bool, int)

  // in1 must be 3, in2 must be "3_3", in3 must be true, out1 is "3_3", out2 is 33, out3 is false
  IPC_SYNC_MESSAGE_ROUTED3_3(Msg_R_3_3, int, std::string, bool, std::string, int, bool)

IPC_END_MESSAGES(TestMsg)
