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

// Constants for the names of various preferences, for easier changing.

#ifndef CHROME_COMMON_PREF_NAMES_H_
#define CHROME_COMMON_PREF_NAMES_H_

namespace prefs {

// Profile prefs
extern const wchar_t kHomePageIsNewTabPage[];
extern const wchar_t kHomePage[];
extern const wchar_t kProfileName[];
extern const wchar_t kProfileNickname[];
extern const wchar_t kProfileID[];
extern const wchar_t kRecentlyViewedModelBiasActiveTabHistory[];
extern const wchar_t kRecentlyViewedModelSelectionMode[];
extern const wchar_t kSessionExitedCleanly[];
extern const wchar_t kRestoreOnStartup[];
extern const wchar_t kURLsToRestoreOnStartup[];
extern const wchar_t kApplicationLocale[];
extern const wchar_t kDefaultCharset[];
extern const wchar_t kAcceptLanguages[];
extern const wchar_t kStaticEncodings[];
extern const wchar_t kShowBookmarkBar[];
extern const wchar_t kWebKitStandardFontIsSerif[];
extern const wchar_t kWebKitFixedFontFamily[];
extern const wchar_t kWebKitSerifFontFamily[];
extern const wchar_t kWebKitSansSerifFontFamily[];
extern const wchar_t kWebKitCursiveFontFamily[];
extern const wchar_t kWebKitFantasyFontFamily[];
extern const wchar_t kWebKitDefaultFontSize[];
extern const wchar_t kWebKitDefaultFixedFontSize[];
extern const wchar_t kWebKitMinimumFontSize[];
extern const wchar_t kWebKitMinimumLogicalFontSize[];
extern const wchar_t kWebKitJavascriptEnabled[];
extern const wchar_t kWebKitJavascriptCanOpenWindowsAutomatically[];
extern const wchar_t kWebKitLoadsImagesAutomatically[];
extern const wchar_t kWebKitPluginsEnabled[];
extern const wchar_t kWebKitDomPasteEnabled[];
extern const wchar_t kWebKitShrinksStandaloneImagesToFit[];
extern const wchar_t kWebKitDeveloperExtrasEnabled[];
extern const wchar_t kWebKitUsesUniversalDetector[];
extern const wchar_t kWebKitTextAreasAreResizable[];
extern const wchar_t kWebKitJavaEnabled[];
extern const wchar_t kAlwaysCreateDestinationsTab[];
extern const wchar_t kPasswordManagerEnabled[];
extern const wchar_t kSafeBrowsingEnabled[];
extern const wchar_t kSearchSuggestEnabled[];
extern const wchar_t kCookieBehavior[];
extern const wchar_t kMixedContentFiltering[];
extern const wchar_t kDefaultSearchProviderSearchURL[];
extern const wchar_t kDefaultSearchProviderSuggestURL[];
extern const wchar_t kDefaultSearchProviderName[];
extern const wchar_t kDefaultSearchProviderID[];
extern const wchar_t kBlockPopups[];
extern const wchar_t kPromptForDownload[];
extern const wchar_t kAlternateErrorPagesEnabled[];
extern const wchar_t kDnsPrefetchingEnabled[];
extern const wchar_t kDnsStartupPrefetchList[];
extern const wchar_t kIpcDisabledMessages[];
extern const wchar_t kShowHomeButton[];
extern const wchar_t kRecentlySelectedEncoding[];

// Local state
extern const wchar_t kAvailableProfiles[];

extern const wchar_t kMetricsClientID[];
extern const wchar_t kMetricsSessionID[];
extern const wchar_t kMetricsIsRecording[];
extern const wchar_t kMetricsClientIDTimestamp[];
extern const wchar_t kMetricsReportingEnabled[];
extern const wchar_t kMetricsInitialLogs[];
extern const wchar_t kMetricsOngoingLogs[];

extern const wchar_t kProfileMetrics[];
extern const wchar_t kProfilePrefix[];

extern const wchar_t kStabilityExitedCleanly[];
extern const wchar_t kStabilitySessionEndCompleted[];
extern const wchar_t kStabilityLaunchCount[];
extern const wchar_t kStabilityCrashCount[];
extern const wchar_t kStabilityIncompleteSessionEndCount[];
extern const wchar_t kStabilityPageLoadCount[];
extern const wchar_t kStabilityRendererCrashCount[];
extern const wchar_t kStabilityLaunchTimeSec[];
extern const wchar_t kStabilityLastTimestampSec[];
extern const wchar_t kStabilityUptimeSec[];
extern const wchar_t kStabilityRendererHangCount[];

extern const wchar_t kStabilityBreakpadRegistrationSuccess[];
extern const wchar_t kStabilityBreakpadRegistrationFail[];
extern const wchar_t kStabilityDebuggerPresent[];
extern const wchar_t kStabilityDebuggerNotPresent[];

extern const wchar_t kSecurityRendererOnSboxDesktop[];
extern const wchar_t kSecurityRendererOnDefaultDesktop[];

extern const wchar_t kStabilityPluginStats[];
extern const wchar_t kStabilityPluginPath[];
extern const wchar_t kStabilityPluginLaunches[];
extern const wchar_t kStabilityPluginInstances[];
extern const wchar_t kStabilityPluginCrashes[];

extern const wchar_t kStartRenderersManually[];
extern const wchar_t kBrowserWindowPlacement[];
extern const wchar_t kTaskManagerWindowPlacement[];
extern const wchar_t kPageInfoWindowPlacement[];
extern const wchar_t kMemoryCacheSize[];

extern const wchar_t kDownloadDefaultDirectory[];
extern const wchar_t kDownloadExtensionsToOpen[];

extern const wchar_t kSaveFileDefaultDirectory[];

extern const wchar_t kHungPluginDetectFrequency[];
extern const wchar_t kPluginMessageResponseTimeout[];

extern const wchar_t kSpellCheckDictionary[];

extern const wchar_t kExcludedSchemes[];

extern const wchar_t kSafeBrowsingClientKey[];
extern const wchar_t kSafeBrowsingWrappedKey[];

extern const wchar_t kOptionsWindowLastTabIndex[];
extern const wchar_t kShouldShowFirstRunBubble[];
extern const wchar_t kShouldShowWelcomePage[];

extern const wchar_t kLastKnownGoogleURL[];

extern const wchar_t kGeoIDAtInstall[];

extern const wchar_t kShutdownType[];
extern const wchar_t kShutdownNumProcesses[];
extern const wchar_t kShutdownNumProcessesSlow[];

extern const wchar_t kNumBookmarksOnBookmarkBar[];
extern const wchar_t kNumFoldersOnBookmarkBar[];
extern const wchar_t kNumBookmarksInOtherBookmarkFolder[];
extern const wchar_t kNumFoldersInOtherBookmarkFolder[];

extern const wchar_t kNumKeywords[];
}

#endif  // CHROME_COMMON_PREF_NAMES_H_
