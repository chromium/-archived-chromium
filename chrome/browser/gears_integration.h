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

#ifndef CHROME_BROWSER_GEARS_INTEGRATION_H__
#define CHROME_BROWSER_GEARS_INTEGRATION_H__

#include <string>

#include "base/task.h"
#include "chrome/common/gears_api.h"

// TODO(michaeln): Rework this interface to match how other first class
// citizens of chrome are structured, as a GearsService with an accessor
// available via browser.gears_service().

class CPCommandInterface;
class GURL;
class SkBitmap;
namespace webkit_glue {
struct WebApplicationInfo;
}

// We use this in place of GearsShortcutData so we can keep browser-specific
// data on the structure.
struct GearsCreateShortcutData : public GearsShortcutData {
  CPCommandInterface* command_interface;
};

// Called when the Gears Settings button is pressed. |parent_hwnd| is the
// window the Gears Settings dialog should be parented to.
void GearsSettingsPressed(HWND parent_hwnd);

// Calls into the Gears API to create a shortcut with the given parameters.
// 'app_info' is the optional information provided by the page.  If any info is
// missing, we fallback to the given fallback params.  'fallback_icon' must be a
// 16x16 favicon.  'callback' will be called with a value indicating whether the
// shortcut has been created successfully.
typedef Callback2<const GearsShortcutData&, bool>::Type
    GearsCreateShortcutCallback;

void GearsCreateShortcut(
    const webkit_glue::WebApplicationInfo& app_info,
    const std::wstring& fallback_name,
    const GURL& fallback_url,
    const SkBitmap& fallback_icon,
    GearsCreateShortcutCallback* callback);

// Call into Gears to query the list of shortcuts.  Results will be returned
// asynchronously via the callback.  The callback's arguments will be NULL
// if there was an error.
typedef Callback1<GearsShortcutList*>::Type GearsQueryShortcutsCallback;

void GearsQueryShortcuts(GearsQueryShortcutsCallback* callback);

// When the Gears shortcut database is modified, the main thread is notified
// via the NotificationService, NOTIFY_WEB_APP_INSTALL_CHANGED.

#endif  // CHROME_BROWSER_GEARS_INTEGRATION_H__
