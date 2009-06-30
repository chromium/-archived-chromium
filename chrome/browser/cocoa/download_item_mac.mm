// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/download_item_mac.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/download_shelf_controller.h"
#import "chrome/browser/cocoa/download_shelf_mac.h"
#import "chrome/browser/cocoa/download_shelf_view.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_shelf.h"

// A class for the chromium-side part of the download shelf context menu.

class DownloadShelfContextMenuMac : public DownloadShelfContextMenu {
 public:
  DownloadShelfContextMenuMac(BaseDownloadItemModel* model,
      DownloadShelfContextMenuBridge* bridge)
      : DownloadShelfContextMenu(model), bridge_(bridge) {
  }

  NSMenu* GetCocoaMenu();

  using DownloadShelfContextMenu::ExecuteItemCommand;

 private:
  DownloadShelfContextMenuBridge* bridge_;  // weak, owns us
};

NSMenu* DownloadShelfContextMenuMac::GetCocoaMenu() {
  // TODO(thakis): win/gtk show slightly different menus when the download is
  // in progress/done (mainly the first item)
  // TODO(thakis): this probably wants to be in a xib file or at least use
  // localized strings

  NSMenuItem* item;
  NSMenu* menu =
      [[[NSMenu alloc] initWithTitle:@"DownloadItemPopup"] autorelease];
  SEL action = @selector(performAction:);

  item = [menu addItemWithTitle:@"Open" action:action keyEquivalent:@""];
  //  [item addItemWithTitle:@"Open when complete" ...];  // In-progress text.
  [item setTag:OPEN_WHEN_COMPLETE];
  [item setTarget:bridge_];

  // TODO(thakis): Set correct checkbox state, make this a checkbox item
  item = [menu addItemWithTitle:@"Always open type"
                         action:action
                  keyEquivalent:@""];
  [item setTag:ALWAYS_OPEN_TYPE];
  [item setTarget:bridge_];

  [menu addItem:[NSMenuItem separatorItem]];

  item = [menu addItemWithTitle:@"Reveal in Finder"
                         action:action
                  keyEquivalent:@""];
  [item setTag:SHOW_IN_FOLDER];
  [item setTarget:bridge_];

  [menu addItem:[NSMenuItem separatorItem]];

  item = [menu addItemWithTitle:@"Cancel" action:action keyEquivalent:@""];
  [item setTag:CANCEL];
  [item setTarget:bridge_];

  return menu;
}


// A class for the cocoa side of the download shelf context menu.

@interface DownloadShelfContextMenuBridge : NSObject {
 @private
  scoped_ptr<DownloadShelfContextMenuMac> contextMenu_;
}

- (DownloadShelfContextMenuBridge*)initWithModel:(BaseDownloadItemModel*)model;
@end

@interface DownloadShelfContextMenuBridge(Private)
- (void)performAction:(id)sender;
- (NSMenu*)menu;
@end

@implementation DownloadShelfContextMenuBridge

- (DownloadShelfContextMenuBridge*)initWithModel:(BaseDownloadItemModel*)model {
  if ((self = [super init]) == nil) {
    return nil;
  }
  contextMenu_.reset(new DownloadShelfContextMenuMac(model, self));
  return self;
}

@end

@implementation DownloadShelfContextMenuBridge(Private)

- (void)performAction:(id)sender {
  contextMenu_->ExecuteItemCommand([sender tag]);
}

- (NSMenu*)menu {
  return contextMenu_->GetCocoaMenu();
}

@end


// DownloadItemMac -------------------------------------------------------------

DownloadItemMac::DownloadItemMac(BaseDownloadItemModel* download_model,
                                 NSRect frame,
                                 DownloadShelfController* parent)
    : download_model_(download_model), parent_(parent) {
  download_model_->download()->AddObserver(this);

  // TODO(thakis): The windows version of this does all kinds of things
  // (gratituous use of animation, special handling of dangerous downloads)
  // that we don't currently do.

  scoped_nsobject<NSPopUpButton> view(
      [[NSPopUpButton alloc] initWithFrame:frame pullsDown:YES]);
  [parent_ addDownloadItem:view.get()];

  FilePath download_path = download_model->download()->GetFileName();

  // TODO(thakis): use filename eliding like gtk/windows versions
  NSString* titleString = base::SysWideToNSString(
      download_path.ToWStringHack());

  menu_.reset([[DownloadShelfContextMenuBridge alloc]
      initWithModel:download_model_.get()]);
  [view.get() setMenu:[menu_.get() menu]];

  [view.get() insertItemWithTitle:titleString atIndex:0];

  NSString* extension = base::SysUTF8ToNSString(download_path.Extension());
  [[view.get() itemAtIndex:0] setImage:
      [[NSWorkspace sharedWorkspace] iconForFileType:extension]];
}

DownloadItemMac::~DownloadItemMac() {
  download_model_->download()->RemoveObserver(this);
}

void DownloadItemMac::OnDownloadUpdated(DownloadItem* download) {
  DCHECK_EQ(download, download_model_->download());

  std::wstring status_text = download_model_->GetStatusText();
  // Remove the status text label.
  if (status_text.empty()) {
    // TODO(thakis): Once there is a status label, hide it here
    return;
  }

  // TODO(thakis): Set status_text as status label
}
