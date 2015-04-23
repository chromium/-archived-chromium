// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Calculate Crc by calling CRC method in LZMA SDK

#include "courgette/crc.h"

extern "C" {
#include "third_party/lzma_sdk/7zCrc.h"
}

namespace courgette {

uint32 CalculateCrc(const uint8* buffer, size_t size) {
  CrcGenerateTable();
  uint32 crc = 0xffffffffL;
  crc = ~CrcCalc(buffer, size);
  return crc;
}

}  // namespace
