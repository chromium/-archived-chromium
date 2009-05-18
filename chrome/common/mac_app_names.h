// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_APP_NAMES_H_
#define CHROME_COMMON_MAC_APP_NAMES_H_

#if defined(GOOGLE_CHROME_BUILD)
#define MAC_BROWSER_APP_NAME "Google Chrome.app"
#elif defined(CHROMIUM_BUILD)
#define MAC_BROWSER_APP_NAME "Chromium.app"
#else
#error "Not sure what the branding is!"
#endif

#endif  // CHROME_COMMON_MAC_APP_NAMES_H_
