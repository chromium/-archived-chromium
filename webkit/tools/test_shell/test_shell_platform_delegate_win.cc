// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <commctrl.h>
#include "base/command_line.h"
#include "base/event_recorder.h"
#include "base/gfx/native_theme.h"
#include "base/resource_util.h"
#include "base/win_util.h"
#include "webkit/tools/test_shell/foreground_helper.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_platform_delegate.h"

TestShellPlatformDelegate::TestShellPlatformDelegate(
    const CommandLine& command_line)
    : command_line_(command_line) {
#ifdef _CRTDBG_MAP_ALLOC
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
#endif
}

TestShellPlatformDelegate::~TestShellPlatformDelegate() {
#ifdef _CRTDBG_MAP_ALLOC
  _CrtDumpMemoryLeaks();
#endif
}

void TestShellPlatformDelegate::PreflightArgs(int *argc, char ***argv) {
}

// This test approximates whether you have the Windows XP theme selected by
// inspecting a couple of metrics. It does not catch all cases, but it does
// pick up on classic vs xp, and normal vs large fonts. Something it misses
// is changes to the color scheme (which will infact cause pixel test
// failures).
//
// ** Expected dependencies **
// + Theme: Windows XP
// + Color scheme: Default (blue)
// + Font size: Normal
// + Font smoothing: off (minor impact).
//
static bool HasLayoutTestThemeDependenciesWin() {
  // This metric will be 17 when font size is "Normal". The size of drop-down
  // menus depends on it.
  if (::GetSystemMetrics(SM_CXVSCROLL) != 17)
    return false;

  // Check that the system fonts RenderThemeWin relies on are Tahoma 11 pt.
  NONCLIENTMETRICS metrics;
  win_util::GetNonClientMetrics(&metrics);
  LOGFONTW* system_fonts[] =
  { &metrics.lfStatusFont, &metrics.lfMenuFont, &metrics.lfSmCaptionFont };

  for (size_t i = 0; i < arraysize(system_fonts); ++i) {
    if (system_fonts[i]->lfHeight != -11 ||
        0 != wcscmp(L"Tahoma", system_fonts[i]->lfFaceName))
      return false;
  }
  return true;
}

bool TestShellPlatformDelegate::CheckLayoutTestSystemDependencies() {
  bool has_deps = HasLayoutTestThemeDependenciesWin();
  if (!has_deps) {
    fprintf(stderr,
            "\n"
            "###############################################################\n"
            "## Layout test system dependencies check failed.\n"
            "## Some layout tests may fail due to unexpected theme.\n"
            "##\n"
            "## To fix, go to Display Properties -> Appearance, and select:\n"
            "##  + Windows and buttons: Windows XP style\n"
            "##  + Color scheme: Default (blue)\n"
            "##  + Font size: Normal\n"
            "###############################################################\n");
  }
  return has_deps;
}

void TestShellPlatformDelegate::SuppressErrorReporting() {
  _set_abort_behavior(0, _WRITE_ABORT_MSG);
}

void TestShellPlatformDelegate::InitializeGUI() {
  INITCOMMONCONTROLSEX InitCtrlEx;

  InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
  InitCtrlEx.dwICC  = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&InitCtrlEx);
  TestShell::RegisterWindowClass();
}

void TestShellPlatformDelegate::SelectUnifiedTheme() {
  gfx::NativeTheme::instance()->DisableTheming();
}

void TestShellPlatformDelegate::SetWindowPositionForRecording(
    TestShell *shell) {
  // Move the window to the upper left corner for consistent
  // record/playback mode.  For automation, we want this to work
  // on build systems where the script invoking us is a background
  // process.  So for this case, make our window the topmost window
  // as well.
  ForegroundHelper::SetForeground(shell->mainWnd());
  ::SetWindowPos(shell->mainWnd(), HWND_TOP, 0, 0, 600, 800, 0);
}
