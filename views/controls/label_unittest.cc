// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/border.h"
#include "views/controls/label.h"

namespace views {

// All text sizing measurements (width and height) should be greater than this.
const int kMinTextDimension = 4;

TEST(LabelTest, FontProperty) {
  Label label;
  std::wstring font_name(L"courier");
  gfx::Font font = gfx::Font::CreateFont(font_name, 30);
  label.SetFont(font);
  gfx::Font font_used = label.GetFont();
  EXPECT_STREQ(font_name.c_str(), font_used.FontName().c_str());
  EXPECT_EQ(30, font_used.FontSize());
}

TEST(LabelTest, TextProperty) {
  Label label;
  std::wstring test_text(L"A random string.");
  label.SetText(test_text);
  EXPECT_STREQ(test_text.c_str(), label.GetText().c_str());
}

TEST(LabelTest, UrlProperty) {
  Label label;
  std::string my_url("http://www.orkut.com/some/Random/path");
  GURL url(my_url);
  label.SetURL(url);
  EXPECT_STREQ(my_url.c_str(), label.GetURL().spec().c_str());
  EXPECT_STREQ(UTF8ToWide(my_url).c_str(), label.GetText().c_str());
}

TEST(LabelTest, ColorProperty) {
  Label label;
  SkColor color = SkColorSetARGB(20, 40, 10, 5);
  label.SetColor(color);
  EXPECT_EQ(color, label.GetColor());
  SkColor h_color = SkColorSetARGB(40, 80, 20, 10);
  label.SetHighlightColor(h_color);
  EXPECT_EQ(h_color, label.GetHighlightColor());
}

TEST(LabelTest, AlignmentProperty) {
  Label label;
  bool reverse_alignment =
      l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT;

  label.SetHorizontalAlignment(Label::ALIGN_RIGHT);
  EXPECT_EQ(
      reverse_alignment ? Label::ALIGN_LEFT : Label::ALIGN_RIGHT,
      label.GetHorizontalAlignment());
  label.SetHorizontalAlignment(Label::ALIGN_LEFT);
  EXPECT_EQ(
      reverse_alignment ? Label::ALIGN_RIGHT : Label::ALIGN_LEFT,
      label.GetHorizontalAlignment());
  label.SetHorizontalAlignment(Label::ALIGN_CENTER);
  EXPECT_EQ(Label::ALIGN_CENTER, label.GetHorizontalAlignment());

  // The label's alignment should not be flipped if the RTL alignment mode
  // is AUTO_DETECT_ALIGNMENT.
  label.SetRTLAlignmentMode(Label::AUTO_DETECT_ALIGNMENT);
  label.SetHorizontalAlignment(Label::ALIGN_RIGHT);
  EXPECT_EQ(Label::ALIGN_RIGHT, label.GetHorizontalAlignment());
  label.SetHorizontalAlignment(Label::ALIGN_LEFT);
  EXPECT_EQ(Label::ALIGN_LEFT, label.GetHorizontalAlignment());
  label.SetHorizontalAlignment(Label::ALIGN_CENTER);
  EXPECT_EQ(Label::ALIGN_CENTER, label.GetHorizontalAlignment());
}

TEST(LabelTest, RTLAlignmentModeProperty) {
  Label label;
  EXPECT_EQ(Label::USE_UI_ALIGNMENT, label.GetRTLAlignmentMode());

  label.SetRTLAlignmentMode(Label::AUTO_DETECT_ALIGNMENT);
  EXPECT_EQ(Label::AUTO_DETECT_ALIGNMENT, label.GetRTLAlignmentMode());

  label.SetRTLAlignmentMode(Label::USE_UI_ALIGNMENT);
  EXPECT_EQ(Label::USE_UI_ALIGNMENT, label.GetRTLAlignmentMode());
}

TEST(LabelTest, MultiLineProperty) {
  Label label;
  EXPECT_FALSE(label.IsMultiLine());
  label.SetMultiLine(true);
  EXPECT_TRUE(label.IsMultiLine());
  label.SetMultiLine(false);
  EXPECT_FALSE(label.IsMultiLine());
}

TEST(LabelTest, TooltipProperty) {
  Label label;
  std::wstring test_text(L"My cool string.");
  label.SetText(test_text);

  std::wstring tooltip;
  EXPECT_TRUE(label.GetTooltipText(0, 0, &tooltip));
  EXPECT_STREQ(test_text.c_str(), tooltip.c_str());

  std::wstring tooltip_text(L"The tooltip!");
  label.SetTooltipText(tooltip_text);
  EXPECT_TRUE(label.GetTooltipText(0, 0, &tooltip));
  EXPECT_STREQ(tooltip_text.c_str(), tooltip.c_str());

  std::wstring empty_text;
  label.SetTooltipText(empty_text);
  EXPECT_TRUE(label.GetTooltipText(0, 0, &tooltip));
  EXPECT_STREQ(test_text.c_str(), tooltip.c_str());

  // Make the label big enough to hold the text
  // and expect there to be no tooltip.
  label.SetBounds(0, 0, 1000, 40);
  EXPECT_FALSE(label.GetTooltipText(0, 0, &tooltip));

  // Verify that setting the tooltip still shows it.
  label.SetTooltipText(tooltip_text);
  EXPECT_TRUE(label.GetTooltipText(0, 0, &tooltip));
  EXPECT_STREQ(tooltip_text.c_str(), tooltip.c_str());
  // Clear out the tooltip.
  label.SetTooltipText(empty_text);

  // Shrink the bounds and the tooltip should come back.
  label.SetBounds(0, 0, 1, 1);
  EXPECT_TRUE(label.GetTooltipText(0, 0, &tooltip));

  // Make the label multiline and there is no tooltip again.
  label.SetMultiLine(true);
  EXPECT_FALSE(label.GetTooltipText(0, 0, &tooltip));

  // Verify that setting the tooltip still shows it.
  label.SetTooltipText(tooltip_text);
  EXPECT_TRUE(label.GetTooltipText(0, 0, &tooltip));
  EXPECT_STREQ(tooltip_text.c_str(), tooltip.c_str());
  // Clear out the tooltip.
  label.SetTooltipText(empty_text);
}

TEST(LabelTest, Accessibility) {
  Label label;
  std::wstring test_text(L"My special text.");
  label.SetText(test_text);

  AccessibilityTypes::Role role;
  EXPECT_TRUE(label.GetAccessibleRole(&role));
  EXPECT_EQ(AccessibilityTypes::ROLE_TEXT, role);

  std::wstring name;
  EXPECT_TRUE(label.GetAccessibleName(&name));
  EXPECT_STREQ(test_text.c_str(), name.c_str());

  AccessibilityTypes::State state;
  EXPECT_TRUE(label.GetAccessibleState(&state));
  EXPECT_EQ(AccessibilityTypes::STATE_READONLY, state);
}

TEST(LabelTest, SingleLineSizing) {
  Label label;
  std::wstring test_text(L"A not so random string in one line.");
  label.SetText(test_text);

  // GetPreferredSize
  gfx::Size required_size = label.GetPreferredSize();
  EXPECT_GT(required_size.height(), kMinTextDimension);
  EXPECT_GT(required_size.width(), kMinTextDimension);

  // Test with highlights.
  label.SetDrawHighlighted(true);
  gfx::Size highlighted_size = label.GetPreferredSize();
  EXPECT_GT(highlighted_size.height(), required_size.height());
  EXPECT_GT(highlighted_size.width(), required_size.width());
  label.SetDrawHighlighted(false);

  // Test everything with borders.
  gfx::Insets border(10, 20, 30, 40);
  label.set_border(Border::CreateEmptyBorder(border.top(),
                                             border.left(),
                                             border.bottom(),
                                             border.right()));

  // GetPreferredSize and borders.
  label.SetBounds(0, 0, 0, 0);
  gfx::Size required_size_with_border = label.GetPreferredSize();
  EXPECT_EQ(required_size_with_border.height(),
            required_size.height() + border.height());
  EXPECT_EQ(required_size_with_border.width(),
            required_size.width() + border.width());
}

TEST(LabelTest, MultiLineSizing) {
  Label label;
  label.SetFocusable(false);
  std::wstring test_text(L"A random string\nwith multiple lines\nand returns!");
  label.SetText(test_text);
  label.SetMultiLine(true);

  // GetPreferredSize
  gfx::Size required_size = label.GetPreferredSize();
  EXPECT_GT(required_size.height(), kMinTextDimension);
  EXPECT_GT(required_size.width(), kMinTextDimension);

  // SizeToFit with unlimited width.
  label.SizeToFit(0);
  int required_width = label.GetLocalBounds(true).width();
  EXPECT_GT(required_width, kMinTextDimension);

  // SizeToFit with limited width.
  label.SizeToFit(required_width - 1);
  int constrained_width = label.GetLocalBounds(true).width();
  EXPECT_LT(constrained_width, required_width);
  EXPECT_GT(constrained_width, kMinTextDimension);

  // Change the width back to the desire width.
  label.SizeToFit(required_width);
  EXPECT_EQ(required_width, label.GetLocalBounds(true).width());

  // General tests for GetHeightForWidth.
  int required_height = label.GetHeightForWidth(required_width);
  EXPECT_GT(required_height, kMinTextDimension);
  int height_for_constrained_width = label.GetHeightForWidth(constrained_width);
  EXPECT_GT(height_for_constrained_width, required_height);
  // Using the constrained width or the required_width - 1 should give the
  // same result for the height because the constrainted width is the tight
  // width when given "required_width - 1" as the max width.
  EXPECT_EQ(height_for_constrained_width,
            label.GetHeightForWidth(required_width - 1));

  // Test everything with borders.
  gfx::Insets border(10, 20, 30, 40);
  label.set_border(Border::CreateEmptyBorder(border.top(),
                                             border.left(),
                                             border.bottom(),
                                             border.right()));

  // SizeToFit and borders.
  label.SizeToFit(0);
  int required_width_with_border = label.GetLocalBounds(true).width();
  EXPECT_EQ(required_width_with_border, required_width + border.width());

  // GetHeightForWidth and borders.
  int required_height_with_border =
      label.GetHeightForWidth(required_width_with_border);
  EXPECT_EQ(required_height_with_border, required_height + border.height());

  // Test that the border width is subtracted before doing the height
  // calculation.  If it is, then the height will grow when width
  // is shrunk.
  int height1 = label.GetHeightForWidth(required_width_with_border - 1);
  EXPECT_GT(height1, required_height_with_border);
  EXPECT_EQ(height1, height_for_constrained_width + border.height());

  // GetPreferredSize and borders.
  label.SetBounds(0, 0, 0, 0);
  gfx::Size required_size_with_border = label.GetPreferredSize();
  EXPECT_EQ(required_size_with_border.height(),
            required_size.height() + border.height());
  EXPECT_EQ(required_size_with_border.width(),
            required_size.width() + border.width());
}

TEST(LabelTest, DrawSingleLineString) {
  Label label;
  label.SetFocusable(false);

  // Turn off mirroring so that we don't need to figure out if
  // align right really means align left.
  label.EnableUIMirroringForRTLLanguages(false);

  std::wstring test_text(L"Here's a string with no returns.");
  label.SetText(test_text);
  gfx::Size required_size(label.GetPreferredSize());
  gfx::Size extra(22, 8);
  label.SetBounds(0,
                  0,
                  required_size.width() + extra.width(),
                  required_size.height() + extra.height());

  // Do some basic verifications for all three alignments.
  std::wstring paint_text;
  gfx::Rect text_bounds;
  int flags;

  // Centered text.
  label.CalculateDrawStringParams(&paint_text, &text_bounds, &flags);
  EXPECT_STREQ(test_text.c_str(), paint_text.c_str());
  // The text should be centered horizontally and vertically.
  EXPECT_EQ(extra.width() / 2, text_bounds.x());
  EXPECT_EQ(extra.height() / 2 , text_bounds.y());
  EXPECT_EQ(required_size.width(), text_bounds.width());
  EXPECT_EQ(required_size.height(), text_bounds.height());
  EXPECT_EQ(0, flags);

  // Left aligned text.
  label.SetHorizontalAlignment(Label::ALIGN_LEFT);
  paint_text.clear();
  text_bounds.SetRect(0, 0, 0, 0);
  label.CalculateDrawStringParams(&paint_text, &text_bounds, &flags);
  EXPECT_STREQ(test_text.c_str(), paint_text.c_str());
  // The text should be left aligned horizontally and centered vertically.
  EXPECT_EQ(0, text_bounds.x());
  EXPECT_EQ(extra.height() / 2 , text_bounds.y());
  EXPECT_EQ(required_size.width(), text_bounds.width());
  EXPECT_EQ(required_size.height(), text_bounds.height());
  EXPECT_EQ(0, flags);

  // Right aligned text.
  label.SetHorizontalAlignment(Label::ALIGN_RIGHT);
  paint_text.clear();
  text_bounds.SetRect(0, 0, 0, 0);
  label.CalculateDrawStringParams(&paint_text, &text_bounds, &flags);
  EXPECT_STREQ(test_text.c_str(), paint_text.c_str());
  // The text should be right aligned horizontally and centered vertically.
  EXPECT_EQ(extra.width(), text_bounds.x());
  EXPECT_EQ(extra.height() / 2 , text_bounds.y());
  EXPECT_EQ(required_size.width(), text_bounds.width());
  EXPECT_EQ(required_size.height(), text_bounds.height());
  EXPECT_EQ(0, flags);

  // Test single line drawing with a border.
  gfx::Insets border(39, 34, 8, 96);
  label.set_border(Border::CreateEmptyBorder(border.top(),
                                             border.left(),
                                             border.bottom(),
                                             border.right()));

  gfx::Size required_size_with_border(label.GetPreferredSize());
  EXPECT_EQ(required_size.width() + border.width(),
            required_size_with_border.width());
  EXPECT_EQ(required_size.height() + border.height(),
            required_size_with_border.height());
  label.SetBounds(0,
                  0,
                  required_size_with_border.width() + extra.width(),
                  required_size_with_border.height() + extra.height());

  // Centered text with border.
  label.SetHorizontalAlignment(Label::ALIGN_CENTER);
  paint_text.clear();
  text_bounds.SetRect(0, 0, 0, 0);
  label.CalculateDrawStringParams(&paint_text, &text_bounds, &flags);
  EXPECT_STREQ(test_text.c_str(), paint_text.c_str());
  // The text should be centered horizontally and vertically within the border.
  EXPECT_EQ(border.left() + extra.width() / 2, text_bounds.x());
  EXPECT_EQ(border.top() + extra.height() / 2 , text_bounds.y());
  EXPECT_EQ(required_size.width(), text_bounds.width());
  EXPECT_EQ(required_size.height(), text_bounds.height());
  EXPECT_EQ(0, flags);

  // Left aligned text with border.
  label.SetHorizontalAlignment(Label::ALIGN_LEFT);
  paint_text.clear();
  text_bounds.SetRect(0, 0, 0, 0);
  label.CalculateDrawStringParams(&paint_text, &text_bounds, &flags);
  EXPECT_STREQ(test_text.c_str(), paint_text.c_str());
  // The text should be left aligned horizontally and centered vertically.
  EXPECT_EQ(border.left(), text_bounds.x());
  EXPECT_EQ(border.top() + extra.height() / 2 , text_bounds.y());
  EXPECT_EQ(required_size.width(), text_bounds.width());
  EXPECT_EQ(required_size.height(), text_bounds.height());
  EXPECT_EQ(0, flags);

  // Right aligned text.
  label.SetHorizontalAlignment(Label::ALIGN_RIGHT);
  paint_text.clear();
  text_bounds.SetRect(0, 0, 0, 0);
  label.CalculateDrawStringParams(&paint_text, &text_bounds, &flags);
  EXPECT_STREQ(test_text.c_str(), paint_text.c_str());
  // The text should be right aligned horizontally and centered vertically.
  EXPECT_EQ(border.left() + extra.width(), text_bounds.x());
  EXPECT_EQ(border.top() + extra.height() / 2 , text_bounds.y());
  EXPECT_EQ(required_size.width(), text_bounds.width());
  EXPECT_EQ(required_size.height(), text_bounds.height());
  EXPECT_EQ(0, flags);
}

TEST(LabelTest, DrawMultiLineString) {
  Label label;
  label.SetFocusable(false);

  // Turn off mirroring so that we don't need to figure out if
  // align right really means align left.
  label.EnableUIMirroringForRTLLanguages(false);

  std::wstring test_text(L"Another string\nwith returns\n\n!");
  label.SetText(test_text);
  label.SetMultiLine(true);
  label.SizeToFit(0);

  // Do some basic verifications for all three alignments.
  std::wstring paint_text;
  gfx::Rect text_bounds;
  int flags;
  label.CalculateDrawStringParams(&paint_text, &text_bounds, &flags);
  EXPECT_STREQ(test_text.c_str(), paint_text.c_str());
  EXPECT_EQ(0, text_bounds.x());
  EXPECT_EQ(0, text_bounds.y());
  EXPECT_GT(text_bounds.width(), kMinTextDimension);
  EXPECT_GT(text_bounds.height(), kMinTextDimension);
  EXPECT_EQ(gfx::Canvas::MULTI_LINE | gfx::Canvas::TEXT_ALIGN_CENTER, flags);
  gfx::Rect center_bounds(text_bounds);

  label.SetHorizontalAlignment(Label::ALIGN_LEFT);
  paint_text.clear();
  text_bounds.SetRect(0, 0, 0, 0);
  label.CalculateDrawStringParams(&paint_text, &text_bounds, &flags);
  EXPECT_STREQ(test_text.c_str(), paint_text.c_str());
  EXPECT_EQ(0, text_bounds.x());
  EXPECT_EQ(0, text_bounds.y());
  EXPECT_GT(text_bounds.width(), kMinTextDimension);
  EXPECT_GT(text_bounds.height(), kMinTextDimension);
  EXPECT_EQ(gfx::Canvas::MULTI_LINE | gfx::Canvas::TEXT_ALIGN_LEFT, flags);

  label.SetHorizontalAlignment(Label::ALIGN_RIGHT);
  paint_text.clear();
  text_bounds.SetRect(0, 0, 0, 0);
  label.CalculateDrawStringParams(&paint_text, &text_bounds, &flags);
  EXPECT_STREQ(test_text.c_str(), paint_text.c_str());
  EXPECT_EQ(0, text_bounds.x());
  EXPECT_EQ(0, text_bounds.y());
  EXPECT_GT(text_bounds.width(), kMinTextDimension);
  EXPECT_GT(text_bounds.height(), kMinTextDimension);
  EXPECT_EQ(gfx::Canvas::MULTI_LINE | gfx::Canvas::TEXT_ALIGN_RIGHT, flags);

  // Test multiline drawing with a border.
  gfx::Insets border(19, 92, 23, 2);
  label.set_border(Border::CreateEmptyBorder(border.top(),
                                             border.left(),
                                             border.bottom(),
                                             border.right()));
  label.SizeToFit(0);
  label.SetHorizontalAlignment(Label::ALIGN_CENTER);
  paint_text.clear();
  text_bounds.SetRect(0, 0, 0, 0);
  label.CalculateDrawStringParams(&paint_text, &text_bounds, &flags);
  EXPECT_STREQ(test_text.c_str(), paint_text.c_str());
  EXPECT_EQ(center_bounds.x() + border.left(), text_bounds.x());
  EXPECT_EQ(center_bounds.y() + border.top(), text_bounds.y());
  EXPECT_EQ(center_bounds.width(), text_bounds.width());
  EXPECT_EQ(center_bounds.height(), text_bounds.height());
  EXPECT_EQ(gfx::Canvas::MULTI_LINE | gfx::Canvas::TEXT_ALIGN_CENTER, flags);
}

}  // namespace views
