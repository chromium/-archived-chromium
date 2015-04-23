// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares helper functions necessary to run reliablity test under
// UI test framework.

#ifndef CHROME_TEST_RELIABILITY_PAGE_LOAD_TEST_H__
#define CHROME_TEST_RELIABILITY_PAGE_LOAD_TEST_H__

#include "base/command_line.h"

// Parse the command line options and set the page range accordingly.
void SetPageRange(const CommandLine&);

#endif // CHROME_TEST_RELIABILITY_PAGE_LOAD_TEST_H__
