// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/download_item_controller.h"

#include "app/l10n_util.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/cocoa/download_item_mac.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_shelf.h"

// A class for the chromium-side part of the download shelf context menu.

class DownloadShelfContextMenuMac : public DownloadShelfContextMenu {
 public:
  DownloadShelfContextMenuMac(BaseDownloadItemModel* model)
      : DownloadShelfContextMenu(model) { }

  using DownloadShelfContextMenu::ExecuteItemCommand;
  using DownloadShelfContextMenu::ItemIsChecked;
  using DownloadShelfContextMenu::IsItemCommandEnabled;

  using DownloadShelfContextMenu::SHOW_IN_FOLDER;
  using DownloadShelfContextMenu::OPEN_WHEN_COMPLETE;
  using DownloadShelfContextMenu::ALWAYS_OPEN_TYPE;
  using DownloadShelfContextMenu::CANCEL;
};

// Implementation of DownloadItemController

@implementation DownloadItemController

- (id)initWithFrame:(NSRect)frameRect
              model:(BaseDownloadItemModel*)downloadModel {
  if ((self = [super initWithNibName:@"DownloadItem"
                              bundle:mac_util::MainAppBundle()])) {
    // Must be called before [self view], so that bridge_ is set in awakeFromNib
    bridge_.reset(new DownloadItemMac(downloadModel, self));
    menuBridge_.reset(new DownloadShelfContextMenuMac(downloadModel));

    [[self view] setFrame:frameRect];
  }
  return self;
}

- (void)awakeFromNib {
  [self setStateFromDownload:bridge_->download_model()];
}

- (void)setStateFromDownload:(BaseDownloadItemModel*)downloadModel {
  // TODO(thakis): The windows version of this does all kinds of things
  // (gratituous use of animation, special handling of dangerous downloads)
  // that we don't currently do.

  // Set correct popup menu.
  if (downloadModel->download()->state() == DownloadItem::COMPLETE)
    [popupButton_ setMenu:completeDownloadMenu_];
  else
    [popupButton_ setMenu:activeDownloadMenu_];

  // Set name and icon of download.
  FilePath downloadPath = downloadModel->download()->GetFileName();

  // TODO(thakis): use filename eliding like gtk/windows versions.
  NSString* titleString = base::SysWideToNSString(downloadPath.ToWStringHack());
  [[popupButton_ itemAtIndex:0] setTitle:titleString];

  // TODO(paulg): Use IconManager for loading icons on the file thread
  // (crbug.com/16226).
  NSString* extension = base::SysUTF8ToNSString(downloadPath.Extension());
  [[popupButton_ itemAtIndex:0] setImage:
      [[NSWorkspace sharedWorkspace] iconForFileType:extension]];

  // Set status text.
  std::wstring statusText = downloadModel->GetStatusText();
  // Remove the status text label.
  if (statusText.empty()) {
    // TODO(thakis): Once there is a status label, hide it here.
    return;
  }

  // TODO(thakis): Set status_text as status label.
}

// Sets the enabled and checked state of a particular menu item for this
// download. We translate the NSMenuItem selection to menu selections understood
// by the non platform specific download context menu.
- (BOOL)validateMenuItem:(NSMenuItem *)item {
  SEL action = [item action];

  int actionId = 0;
  if (action == @selector(handleOpen:)) {
    actionId = DownloadShelfContextMenuMac::OPEN_WHEN_COMPLETE;
  } else if (action == @selector(handleAlwaysOpen:)) {
    actionId = DownloadShelfContextMenuMac::ALWAYS_OPEN_TYPE;
  } else if (action == @selector(handleReveal:)) {
    actionId = DownloadShelfContextMenuMac::SHOW_IN_FOLDER;
  } else if (action == @selector(handleCancel:)) {
    actionId = DownloadShelfContextMenuMac::CANCEL;
  } else {
    NOTREACHED();
    return YES;
  }

  if (menuBridge_->ItemIsChecked(actionId))
    [item setState:NSOnState];
  else
    [item setState:NSOffState];

  return menuBridge_->IsItemCommandEnabled(actionId) ? YES : NO;
}

- (IBAction)handleOpen:(id)sender {
  menuBridge_->ExecuteItemCommand(
      DownloadShelfContextMenuMac::OPEN_WHEN_COMPLETE);
}

- (IBAction)handleAlwaysOpen:(id)sender {
  menuBridge_->ExecuteItemCommand(
      DownloadShelfContextMenuMac::ALWAYS_OPEN_TYPE);
}

- (IBAction)handleReveal:(id)sender {
  menuBridge_->ExecuteItemCommand(DownloadShelfContextMenuMac::SHOW_IN_FOLDER);
}

- (IBAction)handleCancel:(id)sender {
  menuBridge_->ExecuteItemCommand(DownloadShelfContextMenuMac::CANCEL);
}

@end
