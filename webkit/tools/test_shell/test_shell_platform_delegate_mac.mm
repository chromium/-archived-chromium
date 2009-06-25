// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

// #include <Carbon/Carbon.h>
// #include <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>
#import <objc/objc-runtime.h>
#include <mach/task.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_platform_delegate.h"
#include "webkit/tools/test_shell/test_shell_switches.h"
#include "WebSystemInterface.h"

static NSAutoreleasePool *gTestShellAutoreleasePool = nil;

static void SetDefaultsToLayoutTestValues(void) {
  // So we can match the WebKit layout tests, we want to force a bunch of
  // preferences that control appearance to match.
  // (We want to do this as early as possible in application startup so
  // the settings are in before any higher layers could cache values.)

  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

  const NSInteger kMinFontSizeCGSmoothes = 4;
  const NSInteger kNoFontSmoothing = 0;
  const NSInteger kBlueTintedAppearance = 1;
  [defaults setInteger:kMinFontSizeCGSmoothes
                forKey:@"AppleAntiAliasingThreshold"];
  [defaults setInteger:kNoFontSmoothing
                forKey:@"AppleFontSmoothing"];
  [defaults setInteger:kBlueTintedAppearance
                forKey:@"AppleAquaColorVariant"];
  [defaults setObject:@"0.709800 0.835300 1.000000"
               forKey:@"AppleHighlightColor"];
  [defaults setObject:@"0.500000 0.500000 0.500000"
               forKey:@"AppleOtherHighlightColor"];
  [defaults setObject:[NSArray arrayWithObject:@"en"]
               forKey:@"AppleLanguages"];

  // AppKit pulls scrollbar style from NSUserDefaults.  HIToolbox uses
  // CFPreferences, but AnyApplication, so we set it, force it to load, and
  // then reset the pref to what it was (HIToolbox will cache what it loaded).
  [defaults setObject:@"DoubleMax" forKey:@"AppleScrollBarVariant"];
  CFTypeRef initialValue =
      CFPreferencesCopyValue(CFSTR("AppleScrollBarVariant"),
                             kCFPreferencesAnyApplication,
                             kCFPreferencesCurrentUser,
                             kCFPreferencesAnyHost);
  CFPreferencesSetValue(CFSTR("AppleScrollBarVariant"),
                        CFSTR("DoubleMax"),
                        kCFPreferencesAnyApplication,
                        kCFPreferencesCurrentUser,
                        kCFPreferencesAnyHost);
  // Make HIToolbox read from CFPreferences
  ThemeScrollBarArrowStyle style;
  GetThemeScrollBarArrowStyle(&style);
  if (initialValue) {
    // Reset the preference to what it was
    CFPreferencesSetValue(CFSTR("AppleScrollBarVariant"),
                          initialValue,
                          kCFPreferencesAnyApplication,
                          kCFPreferencesCurrentUser,
                          kCFPreferencesAnyHost);
    CFRelease(initialValue);
  }
}

static void ClearAnyDefaultsForLayoutTests(void) {
  // Not running a test, clear the keys so the TestShell looks right to the
  // running user.

  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

  [defaults removeObjectForKey:@"AppleAntiAliasingThreshold"];
  [defaults removeObjectForKey:@"AppleFontSmoothing"];
  [defaults removeObjectForKey:@"AppleAquaColorVariant"];
  [defaults removeObjectForKey:@"AppleHighlightColor"];
  [defaults removeObjectForKey:@"AppleOtherHighlightColor"];
  [defaults removeObjectForKey:@"AppleLanguages"];
  [defaults removeObjectForKey:@"AppleScrollBarVariant"];
}

#if OBJC_API_VERSION == 2
static void SwizzleAllMethods(Class imposter, Class original) {
  unsigned int imposterMethodCount = 0;
  Method* imposterMethods = class_copyMethodList(imposter, &imposterMethodCount);

  unsigned int originalMethodCount = 0;
  Method* originalMethods = class_copyMethodList(original, &originalMethodCount);

  for (unsigned int i = 0; i < imposterMethodCount; i++) {
    SEL imposterMethodName = method_getName(imposterMethods[i]);

    // Attempt to add the method to the original class.  If it fails, the method
    // already exists and we should instead exchange the implementations.
    if (class_addMethod(original,
                        imposterMethodName,
                        method_getImplementation(originalMethods[i]),
                        method_getTypeEncoding(originalMethods[i]))) {
      continue;
    }

    unsigned int j = 0;
    for (; j < originalMethodCount; j++) {
      SEL originalMethodName = method_getName(originalMethods[j]);
      if (sel_isEqual(imposterMethodName, originalMethodName)) {
        break;
      }
    }

    // If class_addMethod failed above then the method must exist on the
    // original class.
    DCHECK(j < originalMethodCount) << "method wasn't found?";
    method_exchangeImplementations(imposterMethods[i], originalMethods[j]);
  }

  if (imposterMethods) {
    free(imposterMethods);
  }
  if (originalMethods) {
    free(originalMethods);
  }
}
#endif

static void SwizzleNSPasteboard(void) {
  // We replace NSPaseboard w/ the shim (from WebKit) that avoids having
  // sideeffects w/ whatever the user does at the same time.

  Class imposterClass = objc_getClass("DumpRenderTreePasteboard");
  Class originalClass = objc_getClass("NSPasteboard");
#if OBJC_API_VERSION == 0
  class_poseAs(imposterClass, originalClass);
#else
  // Swizzle instance methods...
  SwizzleAllMethods(imposterClass, originalClass);
  // and then class methods.
  SwizzleAllMethods(object_getClass(imposterClass),
                    object_getClass(originalClass));
#endif
}

TestShellPlatformDelegate::TestShellPlatformDelegate(
    const CommandLine &command_line)
    : command_line_(command_line) {
  gTestShellAutoreleasePool = [[NSAutoreleasePool alloc] init];
  InitWebCoreSystemInterface();
  // Force AppKit to init itself, but don't start the runloop yet
  [NSApplication sharedApplication];
  [NSBundle loadNibNamed:@"MainMenu" owner:NSApp];
}

TestShellPlatformDelegate::~TestShellPlatformDelegate() {
  [gTestShellAutoreleasePool release];
}

bool TestShellPlatformDelegate::CheckLayoutTestSystemDependencies() {
  return true;
}

void TestShellPlatformDelegate::InitializeGUI() {
  if (command_line_.HasSwitch(test_shell::kLayoutTests)) {
    // If we're doing automated testing, we won't be using a conventional
    // run loop, so tell Cocoa to finish initializing.
    [NSApp finishLaunching];
  } else {
    // Make sure any settings from a previous layout run are cleared
    ClearAnyDefaultsForLayoutTests();
  }
}

void TestShellPlatformDelegate::PreflightArgs(int *argc, char ***argv) {
}

void TestShellPlatformDelegate::SetWindowPositionForRecording(TestShell *) {
}

void TestShellPlatformDelegate::SelectUnifiedTheme() {
  SetDefaultsToLayoutTestValues();
  SwizzleNSPasteboard();
}

void TestShellPlatformDelegate::SuppressErrorReporting() {
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();

  // If we die during tests, we don't want to be spamming the user's crash
  // reporter. Set our exception port to null and add signal handlers.
  // Both of these are necessary to avoid the crash reporter. Although, we do
  // still seem to be missing some cases.
  if (!parsed_command_line.HasSwitch(test_shell::kGDB)) {
    task_set_exception_ports(mach_task_self(), EXC_MASK_ALL, MACH_PORT_NULL,
                             EXCEPTION_DEFAULT, THREAD_STATE_NONE);
  }
}
