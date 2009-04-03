// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines messages between the browser and worker process, as well as between
// the renderer and worker process.

#ifndef CHROME_COMMON_WORKER_MESSAGES_H_
#define CHROME_COMMON_WORKER_MESSAGES_H_

#include <string>

#include "base/basictypes.h"
#include "ipc/ipc_message_utils.h"
#include "chrome/common/common_message_utils.h"

#if 0
// This for tools which parse #include lines, but cannot process when we
// include via a macro name.
#include "chrome/common/worker_messages_internal.h"
#endif
#define MESSAGES_INTERNAL_FILE "chrome/common/worker_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // CHROME_COMMON_WORKER_MESSAGES_H_
