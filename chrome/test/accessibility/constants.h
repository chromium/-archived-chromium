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

#ifndef CHROME_TEST_ACCISSIBILITY_CONSTANTS_H__
#define CHROME_TEST_ACCISSIBILITY_CONSTANTS_H__

#include <windows.h>
#include <tchar.h>

///////////////////////////////////////////////////////////////////
// Constant Definitations specific to Chrome Accessibility Tests.
///////////////////////////////////////////////////////////////////

// Safe delete and release operations.
#define CHK_RELEASE(obj)  { if (obj) { (obj)->Release(); (obj) = NULL; } }
#define CHK_DELETE(obj)  { if (obj) { delete (obj); (obj) = NULL; } }


// Chrome Accessibility Tests specific strings.
#define CHROME_PATH         _T("C:\\Program Files\\Google\\Chrome\\Chrome.exe")
#define CHROME_VIEWS_TEXT_FIELD_EDIT    _T("ChromeViewsTextFieldEdit")
#define CHROME_AUTOCOMPLETE_EDIT        _T("Chrome_AutocompleteEdit")
#define CHROME_VIEWS_NATIVE_CTRL_CONTNR _T("ChromeViewsNativeControlContainer")
#define CHROME_HWND_VIEW_CONTAINER      _T("Chrome_HWNDViewContainer_0")
#define STD_BUTTON                      _T("Button")
#define AUTH_TITLE                      _T("Authentication Required - Chrome")
#define CHROME_TAB_CONTENTS             _T("Chrome_TabContents")

#define BROWSER_WIN_ROLE                _T("window")
#define BROWSER_APP_ROLE                _T("application")
#define BROWSER_CLIENT_ROLE             _T("client")

#define CHROME_APP_ACC_INDEX            (3)
#define CHROME_CLIENT_ACC_INDEX         (0)

// Chrome Client chidren.
#define BROWSER_VIEW_ACC_INDEX          (0)
#define TABSTRIP_ACC_INDEX              (1)
#define CHROME_MIN_ACC_INDEX            (4)
#define CHROME_MAX_ACC_INDEX            (5)
#define CHROME_RESTORE_ACC_INDEX        (6)
#define CHROME_CLOSE_ACC_INDEX          (7)

// Browser View children.
#define TOOLBAR_ACC_INDEX               (0)

// Toolbar children.
#define BACK_BTN_INDEX                  (0)
#define FORWARD_BTN_INDEX               (1)
#define RELOAD_BTN_INDEX                (2)
#define STAR_BTN_INDEX                  (4)
#define GO_BTN_INDEX                    (6)
#define PAGE_BTN_INDEX                  (7)
#define MENU_BTN_INDEX                  (8)

// Digit limits for tab index which can be used in accelerator.
#define MAX_TAB_INDEX_DIGIT             (9)
#define MIN_TAB_INDEX_DIGIT             (1)

// Object Names.
#define BROWSER_STR                      _T("browser")
#define TOOLBAR_STR                      _T("toolbar")
#define TABSTRIP_STR                     _T("tabstrip")
#define TAB_STR                          _T("tab")
#define BROWSER_VIEW_STR                 _T("browser_view")

// Enums for keyboard keys. These values are directed to virtual-key values.
enum KEYBD_KEYS {
    KEY_F3      = VK_F3,
    KEY_F4      = VK_F4,
    KEY_F5      = VK_F5,
    KEY_F6      = VK_F6,
    KEY_ALT     = VK_MENU,
    KEY_CONTROL = VK_CONTROL,
    KEY_SHIFT   = VK_SHIFT,
    KEY_ENTER   = VK_RETURN,
    KEY_TAB     = VK_TAB,
    KEY_BACK    = VK_BACK,
    KEY_HOME    = VK_HOME,
    KEY_END     = VK_END,
    KEY_ESC     = VK_ESCAPE,
    KEY_INSERT  = VK_INSERT,
    KEY_DELETE  = VK_DELETE,
    KEY_LEFT    = VK_LEFT,
    KEY_RIGHT   = VK_RIGHT,
    KEY_PLUS    = VK_ADD,
    KEY_MINUS   = VK_SUBTRACT,
    KEY_0       = '0',
    KEY_1       = '1',
    KEY_2       = '2',
    KEY_3       = '3',
    KEY_4       = '4',
    KEY_5       = '5',
    KEY_6       = '6',
    KEY_7       = '7',
    KEY_8       = '8',
    KEY_9       = '9',
    KEY_D       = 'D',
    KEY_F       = 'F',
    KEY_G       = 'G',
    KEY_K       = 'K',
    KEY_L       = 'L',
    KEY_N       = 'N',
    KEY_O       = 'O',
    KEY_R       = 'R',
    KEY_T       = 'T',
    KEY_W       = 'W',
    KEY_INVALID = -1
};


#endif  // CHROME_TEST_ACCISSIBILITY_CONSTANTS_H__
