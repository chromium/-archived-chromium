// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_message_macros.h"

IPC_BEGIN_MESSAGES(Test)
  IPC_SYNC_MESSAGE_CONTROL0_0(SyncChannelTestMsg_NoArgs)

  IPC_SYNC_MESSAGE_CONTROL0_1(SyncChannelTestMsg_AnswerToLife,
                              int /* answer */)

  IPC_SYNC_MESSAGE_CONTROL1_1(SyncChannelTestMsg_Double,
                              int /* in */,
                              int /* out */)

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

  // in1 must be "3_3", in2 must be false, in3 must be 2, out1 is true, out2 is
  // 32
  IPC_SYNC_MESSAGE_CONTROL3_2(Msg_C_3_2, std::string, bool, int, bool, int)

  // in1 must be 3, in2 must be "3_3", in3 must be true, out1 is "3_3", out2 is
  // 33, out3 is false
  IPC_SYNC_MESSAGE_CONTROL3_3(Msg_C_3_3, int, std::string, bool, std::string,
                              int, bool)


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

  // in1 must be "3_3", in2 must be false, in3 must be 2, out1 is true, out2
  // is 32
  IPC_SYNC_MESSAGE_ROUTED3_2(Msg_R_3_2, std::string, bool, int, bool, int)

  // in1 must be 3, in2 must be "3_3", in3 must be true, out1 is "3_3", out2 is
  // 33, out3 is false
  IPC_SYNC_MESSAGE_ROUTED3_3(Msg_R_3_3, int, std::string, bool, std::string,
                             int, bool)

IPC_END_MESSAGES(TestMsg)
