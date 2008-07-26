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
//
// This file contains helper functions for getting strings that are included in
// our DLL for all languages (i.e., does not come from our language DLL).
//
// These resource strings are organized such that we can get a localized string
// by taking the base resource ID and adding a language offset.  For example,
// to get the resource id for the localized product name in en-US, we take
// IDS_PRODUCT_NAME_BASE + IDS_L10N_OFFSET_EN_US.

#ifndef CHROME_INSTALLER_UTIL_L10N_STRING_UTIL_H_
#define CHROME_INSTALLER_UTIL_L10N_STRING_UTIL_H_

#include <string>

namespace installer_util {

// Given a string base id, return the localized version of the string based on
// the system language.  This is used for shortcuts placed on the user's
// desktop.
std::wstring GetLocalizedString(int base_message_id);

}

#endif  // CHROME_INSTALLER_UTIL_L10N_STRING_UTIL_H_
