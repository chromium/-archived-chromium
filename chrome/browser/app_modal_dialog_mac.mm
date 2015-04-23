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
@interface AppModalDialogHelper : NSObject {
 @private
  NSAlert* alert_;
  NSTextField* textField_;  // WEAK; owned by alert_
}

- (NSAlert*)alert;
- (NSTextField*)textField;
- (void)alertDidEnd:(NSAlert *)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo;

@end

@implementation AppModalDialogHelper

- (NSAlert*)alert {
  alert_ = [[NSAlert alloc] init];
  return alert_;
}

- (NSTextField*)textField {
  textField_ = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 300, 22)];
  [alert_ setAccessoryView:textField_];
  [textField_ release];

  return textField_;
}

- (void)dealloc {
  [alert_ release];
  [super dealloc];
}

// |contextInfo| is the bridge back to the C++ AppModalDialog. When complete,
// autorelease to clean ourselves up.
- (void)alertDidEnd:(NSAlert *)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  AppModalDialog* bridge = reinterpret_cast<AppModalDialog*>(contextInfo);
  std::wstring input;
  if (textField_)
    input = base::SysNSStringToWide([textField_ stringValue]);
  switch (returnCode) {
    case NSAlertFirstButtonReturn:   // OK
      bridge->OnAccept(input, false);
      break;
    case NSAlertSecondButtonReturn:  // Cancel
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
  bool text_field = false;
  bool one_button = false;
  switch (dialog_flags_) {
    case MessageBoxFlags::kIsJavascriptAlert:
      one_button = true;
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
      text_field = true;
      break;

    default:
      NOTREACHED();
  }

  // Create a helper which will receive the sheet ended selector. It will
  // delete itself when done. It doesn't need anything passed to its init
  // as it will get a contextInfo parameter.
  AppModalDialogHelper* helper = [[AppModalDialogHelper alloc] init];

  // Show the modal dialog.
  NSAlert* alert = [helper alert];
  NSTextField* field = nil;
  if (text_field) {
    field = [helper textField];
    [field setStringValue:base::SysWideToNSString(default_prompt_text_)];
  }
  [alert setDelegate:helper];
  [alert setInformativeText:base::SysWideToNSString(message_text_)];
  [alert setMessageText:base::SysWideToNSString(title_)];
  [alert addButtonWithTitle:default_button];
  if (!one_button)
    [alert addButtonWithTitle:other_button];

  [alert beginSheetModalForWindow:nil  // nil here makes it app-modal
                    modalDelegate:helper
                   didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
                      contextInfo:this];

  if (field)
    [[alert window] makeFirstResponder:field];
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
