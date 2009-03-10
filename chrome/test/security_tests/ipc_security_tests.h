// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SECURITY_TESTS_IPC_SECURITY_TESTS_H__
#define CHROME_TEST_SECURITY_TESTS_IPC_SECURITY_TESTS_H__

// Impersonates a chrome server pipe. See the implementation for details.
// Returns false if the attack could not be set. If it returns true then
// it spawns a thread that will terminate the renderer if the attack is
// successful.
bool PipeImpersonationAttack();

#endif  // CHROME_TEST_SECURITY_TESTS_IPC_SECURITY_TESTS_H__
