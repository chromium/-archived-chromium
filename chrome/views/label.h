// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_LABEL_H__
#define CHROME_VIEWS_LABEL_H__

#include "chrome/common/gfx/chrome_font.h"
#include "chrome/views/view.h"
#include "googleurl/src/gurl.h"
#include "skia/include/SkColor.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace views {

/////////////////////////////////////////////////////////////////////////////
//
// Label class
//
// A label is a view subclass that can display a string.
//
/////////////////////////////////////////////////////////////////////////////
class Label : public View {
 public:
  enum Alignment { ALIGN_LEFT = 0,
                   ALIGN_CENTER,
                   ALIGN_RIGHT };

  // The view class name.
  static const char kViewClassName[];

  // Create a new label with a default font and empty value
  Label();

  // Create a new label with a default font
  explicit Label(const std::wstring& text);

  Label(const std::wstring& text, const ChromeFont& font);

  virtual ~Label();

  // Overridden to compute the size required to display this label
  virtual gfx::Size GetPreferredSize();

  // Return the height necessary to display this label with the provided width.
  // This method is used to layout multi-line labels. It is equivalent to
  // GetPreferredSize().height() if the receiver is not multi-line
  virtual int GetHeightForWidth(int w);

  // Returns chrome/views/Label.
  virtual std::string GetClassName() const;

  // Overridden to paint
  virtual void Paint(ChromeCanvas* canvas);

  // If the mouse is over the label, and a mouse over background has been
  // specified, its used. Otherwise super's implementation is invoked
  virtual void PaintBackground(ChromeCanvas* canvas);

  // Set the font.
  void SetFont(const ChromeFont& font);

  // Return the font used by this label
  ChromeFont GetFont() const;

  // Set the label text.
  void SetText(const std::wstring& text);

  // Return the label text.
  const std::wstring GetText() const;

  // Set URL Value - text_ is set to spec().
  void SetURL(const GURL& url);

  // Return the label URL.
  const GURL GetURL() const;

  // Set the color
  virtual void SetColor(const SkColor& color);

  // Return a reference to the currently used color
  virtual const SkColor GetColor() const;

  // Alignment
  void SetHorizontalAlignment(Alignment a);
  Alignment GetHorizontalAlignment() const;

  // Set whether the label text can wrap on multiple lines.
  // Default is false
  void SetMultiLine(bool f);

  // Return whether the label text can wrap on multiple lines
  bool IsMultiLine();

  // Sets the tooltip text.  Default behavior for a label (single-line) is to
  // show the full text if it is wider than its bounds.  Calling this overrides
  // the default behavior and lets you set a custom tooltip.  To revert to
  // default behavior, call this with an empty string.
  void SetTooltipText(const std::wstring& tooltip_text);

  // Gets the tooltip text for labels that are wider than their bounds, except
  // when the label is multiline, in which case it just returns false (no
  // tooltip).  If a custom tooltip has been specified with SetTooltipText()
  // it is returned instead.
  virtual bool GetTooltipText(int x, int y, std::wstring* tooltip);

  // Mouse enter/exit are overridden to render mouse over background color. These
  // invoke SetContainsMouse as necessary.
  virtual void OnMouseMoved(const MouseEvent& e);
  virtual void OnMouseEntered(const MouseEvent& event);
  virtual void OnMouseExited(const MouseEvent& event);

  // The background color to use when the mouse is over the label. Label
  // takes ownership of the Background.
  void SetMouseOverBackground(Background* background);
  const Background* GetMouseOverBackground() const;

  // Sets the enabled state. Setting the enabled state resets the color.
  virtual void SetEnabled(bool enabled);

  // Resizes the label so its width is set to the width of the longest line and
  // its height deduced accordingly.
  // This is only intended for multi-line labels and is useful when the label's
  // text contains several lines separated with \n.
  // |max_width| is the maximum width that will be used (longer lines will be
  // wrapped).  If 0, no maximum width is enforced.
  void SizeToFit(int max_width);

#if defined(OS_WIN)
  // TODO(port): Make portable equivalents of accessibility-related functions.

  // Returns the MSAA role of the current view. The role is what assistive
  // technologies (ATs) use to determine what behavior to expect from a given
  // control.
  bool GetAccessibleRole(VARIANT* role);

  // Returns a brief, identifying string, containing a unique, readable name.
  bool GetAccessibleName(std::wstring* name);

  // Returns the MSAA state of the current view. Sets the input VARIANT
  // appropriately, and returns true if a change was performed successfully.
  // Overriden from View.
  virtual bool GetAccessibleState(VARIANT* state);
#endif  // defined(OS_WIN)

  // Gets/sets the flag to determine whether the label should be collapsed when
  // it's hidden (not visible). If this flag is true, the label will return a
  // preferred size of (0, 0) when it's not visible.
  void set_collapse_when_hidden(bool value) { collapse_when_hidden_ = value; }
  bool collapse_when_hidden() const { return collapse_when_hidden_; }

 private:
  // These tests call CalculateDrawStringParams in order to verify the
  // calculations done for drawing text.
  FRIEND_TEST(LabelTest, DrawSingleLineString);
  FRIEND_TEST(LabelTest, DrawMultiLineString);

  static ChromeFont GetDefaultFont();

  // Returns parameters to be used for the DrawString call.
  void CalculateDrawStringParams(std::wstring* paint_text,
                                 gfx::Rect* text_bounds,
                                 int* flags);

  // If the mouse is over the text, SetContainsMouse(true) is invoked, otherwise
  // SetContainsMouse(false) is invoked.
  void UpdateContainsMouse(const MouseEvent& event);

  // Updates whether the mouse is contained in the Label. If the new value
  // differs from the current value, and a mouse over background is specified,
  // SchedulePaint is invoked.
  void SetContainsMouse(bool contains_mouse);

  // Returns where the text is drawn, in the receivers coordinate system.
  gfx::Rect GetTextBounds();

  int ComputeMultiLineFlags();
  gfx::Size GetTextSize();
  void Init(const std::wstring& text, const ChromeFont& font);
  std::wstring text_;
  GURL url_;
  ChromeFont font_;
  SkColor color_;
  gfx::Size text_size_;
  bool text_size_valid_;
  bool is_multi_line_;
  bool url_set_;
  Alignment horiz_alignment_;
  std::wstring tooltip_text_;
  // Whether the mouse is over this label.
  bool contains_mouse_;
  scoped_ptr<Background> mouse_over_background_;
  // Whether to collapse the label when it's not visible.
  bool collapse_when_hidden_;

  DISALLOW_COPY_AND_ASSIGN(Label);
};

}  // namespace views

#endif  // CHROME_VIEWS_VIEW_H__
