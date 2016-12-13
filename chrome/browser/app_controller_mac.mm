// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/app_controller_mac.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browser_window.h"
#import "chrome/browser/cocoa/about_window_controller.h"
#import "chrome/browser/cocoa/bookmark_menu_bridge.h"
#import "chrome/browser/cocoa/clear_browsing_data_controller.h"
#import "chrome/browser/cocoa/encoding_menu_controller_delegate_mac.h"
#import "chrome/browser/cocoa/menu_localizer.h"
#import "chrome/browser/cocoa/preferences_window_controller.h"
#import "chrome/browser/cocoa/tab_strip_controller.h"
#import "chrome/browser/cocoa/tab_window_controller.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/temp_scaffolding_stubs.h"

@interface AppController(PRIVATE)
- (void)initMenuState;
- (void)openURLs:(const std::vector<GURL>&)urls;
- (void)openPendingURLs;
- (void)getUrl:(NSAppleEventDescriptor*)event
     withReply:(NSAppleEventDescriptor*)reply;
- (void)openFiles:(NSAppleEventDescriptor*)event
        withReply:(NSAppleEventDescriptor*)reply;
- (void)windowLayeringDidChange:(NSNotification*)inNotification;
@end

@implementation AppController

// This method is called very early in application startup (ie, before
// the profile is loaded or any preferences have been registered). Defer any
// user-data initialization until -applicationDidFinishLaunching:.
- (void)awakeFromNib {
  pendingURLs_.reset(new std::vector<GURL>());

  // We need to register the handlers early to catch events fired on launch.
  NSAppleEventManager* em = [NSAppleEventManager sharedAppleEventManager];
  [em setEventHandler:self
          andSelector:@selector(getUrl:withReply:)
        forEventClass:kInternetEventClass
           andEventID:kAEGetURL];
  [em setEventHandler:self
          andSelector:@selector(getUrl:withReply:)
        forEventClass:'WWW!'    // A particularly ancient AppleEvent that dates
           andEventID:'OURL'];  // back to the Spyglass days.
  [em setEventHandler:self
          andSelector:@selector(openFiles:withReply:)
        forEventClass:kCoreEventClass
           andEventID:kAEOpenDocuments];

  // Register for various window layering changes. We use these to update
  // various UI elements (command-key equivalents, etc) when the frontmost
  // window changes.
  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];
  [notificationCenter
      addObserver:self
         selector:@selector(windowLayeringDidChange:)
             name:NSWindowDidBecomeKeyNotification
           object:nil];
  [notificationCenter
      addObserver:self
         selector:@selector(windowLayeringDidChange:)
             name:NSWindowDidResignKeyNotification
           object:nil];
  [notificationCenter
      addObserver:self
         selector:@selector(windowLayeringDidChange:)
             name:NSWindowDidBecomeMainNotification
           object:nil];
  [notificationCenter
      addObserver:self
         selector:@selector(windowLayeringDidChange:)
             name:NSWindowDidResignMainNotification
           object:nil];

  // Register for a notification that the number of tabs changes in windows
  // so we can adjust the close tab/window command keys.
  [notificationCenter
      addObserver:self
         selector:@selector(tabsChanged:)
             name:kTabStripNumberOfTabsChanged
           object:nil];

  // Set up the command updater for when there are no windows open
  [self initMenuState];
}

// Called when the app is shutting down. Clean-up as appropriate.
- (void)applicationWillTerminate:(NSNotification *)aNotification {
  DCHECK(!BrowserList::HasBrowserWithProfile([self defaultProfile]));
  if (!BrowserList::HasBrowserWithProfile([self defaultProfile])) {
    // As we're shutting down, we need to nuke the TabRestoreService, which will
    // start the shutdown of the NavigationControllers and allow for proper
    // shutdown. If we don't do this chrome won't shutdown cleanly, and may end
    // up crashing when some thread tries to use the IO thread (or another
    // thread) that is no longer valid.
    [self defaultProfile]->ResetTabRestoreService();
  }

  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

// Helper routine to get the window controller if the key window is a tabbed
// window, or nil if not. Examples of non-tabbed windows are "about" or
// "preferences".
- (TabWindowController*)keyWindowTabController {
  NSWindowController* keyWindowController =
      [[[NSApplication sharedApplication] keyWindow] windowController];
  if ([keyWindowController isKindOfClass:[TabWindowController class]])
    return (TabWindowController*)keyWindowController;

  return nil;
}

// If the window has tabs, make "close window" be cmd-shift-w, otherwise leave
// it as the normal cmd-w. Capitalization of the key equivalent affects whether
// the shift modifer is used.
- (void)adjustCloseWindowMenuItemKeyEquivalent:(BOOL)inHaveTabs {
  [closeWindowMenuItem_ setKeyEquivalent:(inHaveTabs ? @"W" : @"w")];
}

// If the window has tabs, make "close tab" take over cmd-w, otherwise it
// shouldn't have any key-equivalent because it should be disabled.
- (void)adjustCloseTabMenuItemKeyEquivalent:(BOOL)hasTabs {
  if (hasTabs) {
    [closeTabMenuItem_ setKeyEquivalent:@"w"];
    [closeTabMenuItem_ setKeyEquivalentModifierMask:NSCommandKeyMask];
  } else {
    [closeTabMenuItem_ setKeyEquivalent:@""];
    [closeTabMenuItem_ setKeyEquivalentModifierMask:0];
  }
}

// See if we have a window with tabs open, and adjust the key equivalents for
// Close Tab/Close Window accordingly
- (void)fixCloseMenuItemKeyEquivalents {
  TabWindowController* tabController = [self keyWindowTabController];
  BOOL windowWithMultipleTabs =
      (tabController && [tabController numberOfTabs] > 1);
  [self adjustCloseWindowMenuItemKeyEquivalent:windowWithMultipleTabs];
  [self adjustCloseTabMenuItemKeyEquivalent:windowWithMultipleTabs];
  fileMenuUpdatePending_ = NO;
}

// Fix up the "close tab/close window" command-key equivalents. We do this
// after a delay to ensure that window layer state has been set by the time
// we do the enabling.
- (void)delayedFixCloseMenuItemKeyEquivalents {
  if (!fileMenuUpdatePending_) {
    [self performSelector:@selector(fixCloseMenuItemKeyEquivalents)
               withObject:nil
               afterDelay:0];
    fileMenuUpdatePending_ = YES;
  }
}

// Called when we get a notification about the window layering changing to
// update the UI based on the new main window.
- (void)windowLayeringDidChange:(NSNotification*)notify {
  [self delayedFixCloseMenuItemKeyEquivalents];

  // TODO(pinkerton): If we have other things here, such as inspector panels
  // that follow the contents of the selected webpage, we would update those
  // here.
}

// Called when the number of tabs changes in one of the browser windows. The
// object is the tab strip controller, but we don't currently care.
- (void)tabsChanged:(NSNotification*)notify {
  [self delayedFixCloseMenuItemKeyEquivalents];
}

// If the auto-update interval is not set, make it 5 hours.
// This code is specific to Mac Chrome Dev Channel.
// Placed here for 2 reasons:
// 1) Same spot as other Pref stuff
// 2) Try and be friendly by keeping this after app launch
// TODO(jrg): remove once we go Beta.
- (void)setUpdateCheckInterval {
#if defined(GOOGLE_CHROME_BUILD)
  CFStringRef app = (CFStringRef)@"com.google.Keystone.Agent";
  CFStringRef checkInterval = (CFStringRef)@"checkInterval";
  CFPropertyListRef plist = CFPreferencesCopyAppValue(checkInterval, app);
  if (!plist) {
    const float fiveHoursInSeconds = 5.0 * 60.0 * 60.0;
    NSNumber *value = [NSNumber numberWithFloat:fiveHoursInSeconds];
    CFPreferencesSetAppValue(checkInterval, value, app);
    CFPreferencesAppSynchronize(app);
  }
#endif
}

// This is called after profiles have been loaded and preferences registered.
// It is safe to access the default profile here.
- (void)applicationDidFinishLaunching:(NSNotification*)notify {
  // Hold an extra ref to the BrowserProcess singleton so it doesn't go away
  // when all the browser windows get closed. We'll release it on quit which
  // will be the signal to exit.
  DCHECK(g_browser_process);
  g_browser_process->AddRefModule();

  // Create the localizer for the main menu. We can't do this in the nib
  // because it's too early. Do it before we create any bookmark menus as well,
  // just in case one has a title that matches any of our strings (unlikely,
  // but technically possible).
  scoped_nsobject<MenuLocalizer> localizer(
      [[MenuLocalizer alloc] initWithBundle:nil]);
  [localizer localizeObject:[NSApplication sharedApplication]
                recursively:YES];

  bookmarkMenuBridge_.reset(new BookmarkMenuBridge());

  [self setUpdateCheckInterval];

  // Build up the encoding menu, the order of the items differs based on the
  // current locale (see http://crbug.com/7647 for details).
  // We need a valid g_browser_process to get the profile which is why we can't
  // call this from awakeFromNib.
  EncodingMenuControllerDelegate::BuildEncodingMenu([self defaultProfile]);

  // Now that we're initialized we can open any URLs we've been holding onto.
  [self openPendingURLs];
}

// We can't use the standard terminate: method because it will abruptly exit
// the app and leave things on the stack in an unfinalized state. We need to
// post a quit message to our run loop so the stack can gracefully unwind.
- (IBAction)quit:(id)sender {
  // TODO(pinkerton):
  // since we have to roll it ourselves, ask the delegate (ourselves, really)
  // if we should terminate. For example, we might not want to if the user
  // has ongoing downloads or multiple windows/tabs open. However, this would
  // require posting UI and may require spinning up another run loop to
  // handle it. If it says to continue, post the quit message, otherwise
  // go back to normal.

  NSAppleEventManager* em = [NSAppleEventManager sharedAppleEventManager];
  [em removeEventHandlerForEventClass:kInternetEventClass
                           andEventID:kAEGetURL];
  [em removeEventHandlerForEventClass:'WWW!'
                           andEventID:'OURL'];
  [em removeEventHandlerForEventClass:kCoreEventClass
                           andEventID:kAEOpenDocuments];

  // TODO(pinkerton): Not sure where this should live, including it here
  // causes all sorts of asserts from the open renderers. On Windows, it
  // lives in Browser::OnWindowClosing, but that's not appropriate on Mac
  // since we don't shut down when we reach zero windows.
  // browser_shutdown::OnShutdownStarting(browser_shutdown::WINDOW_CLOSE);

  // Close all the windows.
  BrowserList::CloseAllBrowsers(true);

  // Release the reference to the browser process. Once all the browsers get
  // dealloc'd, it will stop the RunLoop and fall back into main().
  g_browser_process->ReleaseModule();
}

// Called to determine if we should enable the "restore tab" menu item.
// Checks with the TabRestoreService to see if there's anything there to
// restore and returns YES if so.
- (BOOL)canRestoreTab {
  TabRestoreService* service = [self defaultProfile]->GetTabRestoreService();
  return service && !service->entries().empty();
}

// Called to validate menu items when there are no key windows. All the
// items we care about have been set with the |commandDispatch:| action and
// a target of FirstResponder in IB. If it's not one of those, let it
// continue up the responder chain to be handled elsewhere. We pull out the
// tag as the cross-platform constant to differentiate and dispatch the
// various commands.
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];
  BOOL enable = NO;
  if (action == @selector(commandDispatch:)) {
    NSInteger tag = [item tag];
    if (menuState_->SupportsCommand(tag)) {
      switch (tag) {
        case IDC_RESTORE_TAB:
          enable = [self canRestoreTab];
          break;
        default:
          enable = menuState_->IsCommandEnabled(tag) ? YES : NO;
      }
    }
  } else if (action == @selector(quit:)) {
    enable = YES;
  } else if (action == @selector(showPreferences:)) {
    enable = YES;
  } else if (action == @selector(orderFrontStandardAboutPanel:)) {
    enable = YES;
  }
  return enable;
}

// Called when the user picks a menu item when there are no key windows. Calls
// through to the browser object to execute the command. This assumes that the
// command is supported and doesn't check, otherwise it would have been disabled
// in the UI in validateUserInterfaceItem:.
- (void)commandDispatch:(id)sender {
  Profile* defaultProfile = [self defaultProfile];

  NSInteger tag = [sender tag];
  switch (tag) {
    case IDC_NEW_TAB:
    case IDC_NEW_WINDOW:
      Browser::OpenEmptyWindow(defaultProfile);
      break;
    case IDC_NEW_INCOGNITO_WINDOW:
      Browser::OpenURLOffTheRecord(defaultProfile, GURL());
      break;
    case IDC_RESTORE_TAB:
      Browser::OpenWindowWithRestoredTabs(defaultProfile);
      break;
    case IDC_OPEN_FILE:
      Browser::OpenEmptyWindow(defaultProfile);
      BrowserList::GetLastActive()->
          ExecuteCommandWithDisposition(IDC_OPEN_FILE, CURRENT_TAB);
      break;
    case IDC_CLEAR_BROWSING_DATA:
      // There may not be a browser open, so use the default profile.
      scoped_nsobject<ClearBrowsingDataController> controller(
          [[ClearBrowsingDataController alloc]
              initWithProfile:defaultProfile]);
      [controller runModalDialog];
      break;
  };
}

// NSApplication delegate method called when someone clicks on the
// dock icon and there are no open windows.  To match standard mac
// behavior, we should open a new window.
- (BOOL)applicationShouldHandleReopen:(NSApplication*)theApplication
                    hasVisibleWindows:(BOOL)flag {
  // Don't do anything if there are visible windows.  This will cause
  // AppKit to unminimize the most recently minimized window.
  if (flag)
    return YES;

  // Otherwise open a new window.
  Browser::OpenEmptyWindow([self defaultProfile]);

  // We've handled the reopen event, so return NO to tell AppKit not
  // to do anything.
  return NO;
}

- (void)initMenuState {
  menuState_.reset(new CommandUpdater(NULL));
  menuState_->UpdateCommandEnabled(IDC_NEW_TAB, true);
  menuState_->UpdateCommandEnabled(IDC_NEW_WINDOW, true);
  menuState_->UpdateCommandEnabled(IDC_NEW_INCOGNITO_WINDOW, true);
  menuState_->UpdateCommandEnabled(IDC_OPEN_FILE, true);
  menuState_->UpdateCommandEnabled(IDC_CLEAR_BROWSING_DATA, true);
  menuState_->UpdateCommandEnabled(IDC_RESTORE_TAB, false);
  // TODO(pinkerton): ...more to come...
}

- (Profile*)defaultProfile {
  // TODO(jrg): Find a better way to get the "default" profile.
  if (g_browser_process->profile_manager())
    return* g_browser_process->profile_manager()->begin();

  return NULL;
}

// Various methods to open URLs that we get in a native fashion. We use
// BrowserInit here because on the other platforms, URLs to open come through
// the ProcessSingleton, and it calls BrowserInit. It's best to bottleneck the
// openings through that for uniform handling.

- (void)openURLs:(const std::vector<GURL>&)urls {
  if (pendingURLs_.get()) {
    // too early to open; save for later
    pendingURLs_->insert(pendingURLs_->end(), urls.begin(), urls.end());
    return;
  }

  Browser* browser = BrowserList::GetLastActive();
  // if no browser window exists then create one with no tabs to be filled in
  if (!browser) {
    browser = Browser::Create([self defaultProfile]);
    browser->window()->Show();
  }

  CommandLine dummy((std::wstring()));
  BrowserInit::LaunchWithProfile launch(std::wstring(), dummy);
  launch.OpenURLsInBrowser(browser, false, urls);
}

- (void)openPendingURLs {
  // Since the existence of pendingURLs_ is a flag that it's too early to
  // open URLs, we need to reset pendingURLs_.
  std::vector<GURL> urls;
  swap(urls, *pendingURLs_);
  pendingURLs_.reset();

  if (urls.size())
    [self openURLs:urls];
}

- (void)getUrl:(NSAppleEventDescriptor*)event
     withReply:(NSAppleEventDescriptor*)reply {
  NSString* urlStr = [[event paramDescriptorForKeyword:keyDirectObject]
                      stringValue];

  GURL gurl(base::SysNSStringToUTF8(urlStr));
  std::vector<GURL> gurlVector;
  gurlVector.push_back(gurl);

  [self openURLs:gurlVector];
}

- (void)openFiles:(NSAppleEventDescriptor*)event
        withReply:(NSAppleEventDescriptor*)reply {
  // Ordinarily we'd use the NSApplication delegate method
  // -application:openFiles:, but Cocoa tries to be smart and it sends files
  // specified on the command line into that delegate method. That's too smart
  // for us (our setup isn't done by the time Cocoa triggers the delegate method
  // and we crash). Since all we want are files dropped on the app icon, and we
  // have cross-platform code to handle the command-line files anyway, an Apple
  // Event handler fits the bill just right.
  NSAppleEventDescriptor* fileList =
      [event paramDescriptorForKeyword:keyDirectObject];
  if (!fileList)
    return;
  std::vector<GURL> gurlVector;

  for (NSInteger i = 1; i <= [fileList numberOfItems]; ++i) {
    NSAppleEventDescriptor* fileAliasDesc = [fileList descriptorAtIndex:i];
    if (!fileAliasDesc)
      continue;
    NSAppleEventDescriptor* fileURLDesc =
        [fileAliasDesc coerceToDescriptorType:typeFileURL];
    if (!fileURLDesc)
      continue;
    NSData* fileURLData = [fileURLDesc data];
    if (!fileURLData)
      continue;
    GURL gurl(std::string((char*)[fileURLData bytes], [fileURLData length]));
    gurlVector.push_back(gurl);
  }

  [self openURLs:gurlVector];
}

// Called when the preferences window is closed. We use this to release the
// window controller.
- (void)prefsWindowClosed:(NSNotification*)notify {
  [[NSNotificationCenter defaultCenter]
    removeObserver:self
              name:kUserDoneEditingPrefsNotification
            object:prefsController_.get()];
  prefsController_.reset(NULL);
}

// Show the preferences window, or bring it to the front if it's already
// visible.
- (IBAction)showPreferences:(id)sender {
  if (!prefsController_.get()) {
    prefsController_.reset([[PreferencesWindowController alloc]
                              initWithProfile:[self defaultProfile]]);
    // Watch for a notification of when it goes away so that we can destroy
    // the controller.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(prefsWindowClosed:)
               name:kUserDoneEditingPrefsNotification
             object:prefsController_.get()];
  }
  [prefsController_ showPreferences:sender];
}

// Called when the about window is closed. We use this to release the
// window controller.
- (void)aboutWindowClosed:(NSNotification*)notify {
  [[NSNotificationCenter defaultCenter]
    removeObserver:self
              name:kUserClosedAboutNotification
            object:aboutController_.get()];
  aboutController_.reset(NULL);
}

- (IBAction)orderFrontStandardAboutPanel:(id)sender {
#if !defined(GOOGLE_CHROME_BUILD)
  // If not branded behave like a generic Cocoa app.
  [NSApp orderFrontStandardAboutPanel:sender];
#else
  // Otherwise bring up our special dialog (e.g. with an auto-update button).
  if (!aboutController_) {
    aboutController_.reset([[AboutWindowController alloc]
                             initWithWindowNibName:@"About"]);
    if (!aboutController_) {
      // If we get here something is wacky.  I managed to do it when
      // testing by explicitly forcing an auto-update to an older
      // version then trying to open the about box again (missing
      // nib).  This shouldn't be possible in general but let's try
      // hard to not do nothing.
      [NSApp orderFrontStandardAboutPanel:sender];
      return;
    }
    // Watch for a notification of when it goes away so that we can destroy
    // the controller.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(aboutWindowClosed:)
               name:kUserClosedAboutNotification
             object:aboutController_.get()];
  }
  if (![[aboutController_ window] isVisible])
    [[aboutController_ window] center];
  [aboutController_ showWindow:self];
#endif
}

@end
