// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_modal_dialog.h"

#import <Cocoa/Cocoa.h>

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "base/sys_string_conversions.h"
#include "grit/generated_resources.h"

// Helper object that receives the notification that the dialog/sheet is
// going away. Is responsible for cleaning itself up.
@interface AppModalDialogHelper : NSObject
@end

@implementation AppModalDialogHelper
// |contextInfo| is the bridge back to the C++ AppModalDialog. When complete,
// autorelease to clean ourselves up.
- (void)sheetDidEnd:(NSWindow *)sheet
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  AppModalDialog* bridge = reinterpret_cast<AppModalDialog*>(contextInfo);
  switch (returnCode) {
    case NSAlertDefaultReturn:  // OK
      bridge->OnAccept(std::wstring(), false);
      break;
    case NSAlertOtherReturn:  // Cancel
      bridge->OnCancel();
      break;
    default:
      NOTREACHED();
  }
  [self autorelease];
  delete bridge;  // Done with the dialog, it needs be destroyed.
}
@end

AppModalDialog::~AppModalDialog() {
}

void AppModalDialog::CreateAndShowDialog() {
  // Determine the names of the dialog buttons based on the flags. "Default"
  // is the OK button. "Other" is the cancel button. We don't use the
  // "Alternate" button in NSRunAlertPanel.
  // TODO(pinkerton): Need to find the right localized strings for these.
  NSString* default_button = NSLocalizedString(@"OK", nil);
  NSString* other_button = NSLocalizedString(@"Cancel", nil);
  switch (dialog_flags_) {
    case MessageBoxFlags::kIsJavascriptAlert:
      // OK & Cancel are fine for these types of alerts.
      break;
    case MessageBoxFlags::kIsJavascriptConfirm:
      if (is_before_unload_dialog_) {
        std::string button_text = l10n_util::GetStringUTF8(
            IDS_BEFOREUNLOAD_MESSAGEBOX_OK_BUTTON_LABEL);
        default_button = base::SysUTF8ToNSString(button_text);
        button_text = l10n_util::GetStringUTF8(
            IDS_BEFOREUNLOAD_MESSAGEBOX_CANCEL_BUTTON_LABEL);
        other_button = base::SysUTF8ToNSString(button_text);
      }
      break;
    case MessageBoxFlags::kIsJavascriptPrompt:
      // We need to make a custom message box for javascript prompts. Not
      // sure how to handle this...
      NOTIMPLEMENTED();
      break;

    default:
      NOTREACHED();
  }

  // Create a helper which will receive the sheet ended selector. It will
  // delete itself when done. It doesn't need anything passed to its init
  // as it will get a contextInfo parameter.
  AppModalDialogHelper* helper = [[AppModalDialogHelper alloc] init];

  // Show the modal dialog.
  NSString* title_str = base::SysWideToNSString(title_);
  NSString* message_str = base::SysWideToNSString(message_text_);
  NSBeginAlertSheet(title_str, default_button, nil, other_button, nil,
                    helper, @selector(sheetDidEnd:returnCode:contextInfo:),
                    nil, this, message_str);
}

void AppModalDialog::ActivateModalDialog() {
  NOTIMPLEMENTED();
}

void AppModalDialog::CloseModalDialog() {
  NOTIMPLEMENTED();
}

int AppModalDialog::GetDialogButtons() {
  NOTIMPLEMENTED();
  return 0;
}

void AppModalDialog::AcceptWindow() {
  NOTIMPLEMENTED();
}

void AppModalDialog::CancelWindow() {
  NOTIMPLEMENTED();
}
