// Copyright (c), 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/rwhvm_editcommand_helper.h"

#import <objc/runtime.h>

#include "chrome/browser/renderer_host/render_widget_host.h"
#import "chrome/browser/renderer_host/render_widget_host_view_mac.h"

namespace {
// The names of all the objc selectors w/o ':'s added to an object by
// AddEditingSelectorsToClass().
//
// This needs to be kept in Sync with WEB_COMMAND list in the WebKit tree at:
// WebKit/mac/WebView/WebHTMLView.mm .
const char* kEditCommands[] = {
  "alignCenter",
  "alignJustified",
  "alignLeft",
  "alignRight",
  "copy",
  "cut",
  "delete",
  "deleteBackward",
  "deleteBackwardByDecomposingPreviousCharacter",
  "deleteForward",
  "deleteToBeginningOfLine",
  "deleteToBeginningOfParagraph",
  "deleteToEndOfLine",
  "deleteToEndOfParagraph",
  "deleteToMark",
  "deleteWordBackward",
  "deleteWordForward",
  "ignoreSpelling",
  "indent",
  "insertBacktab",
  "insertLineBreak",
  "insertNewline",
  "insertNewlineIgnoringFieldEditor",
  "insertParagraphSeparator",
  "insertTab",
  "insertTabIgnoringFieldEditor",
  "makeTextWritingDirectionLeftToRight",
  "makeTextWritingDirectionNatural",
  "makeTextWritingDirectionRightToLeft",
  "moveBackward",
  "moveBackwardAndModifySelection",
  "moveDown",
  "moveDownAndModifySelection",
  "moveForward",
  "moveForwardAndModifySelection",
  "moveLeft",
  "moveLeftAndModifySelection",
  "moveParagraphBackwardAndModifySelection",
  "moveParagraphForwardAndModifySelection",
  "moveRight",
  "moveRightAndModifySelection",
  "moveToBeginningOfDocument",
  "moveToBeginningOfDocumentAndModifySelection",
  "moveToBeginningOfLine",
  "moveToBeginningOfLineAndModifySelection",
  "moveToBeginningOfParagraph",
  "moveToBeginningOfParagraphAndModifySelection",
  "moveToBeginningOfSentence",
  "moveToBeginningOfSentenceAndModifySelection",
  "moveToEndOfDocument",
  "moveToEndOfDocumentAndModifySelection",
  "moveToEndOfLine",
  "moveToEndOfLineAndModifySelection",
  "moveToEndOfParagraph",
  "moveToEndOfParagraphAndModifySelection",
  "moveToEndOfSentence",
  "moveToEndOfSentenceAndModifySelection",
  "moveUp",
  "moveUpAndModifySelection",
  "moveWordBackward",
  "moveWordBackwardAndModifySelection",
  "moveWordForward",
  "moveWordForwardAndModifySelection",
  "moveWordLeft",
  "moveWordLeftAndModifySelection",
  "moveWordRight",
  "moveWordRightAndModifySelection",
  "outdent",
  "pageDown",
  "pageDownAndModifySelection",
  "pageUp",
  "pageUpAndModifySelection",
  "selectAll",
  "selectLine",
  "selectParagraph",
  "selectSentence",
  "selectToMark",
  "selectWord",
  "setMark",
  "subscript",
  "superscript",
  "swapWithMark",
  "transpose",
  "underline",
  "unscript",
  "yank",
  "yankAndSelect"};

// Maps an objc-selector to a core command name.
//
// Returns the core command name (which is the selector name with the trailing
// ':' stripped in most cases).
//
// Adapted from a function by the same name in
// WebKit/mac/WebView/WebHTMLView.mm .
// Capitalized names are returned from this function, but that's simply
// matching WebHTMLView.mm.
NSString* CommandNameForSelector(SEL selector) {
  if (selector == @selector(insertParagraphSeparator:) ||
      selector == @selector(insertNewlineIgnoringFieldEditor:))
    return @"InsertNewline";
  if (selector == @selector(insertTabIgnoringFieldEditor:))
      return @"InsertTab";
  if (selector == @selector(pageDown:))
      return @"MovePageDown";
  if (selector == @selector(pageDownAndModifySelection:))
      return @"MovePageDownAndModifySelection";
  if (selector == @selector(pageUp:))
      return @"MovePageUp";
  if (selector == @selector(pageUpAndModifySelection:))
      return @"MovePageUpAndModifySelection";

  // Remove the trailing colon.
  NSString* selector_str = NSStringFromSelector(selector);
  int selector_len = [selector_str length];
  return [selector_str substringToIndex:selector_len - 1];
}

// This function is installed via the objc runtime as the implementation of all
// the various editing selectors.
// The objc runtime hookup occurs in
// RWHVMEditCommandHelper::AddEditingSelectorsToClass().
//
// self - the object we're attached to; it must implement the
// RenderWidgetHostViewMacOwner protocol.
// _cmd - the selector that fired.
// sender - the id of the object that sent the message.
//
// The selector is translated into an edit comand and then forwarded down the
// pipeline to WebCore.
// The route the message takes is:
// RenderWidgetHostViewMac -> RenderViewHost ->
// | IPC | ->
// RenderView -> currently focused WebFrame.
// The WebFrame is in the Chrome glue layer and forwards the message to WebCore.
void EditCommandImp(id self, SEL _cmd, id sender) {
  // Make sure |self| is the right type.
  DCHECK([self conformsToProtocol:@protocol(RenderWidgetHostViewMacOwner)]);

  // SEL -> command name string.
  NSString* command_name_ns = CommandNameForSelector(_cmd);
  std::string edit_command([command_name_ns UTF8String]);

  // Forward the edit command string down the pipeline.
  RenderWidgetHostViewMac* rwhv = [(id<RenderWidgetHostViewMacOwner>)self
      renderWidgetHostViewMac];
  DCHECK(rwhv);

  // The second parameter is the core command value which isn't used here.
  rwhv->GetRenderWidgetHost()->ForwardEditCommand(edit_command, "");
}

}  // namespace

RWHVMEditCommandHelper::RWHVMEditCommandHelper() {
  for (size_t i = 0; i < arraysize(kEditCommands); ++i) {
    edit_command_set_.insert(kEditCommands[i]);
  }
}

// Dynamically adds Selectors to the aformentioned class.
void RWHVMEditCommandHelper::AddEditingSelectorsToClass(Class klass) {
  for (size_t i = 0; i < arraysize(kEditCommands); ++i) {
    // Append trailing ':' to command name to get selector name.
    NSString* sel_str = [NSString stringWithFormat: @"%s:", kEditCommands[i]];

    SEL edit_selector = NSSelectorFromString(sel_str);
    // May want to use @encode() for the last parameter to this method.
    // If class_addMethod fails we assume that all the editing selectors where
    // added to the class.
    // If a certain class already implements a method then class_addMethod
    // returns NO, which we can safely ignore.
    class_addMethod(klass, edit_selector, (IMP)EditCommandImp, "v@:@");
  }
}

bool RWHVMEditCommandHelper::IsMenuItemEnabled(SEL item_action,
    id<RenderWidgetHostViewMacOwner> owner) {
  const char* selector_name = sel_getName(item_action);
  // TODO(jeremy): The final form of this function will check state
  // associated with the Browser.

  // For now just mark all edit commands as enabled.
  NSString* selector_name_ns = [NSString stringWithUTF8String:selector_name];

  // Remove trailing ':'
  size_t str_len = [selector_name_ns length];
  selector_name_ns = [selector_name_ns substringToIndex:str_len - 1];
  std::string edit_command_name([selector_name_ns UTF8String]);

  // search for presence in set and return.
  bool ret = edit_command_set_.find(edit_command_name) !=
      edit_command_set_.end();
  return ret;
}

NSArray* RWHVMEditCommandHelper::GetEditSelectorNames() {
  size_t num_edit_commands = arraysize(kEditCommands);
  NSMutableArray* ret = [NSMutableArray arrayWithCapacity:num_edit_commands];

  for (size_t i = 0; i < num_edit_commands; ++i) {
      [ret addObject:[NSString stringWithUTF8String:kEditCommands[i]]];
  }

  return ret;
}
