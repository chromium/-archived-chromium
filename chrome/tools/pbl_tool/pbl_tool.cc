// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This tool manages privacy blacklists. Primarily for loading a text
// blacklist into the binary agregate blacklist.
#include <iostream>

#include "base/process_util.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"

int main(int argc, char* argv[]) {
  base::EnableTerminationOnHeapCorruption();
  std::cout << "Aw, Snap! This is not implemented yet." << std::endl;
  CHECK(std::string() == Blacklist::StripCookies(std::string()));
}
