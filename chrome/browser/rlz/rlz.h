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

#ifndef CHROME_BROWSER_RLZ_RLZ_H__
#define CHROME_BROWSER_RLZ_RLZ_H__

#include <string>

#include "base/basictypes.h"

// RLZ is a library which is used to measure partner distribution deals.
// Its job is to record certain lifetime events in the registry and to send
// them encoded as a compact string at most once per day. The sent data does
// not contain information that can be used to identify a user or to infer
// browsing habits. The API in this file is a wrapper to rlz.dll which can be
// removed of the system with no adverse effects on chrome.

class RLZTracker {

 public:
  // An Access Point offers a way to search using Google. Other products
  // have specific entries here so do not remove the reserved access points.
  enum AccessPoint {
    NO_ACCESS_POINT = 0,
    RESERVED_ACCESS_POINT_01,
    RESERVED_ACCESS_POINT_02,
    RESERVED_ACCESS_POINT_03,
    RESERVED_ACCESS_POINT_04,
    RESERVED_ACCESS_POINT_05,
    RESERVED_ACCESS_POINT_06,
    RESERVED_ACCESS_POINT_07,
    RESERVED_ACCESS_POINT_08,
    CHROME_OMNIBOX,
    CHROME_HOME_PAGE,
    LAST_ACCESS_POINT
  };

  // A product is an entity which wants to gets credit for setting an access
  // point. Currently only the browser itself is supported but installed apps
  // could have their own entry here.
  enum Product {
    RESERVED_PRODUCT_01 = 1,
    RESERVED_PRODUCT_02,
    RESERVED_PRODUCT_03,
    RESERVED_PRODUCT_04,
    CHROME,
    LAST_PRODUCT
  };

  // Life cycle events. Some of them are applicable to all access points.
  enum Event {
    INVALID_EVENT = 0,
    INSTALL = 1,
    SET_TO_GOOGLE,
    FIRST_SEARCH,
    REPORT_RLS,
    LAST_EVENT
  };

  // Initializes the RLZ library services. 'directory_key' indicates the base
  // directory the RLZ dll would be found. For example base::DIR_CURRENT.
  // If the RLZ dll is not found in this directory the code falls back to try
  // to load it from base::DIR_EXE.
  // Returns false if the dll could not be loaded and initialized.
  // This function is intended primarily for testing.
  static bool InitRlz(int directory_key);

  // Like InitRlz() this function the RLZ library services for use in chrome.
  // Besides binding the dll, it schedules a delayed task that performs the
  // daily ping and registers the some events when 'first-run' is true.
  static bool InitRlzDelayed(int directory_key, bool first_run);

  // Records an RLZ event. Some events can be access point independent.
  // Returns false it the event could not be recorded. Requires write access
  // to the HKCU registry hive on windows.
  static bool RecordProductEvent(Product product, AccessPoint point,
                                 Event event_id);

  // Get the RLZ value of the access point.
  // Returns false if the rlz string could not be obtained. In some cases
  // an empty string can be returned which is not an error.
  static bool GetAccessPointRlz(AccessPoint point, std::wstring* rlz);

  // Clear all events reported by this product. In Chrome this will be called
  // when it is un-installed.
  static bool ClearAllProductEvents(Product product);

  // Called once a day to report the events to the server and to get updated
  // RLZs for the different access points. This call uses Wininet to perform
  // the http request. Returns true if the transaction succeeded and returns
  // false if the transaction failed OR if a previous ping request was done
  // in the last 24 hours.
  static bool SendFinancialPing(Product product,
                                const wchar_t* product_signature,
                                const wchar_t* product_brand,
                                const wchar_t* product_id,
                                const wchar_t* product_lang);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(RLZTracker);
};

#endif  // CHROME_BROWSER_RLZ_RLZ_H__
