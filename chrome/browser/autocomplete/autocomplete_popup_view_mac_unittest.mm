// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autocomplete/autocomplete_popup_view_mac.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "testing/platform_test.h"

namespace {

class AutocompletePopupViewMacTest : public PlatformTest {
 public:
  AutocompletePopupViewMacTest() {}

  virtual void SetUp() {
    PlatformTest::SetUp();

    // These are here because there is no autorelease pool for the
    // constructor.
    color_ = [NSColor blackColor];
    font_ = [NSFont userFontOfSize:12];
  }

  // Returns the length of the run starting at |location| for which
  // |attributeName| remains the same.
  static NSUInteger RunLengthForAttribute(NSAttributedString* string,
                                          NSUInteger location,
                                          NSString* attributeName) {
    const NSRange fullRange = NSMakeRange(0, [string length]);
    NSRange range;
    [string attribute:attributeName
              atIndex:location longestEffectiveRange:&range inRange:fullRange];

    // In order to signal when the run doesn't start exactly at
    // location, return a weirdo length.  This causes the incorrect
    // expectation to manifest at the calling location, which is more
    // useful than an EXPECT_EQ() would be here.
    if (range.location != location) {
      return -1;
    }

    return range.length;
  }

  // Return true if the run starting at |location| has |color| for
  // attribute NSForegroundColorAttributeName.
  static bool RunHasColor(NSAttributedString* string,
                          NSUInteger location, NSColor* color) {
    const NSRange fullRange = NSMakeRange(0, [string length]);
    NSRange range;
    NSColor* runColor = [string attribute:NSForegroundColorAttributeName
                                  atIndex:location
                    longestEffectiveRange:&range inRange:fullRange];

    // According to one "Ali Ozer", you can compare objects within the
    // same color space using -isEqual:.  Converting color spaces
    // seems too heavyweight for these tests.
    // http://lists.apple.com/archives/cocoa-dev/2005/May/msg00186.html
    return [runColor isEqual:color] ? true : false;
  }

  // Return true if the run starting at |location| has the font
  // trait(s) in |mask| font in NSFontAttributeName.
  static bool RunHasFontTrait(NSAttributedString* string, NSUInteger location,
                              NSFontTraitMask mask) {
    const NSRange fullRange = NSMakeRange(0, [string length]);
    NSRange range;
    NSFont* runFont = [string attribute:NSFontAttributeName
                                atIndex:location
                  longestEffectiveRange:&range inRange:fullRange];
    NSFontManager* fontManager = [NSFontManager sharedFontManager];
    if (runFont && (mask == ([fontManager traitsOfFont:runFont]&mask))) {
      return true;
    }
    return false;
  }

  // AutocompleteMatch doesn't really have the right constructor for our
  // needs.  Fake one for us to use.
  static AutocompleteMatch MakeMatch(const std::wstring &contents,
                                     const std::wstring &description) {
    AutocompleteMatch m(NULL, 1, true, AutocompleteMatch::URL_WHAT_YOU_TYPED);
    m.contents = contents;
    m.description = description;
    return m;
  }

  NSColor* color_;  // weak
  NSFont* font_;  // weak
};

// Simple inputs with no matches should result in styled output who's
// text matches the input string, with the passed-in color, and
// nothing bolded.
TEST_F(AutocompletePopupViewMacTest, DecorateMatchedStringNoMatch) {
  const NSString* string = @"This is a test";
  AutocompleteMatch::ACMatchClassifications classifications;

  NSAttributedString* decorated =
      AutocompletePopupViewMac::DecorateMatchedString(
          base::SysNSStringToWide(string), classifications,
          color_, font_);

  // Result has same characters as the input.
  EXPECT_EQ([decorated length], [string length]);
  EXPECT_TRUE([[decorated string] isEqualToString:string]);

  // Our passed-in color for the entire string.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSForegroundColorAttributeName),
            [string length]);
  EXPECT_TRUE(RunHasColor(decorated, 0U, color_));

  // An unbolded font for the entire string.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSFontAttributeName), [string length]);
  EXPECT_FALSE(RunHasFontTrait(decorated, 0U, NSBoldFontMask));
}

// Identical to DecorateMatchedStringNoMatch, except test that URL
// style gets a different color than we passed in.
TEST_F(AutocompletePopupViewMacTest, DecorateMatchedStringURLNoMatch) {
  const NSString* string = @"This is a test";
  AutocompleteMatch::ACMatchClassifications classifications;

  classifications.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));

  NSAttributedString* decorated =
      AutocompletePopupViewMac::DecorateMatchedString(
          base::SysNSStringToWide(string), classifications,
          color_, font_);

  // Result has same characters as the input.
  EXPECT_EQ([decorated length], [string length]);
  EXPECT_TRUE([[decorated string] isEqualToString:string]);

  // One color for the entire string, and it's not the one we passed in.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSForegroundColorAttributeName),
            [string length]);
  EXPECT_FALSE(RunHasColor(decorated, 0U, color_));

  // An unbolded font for the entire string.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSFontAttributeName), [string length]);
  EXPECT_FALSE(RunHasFontTrait(decorated, 0U, NSBoldFontMask));
}

// Test that DIM doesn't have any impact - true at this time.
TEST_F(AutocompletePopupViewMacTest, DecorateMatchedStringDimNoMatch) {
  const NSString* string = @"This is a test";

  // Switch to DIM halfway through.
  AutocompleteMatch::ACMatchClassifications classifications;
  classifications.push_back(
      ACMatchClassification(0, ACMatchClassification::NONE));
  classifications.push_back(
      ACMatchClassification([string length] / 2, ACMatchClassification::DIM));

  NSAttributedString* decorated =
      AutocompletePopupViewMac::DecorateMatchedString(
          base::SysNSStringToWide(string), classifications,
          color_, font_);

  // Result has same characters as the input.
  EXPECT_EQ([decorated length], [string length]);
  EXPECT_TRUE([[decorated string] isEqualToString:string]);

  // Our passed-in color for the entire string.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSForegroundColorAttributeName),
            [string length]);
  EXPECT_TRUE(RunHasColor(decorated, 0U, color_));

  // An unbolded font for the entire string.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSFontAttributeName), [string length]);
  EXPECT_FALSE(RunHasFontTrait(decorated, 0U, NSBoldFontMask));
}

// Test that the matched run gets bold-faced, but keeps the same
// color.
TEST_F(AutocompletePopupViewMacTest, DecorateMatchedStringMatch) {
  const NSString* string = @"This is a test";
  // Match "is".
  const NSUInteger runLength1 = 5, runLength2 = 2, runLength3 = 7;
  // Make sure nobody messed up the inputs.
  EXPECT_EQ(runLength1 + runLength2 + runLength3, [string length]);

  // Push each run onto classifications.
  AutocompleteMatch::ACMatchClassifications classifications;
  classifications.push_back(
      ACMatchClassification(0, ACMatchClassification::NONE));
  classifications.push_back(
      ACMatchClassification(runLength1, ACMatchClassification::MATCH));
  classifications.push_back(
      ACMatchClassification(runLength1 + runLength2,
                            ACMatchClassification::NONE));

  NSAttributedString* decorated =
      AutocompletePopupViewMac::DecorateMatchedString(
          base::SysNSStringToWide(string), classifications,
          color_, font_);

  // Result has same characters as the input.
  EXPECT_EQ([decorated length], [string length]);
  EXPECT_TRUE([[decorated string] isEqualToString:string]);

  // Our passed-in color for the entire string.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSForegroundColorAttributeName),
            [string length]);
  EXPECT_TRUE(RunHasColor(decorated, 0U, color_));

  // Should have three font runs, not bold, bold, then not bold again.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSFontAttributeName), runLength1);
  EXPECT_FALSE(RunHasFontTrait(decorated, 0U, NSBoldFontMask));

  EXPECT_EQ(RunLengthForAttribute(decorated, runLength1,
                                  NSFontAttributeName), runLength2);
  EXPECT_TRUE(RunHasFontTrait(decorated, runLength1, NSBoldFontMask));

  EXPECT_EQ(RunLengthForAttribute(decorated, runLength1 + runLength2,
                                  NSFontAttributeName), runLength3);
  EXPECT_FALSE(RunHasFontTrait(decorated, runLength1 + runLength2,
                               NSBoldFontMask));
}

// Just like DecorateMatchedStringURLMatch, this time with URL style.
TEST_F(AutocompletePopupViewMacTest, DecorateMatchedStringURLMatch) {
  const NSString* string = @"http://hello.world/";
  // Match "hello".
  const NSUInteger runLength1 = 7, runLength2 = 5, runLength3 = 7;
  // Make sure nobody messed up the inputs.
  EXPECT_EQ(runLength1 + runLength2 + runLength3, [string length]);

  // Push each run onto classifications.
  AutocompleteMatch::ACMatchClassifications classifications;
  classifications.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  const int kURLMatch = ACMatchClassification::URL|ACMatchClassification::MATCH;
  classifications.push_back(ACMatchClassification(runLength1, kURLMatch));
  classifications.push_back(
      ACMatchClassification(runLength1 + runLength2,
                            ACMatchClassification::URL));

  NSAttributedString* decorated =
      AutocompletePopupViewMac::DecorateMatchedString(
          base::SysNSStringToWide(string), classifications,
          color_, font_);

  // Result has same characters as the input.
  EXPECT_EQ([decorated length], [string length]);
  EXPECT_TRUE([[decorated string] isEqualToString:string]);

  // One color for the entire string, and it's not the one we passed in.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSForegroundColorAttributeName),
            [string length]);
  EXPECT_FALSE(RunHasColor(decorated, 0U, color_));

  // Should have three font runs, not bold, bold, then not bold again.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSFontAttributeName), runLength1);
  EXPECT_FALSE(RunHasFontTrait(decorated, 0U, NSBoldFontMask));

  EXPECT_EQ(RunLengthForAttribute(decorated, runLength1,
                                  NSFontAttributeName), runLength2);
  EXPECT_TRUE(RunHasFontTrait(decorated, runLength1, NSBoldFontMask));

  EXPECT_EQ(RunLengthForAttribute(decorated, runLength1 + runLength2,
                                  NSFontAttributeName), runLength3);
  EXPECT_FALSE(RunHasFontTrait(decorated, runLength1 + runLength2,
                               NSBoldFontMask));
}

// Check that matches with both contents and description come back
// with contents at the beginning, description at the end, and
// something separating them.  Not being specific about the separator
// on purpose, in case it changes.
TEST_F(AutocompletePopupViewMacTest, MatchText) {
  const NSString* contents = @"contents";
  const NSString* description = @"description";
  AutocompleteMatch m = MakeMatch(base::SysNSStringToWide(contents),
                                  base::SysNSStringToWide(description));

  NSAttributedString* decorated = AutocompletePopupViewMac::MatchText(m, font_);

  // Result contains the characters of the input in the right places.
  EXPECT_GT([decorated length], [contents length] + [description length]);
  EXPECT_TRUE([[decorated string] hasPrefix:contents]);
  EXPECT_TRUE([[decorated string] hasSuffix:description]);

  // Check that the description is a different color from the
  // contents.
  const NSUInteger descriptionLocation =
      [decorated length] - [description length];
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSForegroundColorAttributeName),
            descriptionLocation);
  EXPECT_EQ(RunLengthForAttribute(decorated, descriptionLocation,
                                  NSForegroundColorAttributeName),
            [description length]);

  // Same font all the way through, nothing bold.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSFontAttributeName), [decorated length]);
  EXPECT_FALSE(RunHasFontTrait(decorated, 0, NSBoldFontMask));
}

// Check that MatchText() styles content matches as expected.
TEST_F(AutocompletePopupViewMacTest, MatchTextContentsMatch) {
  const NSString* contents = @"This is a test";
  // Match "is".
  const NSUInteger runLength1 = 5, runLength2 = 2, runLength3 = 7;
  // Make sure nobody messed up the inputs.
  EXPECT_EQ(runLength1 + runLength2 + runLength3, [contents length]);

  AutocompleteMatch m = MakeMatch(base::SysNSStringToWide(contents),
                                  std::wstring());

  // Push each run onto contents classifications.
  m.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::NONE));
  m.contents_class.push_back(
      ACMatchClassification(runLength1, ACMatchClassification::MATCH));
  m.contents_class.push_back(
      ACMatchClassification(runLength1 + runLength2,
                            ACMatchClassification::NONE));

  NSAttributedString* decorated = AutocompletePopupViewMac::MatchText(m, font_);

  // Result has same characters as the input.
  EXPECT_EQ([decorated length], [contents length]);
  EXPECT_TRUE([[decorated string] isEqualToString:contents]);

  // Result is all one color.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSForegroundColorAttributeName),
            [contents length]);

  // Should have three font runs, not bold, bold, then not bold again.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSFontAttributeName), runLength1);
  EXPECT_FALSE(RunHasFontTrait(decorated, 0U, NSBoldFontMask));

  EXPECT_EQ(RunLengthForAttribute(decorated, runLength1,
                                  NSFontAttributeName), runLength2);
  EXPECT_TRUE(RunHasFontTrait(decorated, runLength1, NSBoldFontMask));

  EXPECT_EQ(RunLengthForAttribute(decorated, runLength1 + runLength2,
                                  NSFontAttributeName), runLength3);
  EXPECT_FALSE(RunHasFontTrait(decorated, runLength1 + runLength2,
                               NSBoldFontMask));
}

// Check that MatchText() styles description matches as expected.
TEST_F(AutocompletePopupViewMacTest, MatchTextDescriptionMatch) {
  const NSString* contents = @"This is a test";
  const NSString* description = @"That was a test";
  // Match "That was".
  const NSUInteger runLength1 = 8, runLength2 = 7;
  // Make sure nobody messed up the inputs.
  EXPECT_EQ(runLength1 + runLength2, [description length]);

  AutocompleteMatch m = MakeMatch(base::SysNSStringToWide(contents),
                                  base::SysNSStringToWide(description));

  // Push each run onto contents classifications.
  m.description_class.push_back(
      ACMatchClassification(0, ACMatchClassification::MATCH));
  m.description_class.push_back(
      ACMatchClassification(runLength1, ACMatchClassification::NONE));

  NSAttributedString* decorated = AutocompletePopupViewMac::MatchText(m, font_);

  // Result contains the characters of the input.
  EXPECT_GT([decorated length], [contents length] + [description length]);
  EXPECT_TRUE([[decorated string] hasPrefix:contents]);
  EXPECT_TRUE([[decorated string] hasSuffix:description]);

  // Check that the description is a different color from the
  // contents.
  const NSUInteger descriptionLocation =
      [decorated length] - [description length];
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSForegroundColorAttributeName),
            descriptionLocation);
  EXPECT_EQ(RunLengthForAttribute(decorated, descriptionLocation,
                                  NSForegroundColorAttributeName),
            [description length]);

  // Should have three font runs, not bold, bold, then not bold again.
  // The first run is the contents and the separator, the second run
  // is the first run of the description.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSFontAttributeName), descriptionLocation);
  EXPECT_FALSE(RunHasFontTrait(decorated, 0U, NSBoldFontMask));

  EXPECT_EQ(RunLengthForAttribute(decorated, descriptionLocation,
                                  NSFontAttributeName), runLength1);
  EXPECT_TRUE(RunHasFontTrait(decorated, descriptionLocation, NSBoldFontMask));

  EXPECT_EQ(RunLengthForAttribute(decorated, descriptionLocation + runLength1,
                                  NSFontAttributeName), runLength2);
  EXPECT_FALSE(RunHasFontTrait(decorated, descriptionLocation + runLength1,
                               NSBoldFontMask));
}

// TODO(shess): Test that
// AutocompletePopupViewMac::UpdatePopupAppearance() creates/destroys
// the popup according to result contents.  Test that the matrix gets
// the right number of results.  Test the contents of the cells for
// the right strings.  Icons?  Maybe, that seems harder to test.
// Styling seems almost impossible.

// TODO(shess): Test that AutocompletePopupViewMac::PaintUpdatesNow()
// updates the matrix selection.

// TODO(shess): Test that AutocompletePopupViewMac::AcceptInput()
// updates the model's selection from the matrix before returning.
// Could possibly test that via -select:.

// TODO(shess): Test that AutocompleteButtonCell returns the right
// background colors for on, highlighted, and neither.

// TODO(shess): Test that AutocompleteMatrixTarget can be initialized
// and then sends -select: to the view.

}  // namespace
