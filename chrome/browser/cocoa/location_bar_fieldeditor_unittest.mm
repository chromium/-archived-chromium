// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/toolbar_controller.h"

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {
int NumTypesOnPasteboard(NSPasteboard* pb) {
  return [[pb types] count];
}

void ClearPasteboard(NSPasteboard* pb) {
  [pb declareTypes:[NSArray array] owner:nil];
}

bool NoRichTextOnClipboard(NSPasteboard* pb) {
  return ([pb dataForType:NSRTFPboardType] == nil) &&
      ([pb dataForType:NSRTFDPboardType] == nil) &&
      ([pb dataForType:NSHTMLPboardType] == nil);
}

bool ClipboardContainsText(NSPasteboard* pb, NSString* cmp) {
  NSString* clipboard_text = [pb stringForType:NSStringPboardType];
  return [clipboard_text isEqualToString:cmp];
}

} // namespace

class LocationBarFieldEditorTest : public PlatformTest {
 public:
  virtual void SetUp() {
    PlatformTest::SetUp();
    pb_ = [NSPasteboard pasteboardWithUniqueName];
  }

  virtual void TearDown() {
    [pb_ releaseGlobally];
    pb_ = nil;
    PlatformTest::TearDown();
  }

  NSPasteboard *clipboard() {
    DCHECK(pb_);
    return pb_;
  }

  private:
    NSPasteboard *pb_;
};

TEST_F(LocationBarFieldEditorTest, CutCopyTest) {
  // Make sure pasteboard is empty before we start.
  ASSERT_EQ(NumTypesOnPasteboard(clipboard()), 0);

  NSString* test_string_1 = @"astring";
  NSString* test_string_2 = @"another string";

  scoped_nsobject<LocationBarFieldEditor> field_editor(
      [[LocationBarFieldEditor alloc] init]);
  [field_editor.get() setRichText:YES];

  // Put some text on the clipboard.
  [field_editor.get() setString:test_string_1];
  [field_editor.get() selectAll:nil];
  [field_editor.get() alignRight:nil];  // Add a rich text attribute.
  ASSERT_TRUE(NoRichTextOnClipboard(clipboard()));

  // Check that copying it works and we only get plain text.
  NSPasteboard* pb = clipboard();
  [field_editor.get() performCopy:pb];
  ASSERT_TRUE(NoRichTextOnClipboard(clipboard()));
  ASSERT_TRUE(ClipboardContainsText(clipboard(), test_string_1));

  // Check that cutting it works and we only get plain text.
  [field_editor.get() setString:test_string_2];
  [field_editor.get() selectAll:nil];
  [field_editor.get() alignLeft:nil];  // Add a rich text attribute.
  [field_editor.get() performCut:pb];
  ASSERT_TRUE(NoRichTextOnClipboard(clipboard()));
  ASSERT_TRUE(ClipboardContainsText(clipboard(), test_string_2));
  ASSERT_EQ([[field_editor.get() textStorage] length], 0U);
}
