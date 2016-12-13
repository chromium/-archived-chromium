// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"
#include "base/scoped_nsobject.h"
#include "chrome/common/pref_member.h"

@class CustomHomePagesModel;
class PrefObserverBridge;
class PrefService;
class Profile;
@class SearchEngineListModel;

// A window controller that handles the preferences window. The bulk of the
// work is handled via Cocoa Bindings and getter/setter methods that wrap
// cross-platform PrefMember objects. When prefs change in the back-end
// (that is, outside of this UI), our observer recieves a notification and can
// tickle the KVO to update the UI so we are always in sync. The bindings are
// specified in the nib file. Preferences are persisted into the back-end
// as they are changed in the UI, and are thus immediately available even while
// the window is still open. When the window closes, a notification is sent
// via the system NotificationCenter. This can be used as a signal to
// release this controller, as it's likely the client wants to enforce there
// only being one (we don't do that internally as it makes it very difficult
// to unit test).
@interface PreferencesWindowController : NSWindowController {
 @private
  Profile* profile_;  // weak ref
  PrefService* prefs_;  // weak ref - Obtained from profile_ for convenience.
  scoped_ptr<PrefObserverBridge> observer_;  // Watches for pref changes.
  IBOutlet NSTabView* tabView_;
  IBOutlet NSArrayController* customPagesArrayController_;

  // Basics panel
  IntegerPrefMember restoreOnStartup_;
  scoped_nsobject<CustomHomePagesModel> customPagesSource_;
  BooleanPrefMember newTabPageIsHomePage_;
  StringPrefMember homepage_;
  BooleanPrefMember showHomeButton_;
  BooleanPrefMember showPageOptionButtons_;
  scoped_nsobject<SearchEngineListModel> searchEngineModel_;
  // Used when creating a new home page url to make the new cell editable.
  BOOL pendingSelectForEdit_;

  // User Data panel
  BooleanPrefMember askSavePasswords_;
  BooleanPrefMember formAutofill_;

  // Under the hood panel
  IBOutlet NSView* advancedView_;
  IBOutlet NSScrollView* advancedScroller_;
  BooleanPrefMember alternateErrorPages_;
  BooleanPrefMember useSuggest_;
  BooleanPrefMember dnsPrefetch_;
  BooleanPrefMember safeBrowsing_;
  BooleanPrefMember metricsRecording_;
  IntegerPrefMember cookieBehavior_;
}

// Designated initializer. |profile| should not be NULL.
- (id)initWithProfile:(Profile*)profile;

// Show the preferences window.
- (void)showPreferences:(id)sender;

// IBAction methods for responding to user actions.

// Basics panel
- (IBAction)makeDefaultBrowser:(id)sender;
- (IBAction)addHomepage:(id)sender;
- (IBAction)removeSelectedHomepages:(id)sender;
- (IBAction)useCurrentPagesAsHomepage:(id)sender;

// User Data panel
- (IBAction)showSavedPasswords:(id)sender;
- (IBAction)importData:(id)sender;
- (IBAction)clearData:(id)sender;
- (IBAction)resetTheme:(id)sender;

// Usable from cocoa bindings to hook up the custom home pages table.
@property(readonly) CustomHomePagesModel* customPagesSource;

// NSNotification sent when the prefs window is closed.
extern NSString* const kUserDoneEditingPrefsNotification;

@end
