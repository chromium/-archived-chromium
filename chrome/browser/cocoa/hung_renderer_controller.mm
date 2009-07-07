// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "app/l10n_util.h"
#include "base/mac_util.h"
#include "base/process_util.h"
#include "base/sys_string_conversions.h"
#include "grit/generated_resources.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/hung_renderer_dialog.h"
#import "chrome/browser/cocoa/hung_renderer_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/result_codes.h"
#include "grit/chromium_strings.h"

namespace {
// We only support showing one of these at a time per app.  The
// controller owns itself and is released when its window is closed.
HungRendererController* g_instance = NULL;
}  // end namespace

@implementation HungRendererController

- (id)initWithWindowNibName:(NSString*)nibName {
  NSString* nibpath = [mac_util::MainAppBundle() pathForResource:nibName
                                                          ofType:@"nib"];
  self = [super initWithWindowNibPath:nibpath owner:self];
  if (self) {
    [tableView_ setDataSource:self];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!g_instance);
  [super dealloc];
}

- (void)awakeFromNib {
  // This is easier than creating a localizer, since we only have one
  // string to modify.
  std::wstring productString = l10n_util::GetString(IDS_PRODUCT_NAME);
  [[self window] setTitle:base::SysWideToNSString(productString)];
}

- (IBAction)kill:(id)sender {
  if (hungContents_)
    base::KillProcess(hungContents_->process()->process().handle(),
                      ResultCodes::HUNG, false);
  // Cannot call performClose:, because the close button is disabled.
  [self close];
}

- (IBAction)wait:(id)sender {
  if (hungContents_ && hungContents_->render_view_host())
    hungContents_->render_view_host()->RestartHangMonitorTimeout();
  // Cannot call performClose:, because the close button is disabled.
  [self close];
}

- (int)numberOfRowsInTableView:(NSTableView *)aTableView {
  return hungRenderers_.size();
}

- (id)tableView:(NSTableView*)aTableView
      objectValueForTableColumn:(NSTableColumn*)column
            row:(int)rowIndex {
  // TODO(rohitrao): Add favicons.
  TabContents* contents = hungRenderers_[rowIndex];
  string16 title = contents->GetTitle();
  if (title.empty())
    title = l10n_util::GetStringUTF16(IDS_TAB_UNTITLED_TITLE);
  return base::SysUTF16ToNSString(title);
}

- (void)windowWillClose:(NSNotification*)notification {
  // We have to reset g_instance before autoreleasing the window,
  // because we want to avoid reusing the same dialog if someone calls
  // HungRendererDialog::ShowForTabContents() between the autorelease
  // call and the actual dealloc.
  g_instance = nil;

  [self autorelease];
}

- (void)showForTabContents:(TabContents*)contents {
  DCHECK(contents);
  hungContents_ = contents;
  hungRenderers_.clear();
  for (TabContentsIterator it; !it.done(); ++it) {
    if (it->process() == hungContents_->process())
      hungRenderers_.push_back(*it);
  }
  [tableView_ reloadData];

  [[self window] center];
  [self showWindow:self];
}

- (void)endForTabContents:(TabContents*)contents {
  DCHECK(contents);
  DCHECK(hungContents_);
  if (hungContents_ && hungContents_->process() == contents->process()) {
    // Cannot call performClose:, because the close button is disabled.
    [self close];
  }
}

@end

@implementation HungRendererController (JustForTesting)
- (NSButton*)killButton {
  return killButton_;
}

- (NSButton*)waitButton {
  return waitButton_;
}
@end

// static
void HungRendererDialog::ShowForTabContents(TabContents* contents) {
  if (!logging::DialogsAreSuppressed()) {
    if (!g_instance)
      g_instance = [[HungRendererController alloc]
                     initWithWindowNibName:@"HungRendererDialog"];
    [g_instance showForTabContents:contents];
  }
}

// static
void HungRendererDialog::HideForTabContents(TabContents* contents) {
  if (!logging::DialogsAreSuppressed() && g_instance)
    [g_instance endForTabContents:contents];
}
