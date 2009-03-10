// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEARS_INTEGRATION_H__
#define CHROME_BROWSER_GEARS_INTEGRATION_H__

#include <string>

#include "base/gfx/native_widget_types.h"
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
struct GearsCreateShortcutData : public GearsShortcutData2 {
  CPCommandInterface* command_interface;
};

// Called when the Gears Settings button is pressed. |parent_wnd| is the
// window the Gears Settings dialog should be parented to.
void GearsSettingsPressed(gfx::NativeWindow parent_wnd);

// Calls into the Gears API to create a shortcut with the given parameters.
// 'app_info' is the optional information provided by the page.  If any info is
// missing, we fallback to the given fallback params.  'fallback_icon' must be a
// 16x16 favicon.  'callback' will be called with a value indicating whether the
// shortcut has been created successfully.
typedef Callback2<const GearsShortcutData2&, bool>::Type
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
