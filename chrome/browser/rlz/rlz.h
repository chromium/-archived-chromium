// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RLZ_RLZ_H__
#define CHROME_BROWSER_RLZ_RLZ_H__

#include <string>

#include "base/basictypes.h"

// RLZ is a library which is used to measure distribution scenarios.
// Its job is to record certain lifetime events in the registry and to send
// them encoded as a compact string at most once per day. The sent data does
// not contain information that can be used to identify a user or to infer
// browsing habits. The API in this file is a wrapper to rlz.dll which can be
// removed of the system with no adverse effects on chrome.
// For partner or bundled installs, the RLZ might send more information
// according to the terms disclosed in the EULA. In the Chromium build the
// rlz.dll is not present so all the functionality becomes no-ops.

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

  // Invoked during shutdown to clean up any state created by RLZTracker.
  static void CleanupRlz();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(RLZTracker);
};

#endif  // CHROME_BROWSER_RLZ_RLZ_H__

