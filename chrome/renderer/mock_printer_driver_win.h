// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MOCK_PRINTER_DRIVER_WIN_H_
#define CHROME_RENDERER_MOCK_PRINTER_DRIVER_WIN_H_

#include "base/basictypes.h"

class MockPrinterPage;

// Implements the platform-dependent part of a pseudo printer object.
// This class renders a platform-dependent input and creates a MockPrinterPage
// instance.
class MockPrinterDriverWin {
 public:
  MockPrinterDriverWin();
  ~MockPrinterDriverWin();

  MockPrinterPage* LoadSource(const void* data, size_t size);

  DISALLOW_COPY_AND_ASSIGN(MockPrinterDriverWin);
};

#endif  // CHROME_RENDERER_MOCK_PRINTER_DRIVER_WIN_H_
