// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/standard_menus.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "chrome/app/chrome_dll_resource.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

struct MenuCreateMaterial zoom_menu_materials[] = {
  { MENU_NORMAL, IDC_ZOOM_PLUS, IDS_ZOOM_PLUS, 0, NULL,
    GDK_KP_Add, GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_ZOOM_NORMAL, IDS_ZOOM_NORMAL, 0, NULL,
    GDK_KP_0, GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_ZOOM_MINUS, IDS_ZOOM_MINUS, 0, NULL,
    GDK_KP_Subtract, GDK_CONTROL_MASK },
  { MENU_END }
};

struct MenuCreateMaterial encoding_menu_materials[] = {
  { MENU_CHECKBOX, IDC_ENCODING_AUTO_DETECT, IDS_ENCODING_AUTO_DETECT },
  { MENU_END }
};

struct MenuCreateMaterial developer_menu_materials[] = {
  { MENU_NORMAL, IDC_VIEW_SOURCE, IDS_VIEW_SOURCE, 0, NULL,
    GDK_u, GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_JS_CONSOLE, IDS_JS_CONSOLE, 0, NULL,
    GDK_j, GDK_CONTROL_MASK | GDK_SHIFT_MASK },
  { MENU_NORMAL, IDC_TASK_MANAGER, IDS_TASK_MANAGER, 0, NULL,
    GDK_Escape, GDK_SHIFT_MASK },
  { MENU_END }
};

struct MenuCreateMaterial standard_page_menu_materials[] = {
  { MENU_NORMAL, IDC_CREATE_SHORTCUTS, IDS_CREATE_SHORTCUTS },
  { MENU_SEPARATOR },
  { MENU_NORMAL, IDC_CUT, IDS_CUT, 0, NULL, GDK_x, GDK_CONTROL_MASK, true },
  { MENU_NORMAL, IDC_COPY, IDS_COPY, 0, NULL, GDK_c, GDK_CONTROL_MASK, true },
  { MENU_NORMAL, IDC_PASTE, IDS_PASTE, 0, NULL, GDK_v, GDK_CONTROL_MASK, true },
  { MENU_SEPARATOR },
  { MENU_NORMAL, IDC_FIND, IDS_FIND, 0, NULL, GDK_f, GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_SAVE_PAGE, IDS_SAVE_PAGE, 0, NULL, GDK_s,
    GDK_CONTROL_MASK },
  // Printing hasn't been implemented yet.  Remove it from the menu until
  // someone implements it.
  // http://code.google.com/p/chromium/issues/detail?id=9847
  //{ MENU_NORMAL, IDC_PRINT, IDS_PRINT, 0, NULL, GDK_p, GDK_CONTROL_MASK },
  { MENU_SEPARATOR },
  { MENU_NORMAL, IDC_ZOOM_MENU, IDS_ZOOM_MENU, 0, zoom_menu_materials },
  { MENU_NORMAL, IDC_ENCODING_MENU, IDS_ENCODING_MENU, 0,
    encoding_menu_materials },
  { MENU_SEPARATOR },
  { MENU_NORMAL, IDC_DEVELOPER_MENU, IDS_DEVELOPER_MENU, 0,
    developer_menu_materials },
  { MENU_SEPARATOR },
  { MENU_NORMAL, IDC_REPORT_BUG, IDS_REPORT_BUG },
  { MENU_END }
};

// -----------------------------------------------------------------------

struct MenuCreateMaterial standard_app_menu_materials[] = {
  { MENU_NORMAL, IDC_NEW_TAB, IDS_NEW_TAB, 0, NULL,
    GDK_t, GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_NEW_WINDOW, IDS_NEW_WINDOW, 0, NULL,
    GDK_n, GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_NEW_INCOGNITO_WINDOW, IDS_NEW_INCOGNITO_WINDOW, 0, NULL,
    GDK_n, GDK_CONTROL_MASK | GDK_SHIFT_MASK},
  { MENU_SEPARATOR },
  { MENU_CHECKBOX, IDC_SHOW_BOOKMARK_BAR, IDS_SHOW_BOOKMARK_BAR, 0, NULL,
    GDK_b, GDK_CONTROL_MASK },
  { MENU_SEPARATOR },
  { MENU_NORMAL, IDC_SHOW_HISTORY, IDS_SHOW_HISTORY, 0, NULL,
    GDK_h, GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_SHOW_BOOKMARK_MANAGER, IDS_BOOKMARK_MANAGER, 0, NULL,
    GDK_b, GDK_CONTROL_MASK | GDK_SHIFT_MASK },
  { MENU_NORMAL, IDC_SHOW_DOWNLOADS, IDS_SHOW_DOWNLOADS, 0, NULL,
    GDK_j, GDK_CONTROL_MASK },
  { MENU_SEPARATOR },
  // TODO(erg): P13N stuff goes here as soon as they get IDS strings.
  { MENU_NORMAL, IDC_CLEAR_BROWSING_DATA, IDS_CLEAR_BROWSING_DATA },
  { MENU_NORMAL, IDC_IMPORT_SETTINGS, IDS_IMPORT_SETTINGS },
  { MENU_SEPARATOR },
  { MENU_NORMAL, IDC_OPTIONS, IDS_OPTIONS, IDS_PRODUCT_NAME },
  { MENU_NORMAL, IDC_ABOUT, IDS_ABOUT, IDS_PRODUCT_NAME },
  { MENU_NORMAL, IDC_HELP_PAGE, IDS_HELP_PAGE, 0, NULL, GDK_F1 },
  { MENU_SEPARATOR },
  { MENU_NORMAL, IDC_EXIT, IDS_EXIT, 0, NULL, GDK_q, GDK_CONTROL_MASK },
  { MENU_END }
};
}  // namespace

const MenuCreateMaterial* GetStandardPageMenu() {
  return standard_page_menu_materials;
}

const MenuCreateMaterial* GetStandardAppMenu() {
  return standard_app_menu_materials;
}
