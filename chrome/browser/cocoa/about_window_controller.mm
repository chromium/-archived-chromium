// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/file_version_info.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/app/keystone_glue.h"
#import "chrome/browser/cocoa/about_window_controller.h"
#include "grit/generated_resources.h"

NSString* const kUserClosedAboutNotification =
  @"kUserClosedAboutNotification";

@interface AboutWindowController (Private)
- (KeystoneGlue*)defaultKeystoneGlue;
@end


@implementation AboutWindowController

- (id)initWithWindowNibName:(NSString*)nibName {
  NSString* nibpath = [mac_util::MainAppBundle() pathForResource:nibName
                                                          ofType:@"nib"];
  self = [super initWithWindowNibPath:nibpath owner:self];
  return self;
}

- (void)awakeFromNib {
  // Set our current version.
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  NSString* version = base::SysWideToNSString(version_info->file_version());
  [version_ setStringValue:version];

  // Localize the update now button.
  NSString* updateNow = base::SysWideToNSString(
      l10n_util::GetString(IDS_ABOUT_CHROME_UPDATE_CHECK));
  [updateNowButton_ setStringValue:updateNow];

  // TODO(jrg): localize the rest of the elements:
  // - "Google Chrome" string at top
  // - copyright string at bottom

  // Initiate an update check.
  if ([[self defaultKeystoneGlue] checkForUpdate:self])
     [spinner_ startAnimation:self];
}

- (KeystoneGlue*)defaultKeystoneGlue {
  return [KeystoneGlue defaultKeystoneGlue];
}

// Callback from KeystoneGlue; implementation of KeystoneGlueCallbacks protocol.
// Warning: latest version may be nil if not set in server config.
- (void)upToDateCheckCompleted:(BOOL)updatesAvailable
                 latestVersion:(NSString*)latestVersion {
  [spinner_ stopAnimation:self];

  // If an update is available, be sure to enable the "update now" button.
  NSString* display = @"No updates are available.";
  if (updatesAvailable) {
    [updateNowButton_ setEnabled:YES];
    // TODO(jrg): IDS_UPGRADE_AVAILABLE seems close but there is no facility
    // for specifying a verision in the update string.  Do we care?
    display = latestVersion ?
        [NSString stringWithFormat:@"Version %@ is available for update.",
                   latestVersion] :
        @"A new version is available.";
  }
  [upToDate_ setStringValue:display];
}

- (void)windowWillClose:(NSNotification*)notification {
  // If an update has ever been triggered, we force reuse of the same About Box.
  // This gives us 2 things:
  // 1. If an update is ongoing and the window was closed we would have
  // no way of getting status.
  // 2. If we have a "Please restart" message we want it to stay there.
  if (updateTriggered_)
    return;

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kUserClosedAboutNotification
                    object:self];
}

// Callback from KeystoneGlue; implementation of KeystoneGlueCallbacks protocol.
- (void)updateCompleted:(BOOL)successful installs:(int)installs {
  [spinner_ stopAnimation:self];
  // TODO(jrg): localize.  No current string really says this.
  NSString* display = ((successful && installs) ?
                       @"Update completed!  Please restart Chrome." :
                       @"Self update failed.");
  [updateCompleted_ setStringValue:display];

  // Allow a second chance.
  if (!(successful && installs))
    [updateNowButton_ setEnabled:YES];
}

- (IBAction)updateNow:(id)sender {
  updateTriggered_ = YES;
  // Don't let someone click "Update Now" twice!
  [updateNowButton_ setEnabled:NO];
  [spinner_ startAnimation:self];
  if ([[self defaultKeystoneGlue] startUpdate:self])
    [spinner_ startAnimation:self];
  else {
    // TODO(jrg): localize.
    // IDS_UPGRADE_ERROR doesn't work here; we don't have an error number, and
    // "server not available" is too specific.
    [updateCompleted_ setStringValue:@"Failed to start updates."];
  }
}

- (NSButton*)updateButton {
  return updateNowButton_;
}

- (NSTextField*)upToDateTextField {
  return upToDate_;
}

- (NSTextField*)updateCompletedTextField {
  return updateCompleted_;
}

@end

