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

// Defines all the command-line switches used by Chrome.

#ifndef CHROME_COMMON_CHROME_SWITCHES_H__
#define CHROME_COMMON_CHROME_SWITCHES_H__

#include "base/base_switches.h"

namespace switches {

extern const wchar_t kDisableHangMonitor[];
extern const wchar_t kDisableMetrics[];
extern const wchar_t kDisableMetricsReporting[];
extern const wchar_t kBrowserAssertTest[];
extern const wchar_t kRendererAssertTest[];
extern const wchar_t kBrowserCrashTest[];
extern const wchar_t kRendererCrashTest[];
extern const wchar_t kRendererStartupDialog[];
extern const wchar_t kPluginStartupDialog[];
extern const wchar_t kTestShellStartupDialog[];
extern const wchar_t kPluginLauncher[];

extern const wchar_t kProcessChannelID[];
extern const wchar_t kTestingChannelID[];
extern const wchar_t kHomePage[];
extern const wchar_t kBrowserStartRenderersManually[];
extern const wchar_t kRendererProcess[];
extern const wchar_t kRendererPath[];
extern const wchar_t kPluginProcess[];
extern const wchar_t kSingleProcess[];
extern const wchar_t kProcessPerTab[];
extern const wchar_t kProcessPerSite[];
extern const wchar_t kInProcessPlugins[];
extern const wchar_t kNoSandbox[];
extern const wchar_t kSafePlugins[];
extern const wchar_t kTrustedPlugins[];
extern const wchar_t kTestSandbox[];
extern const wchar_t kUserDataDir[];
extern const wchar_t kApp[];
extern const wchar_t kAppUploadFile[];
extern const wchar_t kDomAutomationController[];
extern const wchar_t kPluginPath[];
extern const wchar_t kJavaScriptFlags[];
extern const wchar_t kGeoID[];
extern const wchar_t kLang[];
extern const wchar_t kDebugChildren[];
extern const wchar_t kWaitForDebuggerChildren[];
extern const wchar_t kLogFilterPrefix[];
extern const wchar_t kEnableLogging[];
extern const wchar_t kDumpHistogramsOnExit[];
extern const wchar_t kDisableLogging[];
extern const wchar_t kRemoteShellPort[];
extern const wchar_t kUninstall[];
extern const wchar_t kOmniBoxPopupCount[];

extern const wchar_t kAutomationClientChannelID[];

extern const wchar_t kRestoreLastSession[];

extern const wchar_t kRecordMode[];
extern const wchar_t kPlaybackMode[];
extern const wchar_t kNoEvents[];

extern const wchar_t kHideIcons[];
extern const wchar_t kShowIcons[];
extern const wchar_t kMakeDefaultBrowser[];

extern const wchar_t kProxyServer[];
extern const wchar_t kDebugPrint[];

extern const wchar_t kDnsLogDetails[];
extern const wchar_t kDnsPrefetchDisable[];

extern const wchar_t kAllowAllActiveX[];

extern const wchar_t kDisableDevTools[];
extern const wchar_t kAlwaysEnableDevTools[];

extern const wchar_t kTabCountToLoadOnSessionRestore[];

extern const wchar_t kMemoryProfiling[];
extern const wchar_t kMemoryModel[];

extern const wchar_t kEnableFileCookies[];

extern const wchar_t kStartMaximized[];

extern const wchar_t kEnableWatchdog[];

extern const wchar_t kFirstRun[];

extern const wchar_t kMessageLoopStrategy[];
extern const wchar_t kMessageLoopHistogrammer[];

extern const wchar_t kImport[];

extern const wchar_t kSilentDumpOnDCHECK[];

extern const wchar_t kDisablePromptOnRepost[];

extern const wchar_t kDisablePopupBlocking[];
extern const wchar_t kDisableJavaScript[];
extern const wchar_t kDisableJava[];
extern const wchar_t kDisablePlugins[];
extern const wchar_t kDisableImages[];

extern const wchar_t kUseLowFragHeapCrt[];

#ifndef NDEBUG
extern const wchar_t kGearsPluginPathOverride[];
#endif

extern const wchar_t kUseNewHttp[];

extern const wchar_t kJavaScriptDebuggerPath[];

}  // namespace switches

#endif  // CHROME_COMMON_CHROME_SWITCHES_H__
