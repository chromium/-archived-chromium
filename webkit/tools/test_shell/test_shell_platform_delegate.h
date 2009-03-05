// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TestShellPlatformDelegate isolates a variety of platform-specific
// functions so that code can invoke them by purpose without resorting to
// ifdefs or runtime platform checks.  Each platform should define an
// implementation of this class.  In many cases, implementation of methods
// in this class will be stubs on platforms where those functions are
// unnecessary.

class TestShellPlatformDelegate {
 public:
  // The TestShellPlatformDelegate object is scoped to main(), and so
  // its constructor is a good place to put per-application initialization
  // (as opposed to per-test code, which should go into the TestShell class).
  TestShellPlatformDelegate(const CommandLine& command_line);
  ~TestShellPlatformDelegate();

  // EnableMemoryDebugging: turn on platform memory debugging assistance
  // (console messages, heap checking, leak detection, etc.).
  void EnableMemoryDebugging();

  // CheckLayoutTestSystemDependencies: check for any system dependencies that
  // can't be easily overridden from within an application (for example, UI or
  // display settings).  Returns false if any dependencies are not met.
  bool CheckLayoutTestSystemDependencies();

  // PreflightArgs: give the platform first crack at the arguments to main()
  // before we parse the command line.  For example, some UI toolkits have
  // runtime flags that they can pre-filter.
  static void PreflightArgs(int* argc, char*** argv);

  // SuppressErrorReporting: if possible, turn off platform error reporting
  // dialogs, crash dumps, etc.
  void SuppressErrorReporting();

  // InitializeGUI: do any special initialization that the UI needs before
  // we start the main message loop
  void InitializeGUI();

  // SelectUnifiedTheme: override user preferences so that the UI theme matches
  // what's in the baseline files.  Whenever possible, override user settings
  // here rather than testing them in CheckLayoutTestSystemDependencies.
  void SelectUnifiedTheme();

  // AboutToExit: give the platform delegate a last chance to restore platform
  // settings.  Normally called by the destructor, but also called before
  // abort() (example: test timeouts).
  void AboutToExit();

  // SetWindowPositionForRecording: if the platform's implementation of
  // EventRecorder requires the window to be in a particular absolute position,
  // make it so.  This is called by TestShell after it creates the window.
  void SetWindowPositionForRecording(TestShell *shell);
 private:
  const CommandLine& command_line_;
};
