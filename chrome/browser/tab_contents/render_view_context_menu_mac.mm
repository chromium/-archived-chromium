// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_context_menu_mac.h"

#include "base/compiler_specific.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/profile.h"
#include "chrome/common/l10n_util.h"
#include "grit/generated_resources.h"

// Obj-C bridge class that is the target of all items in the context menu.
// Relies on the tag being set to the command id. Uses |context_| to
// execute the command once it is selected and determine if commands should
// be enabled or disabled.

@interface ContextMenuTarget : NSObject {
 @private
  RenderViewContextMenuMac* context_;  // weak, owns us.
}
- (id)initWithContext:(RenderViewContextMenuMac*)context;
- (void)itemSelected:(id)sender;
@end

@implementation ContextMenuTarget

- (id)initWithContext:(RenderViewContextMenuMac*)context {
  if ((self = [super init])) {
    DCHECK(context);
    context_ = context;
  }
  return self;
}

- (void)itemSelected:(id)sender {
  context_->ExecuteCommand([sender tag]);
}

@end

RenderViewContextMenuMac::RenderViewContextMenuMac(
    WebContents* web_contents,
    const ContextMenuParams& params,
    NSView* parent_view)
    : RenderViewContextMenu(web_contents, params),
      menu_([[NSMenu alloc] init]),
      insert_menu_(menu_),
      target_(nil) {
  [menu_ setAutoenablesItems:NO];
  target_ = [[ContextMenuTarget alloc] initWithContext:this];
  InitMenu(params.node);

  // show the menu
  [NSMenu popUpContextMenu:menu_
                 withEvent:[NSApp currentEvent]
                   forView:parent_view];
}

RenderViewContextMenuMac::~RenderViewContextMenuMac() {
  [target_ release];
  [menu_ release];
}

// Do things like remove the windows accelerators.
// TODO(pinkerton): Do we want to do anything like make a maximum string width
// and middle-truncate?
NSString* RenderViewContextMenuMac::PrepareLabelForDisplay(
    const std::wstring& label) {
  // Strip out any "&"'s that are windows accelerators and we don't use.
  NSMutableString* title =
    [NSMutableString stringWithString:base::SysWideToNSString(label)];
  NSRange range = NSMakeRange(0, [title length]);
  [title replaceOccurrencesOfString:@"&" withString:@"" options:0 range:range];
  return title;
}

void RenderViewContextMenuMac::AppendMenuItem(int command_id) {
  AppendMenuItem(command_id, l10n_util::GetString(command_id));
}

void RenderViewContextMenuMac::AppendMenuItem(int command_id,
                                              const std::wstring& label) {
  // Create the item and set its target/action to |target_| with the command
  // as |command_id|. Then add it to the menu at the end.
  NSMenuItem* item =
      [[[NSMenuItem alloc] initWithTitle:PrepareLabelForDisplay(label)
                                  action:@selector(itemSelected:)
                           keyEquivalent:@""] autorelease];
  [item setTag:command_id];
  [item setTarget:target_];
  [item setEnabled:IsItemCommandEnabled(command_id) ? YES : NO];
  [insert_menu_ addItem:item];
}

void RenderViewContextMenuMac::AppendRadioMenuItem(int id,
                                                   const std::wstring& label) {
  NOTIMPLEMENTED();
}

void RenderViewContextMenuMac::AppendCheckboxMenuItem(int id,
    const std::wstring& label) {
  NOTIMPLEMENTED();
}

void RenderViewContextMenuMac::AppendSeparator() {
  NSMenuItem* separator = [NSMenuItem separatorItem];
  [insert_menu_ addItem:separator];
}

void RenderViewContextMenuMac::StartSubMenu(int command_id,
                                            const std::wstring& label) {
  // I'm not a fan of this kind of API, but the other platforms have similar
  // guards so at least we know everyone will break together if someone
  // tries to mis-use the API.
  if (insert_menu_ != menu_) {
    NOTREACHED();
    return;
  }

  // We don't need to retain the submenu as the context menu already does, but
  // we switch the "insert menu" so subsequent items are added to the submenu
  // and not the main menu. This happens until someone calls FinishSubMenu().
  NSMenuItem* submenu_item =
      [[[NSMenuItem alloc] initWithTitle:PrepareLabelForDisplay(label)
                                  action:nil
                           keyEquivalent:nil] autorelease];
  insert_menu_ = [[[NSMenu alloc] init] autorelease];
  [submenu_item setSubmenu:insert_menu_];
  [menu_ addItem:submenu_item];
}

void RenderViewContextMenuMac::FinishSubMenu() {
  // Set the "insert menu" back to the main menu so that subsequently added
  // items get added to the main context menu.
  DCHECK(insert_menu_ != menu_);
  insert_menu_ = menu_;
}
