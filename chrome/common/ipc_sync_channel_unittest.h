// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_message_macros.h"

// Messages used for IPC::SyncChannel unit test
IPC_BEGIN_MESSAGES(SyncChannelTest, 9)
  IPC_SYNC_MESSAGE_CONTROL0_0(SyncChannelTestMsg_NoArgs)

  IPC_SYNC_MESSAGE_CONTROL0_1(SyncChannelTestMsg_AnswerToLife,
                              int /* answer */)

  IPC_SYNC_MESSAGE_CONTROL1_1(SyncChannelTestMsg_Double,
                              int /* in */,
                              int /* out */)

IPC_END_MESSAGES(SyncChannelTest)

