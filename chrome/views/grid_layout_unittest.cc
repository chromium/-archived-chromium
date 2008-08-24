// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/grid_layout.h"
#include "chrome/views/view.h"

#include "testing/gtest/include/gtest/gtest.h"

using ChromeViews::ColumnSet;
using ChromeViews::GridLayout;
using ChromeViews::View;

static void ExpectViewBoundsEquals(int x, int y, int w, int h,
                                   const View* view) {
  EXPECT_EQ(x, view->GetX());
  EXPECT_EQ(y, view->GetY());
  EXPECT_EQ(w, view->GetWidth());
  EXPECT_EQ(h, view->GetHeight());
}

class SettableSizeView : public View {
 public:
  explicit SettableSizeView(const CSize& pref) {
    pref_ = pref;
  }

  virtual void GetPreferredSize(CSize *out) {
    *out = pref_;
  }

 private:
  CSize pref_;
};

class GridLayoutTest : public testing::Test {
 public:
  virtual void SetUp() {
    layout = new GridLayout(&host);
  }

  virtual void TearDown() {
    delete layout;
  }

  virtual void RemoveAll() {
    for (int i = host.GetChildViewCount() - 1; i >= 0; i--) {
      host.RemoveChildView(host.GetChildViewAt(i));
    }
  }

  void GetPreferredSize() {
    layout->GetPreferredSize(&host, &pref);
  }

  CSize pref;
  CRect bounds;
  View host;
  GridLayout* layout;
};

class GridLayoutAlignmentTest : public testing::Test {
 public:
   GridLayoutAlignmentTest() :
       host(),
       v1(CSize(10, 20)),
       layout(new GridLayout(&host)) {}

  virtual void SetUp() {
  }

  virtual void TearDown() {
    delete layout;
  }

  virtual void RemoveAll() {
    for (int i = host.GetChildViewCount() - 1; i >= 0; i--) {
      host.RemoveChildView(host.GetChildViewAt(i));
    }
  }

  void TestAlignment(GridLayout::Alignment alignment, CRect* bounds) {
    ColumnSet* c1 = layout->AddColumnSet(0);
    c1->AddColumn(alignment, alignment, 1, GridLayout::USE_PREF, 0, 0);
    layout->StartRow(1, 0);
    layout->AddView(&v1);
    CSize pref;
    layout->GetPreferredSize(&host, &pref);
    EXPECT_TRUE(CSize(10, 20) == pref);
    host.SetBounds(0, 0, 100, 100);
    layout->Layout(&host);
    v1.GetBounds(bounds);
    RemoveAll();
  }

  View host;
  SettableSizeView v1;
  GridLayout* layout;
};

TEST_F(GridLayoutAlignmentTest, Fill) {
  CRect bounds;
  TestAlignment(GridLayout::FILL, &bounds);
  EXPECT_TRUE(CRect(0, 0, 100, 100) == bounds);
}

TEST_F(GridLayoutAlignmentTest, Leading) {
  CRect bounds;
  TestAlignment(GridLayout::LEADING, &bounds);
  EXPECT_TRUE(CRect(0, 0, 10, 20) == bounds);
}

TEST_F(GridLayoutAlignmentTest, Center) {
  CRect bounds;
  TestAlignment(GridLayout::CENTER, &bounds);
  EXPECT_TRUE(CRect(45, 40, 55, 60) == bounds);
}

TEST_F(GridLayoutAlignmentTest, Trailing) {
  CRect bounds;
  TestAlignment(GridLayout::TRAILING, &bounds);
  EXPECT_TRUE(CRect(90, 80, 100, 100) == bounds);
}

TEST_F(GridLayoutTest, TwoColumns) {
  SettableSizeView v1(CSize(10, 20));
  SettableSizeView v2(CSize(20, 20));
  ColumnSet* c1 = layout->AddColumnSet(0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  layout->AddView(&v1);
  layout->AddView(&v2);

  GetPreferredSize();
  EXPECT_TRUE(CSize(30, 20) == pref);

  host.SetBounds(0, 0, pref.cx, pref.cy);
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 10, 20, &v1);
  ExpectViewBoundsEquals(10, 0, 20, 20, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, ColSpan1) {
  SettableSizeView v1(CSize(100, 20));
  SettableSizeView v2(CSize(10, 40));
  ColumnSet* c1 = layout->AddColumnSet(0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                1, GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  layout->AddView(&v1, 2, 1);
  layout->StartRow(0, 0);
  layout->AddView(&v2);

  GetPreferredSize();
  EXPECT_TRUE(CSize(100, 60) == pref);

  host.SetBounds(0, 0, pref.cx, pref.cy);
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 100, 20, &v1);
  ExpectViewBoundsEquals(0, 20, 10, 40, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, ColSpan2) {
  SettableSizeView v1(CSize(100, 20));
  SettableSizeView v2(CSize(10, 20));
  ColumnSet* c1 = layout->AddColumnSet(0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                1, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  layout->AddView(&v1, 2, 1);
  layout->StartRow(0, 0);
  layout->SkipColumns(1);
  layout->AddView(&v2);

  GetPreferredSize();
  EXPECT_TRUE(CSize(100, 40) == pref);

  host.SetBounds(0, 0, pref.cx, pref.cy);
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 100, 20, &v1);
  ExpectViewBoundsEquals(90, 20, 10, 20, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, ColSpan3) {
  SettableSizeView v1(CSize(100, 20));
  SettableSizeView v2(CSize(10, 20));
  SettableSizeView v3(CSize(10, 20));
  ColumnSet* c1 = layout->AddColumnSet(0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  layout->AddView(&v1, 2, 1);
  layout->StartRow(0, 0);
  layout->AddView(&v2);
  layout->AddView(&v3);

  GetPreferredSize();
  EXPECT_TRUE(CSize(100, 40) == pref);

  host.SetBounds(0, 0, pref.cx, pref.cy);
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 100, 20, &v1);
  ExpectViewBoundsEquals(0, 20, 10, 20, &v2);
  ExpectViewBoundsEquals(50, 20, 10, 20, &v3);

  RemoveAll();
}


TEST_F(GridLayoutTest, ColSpan4) {
  ChromeViews::ColumnSet* set = layout->AddColumnSet(0);

  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                 GridLayout::USE_PREF, 0, 0);
  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                 GridLayout::USE_PREF, 0, 0);

  SettableSizeView v1(CSize(10, 10));
  SettableSizeView v2(CSize(10, 10));
  SettableSizeView v3(CSize(25, 20));
  layout->StartRow(0, 0);
  layout->AddView(&v1);
  layout->AddView(&v2);
  layout->StartRow(0, 0);
  layout->AddView(&v3, 2, 1);

  GetPreferredSize();
  EXPECT_TRUE(CSize(25, 30) == pref);

  host.SetBounds(0, 0, pref.cx, pref.cy);
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 10, 10, &v1);
  ExpectViewBoundsEquals(12, 0, 10, 10, &v2);
  ExpectViewBoundsEquals(0, 10, 25, 20, &v3);

  RemoveAll();
}

TEST_F(GridLayoutTest, SameSizeColumns) {
  SettableSizeView v1(CSize(50, 20));
  SettableSizeView v2(CSize(10, 10));
  ColumnSet* c1 = layout->AddColumnSet(0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  c1->LinkColumnSizes(0, 1, -1);
  layout->StartRow(0, 0);
  layout->AddView(&v1);
  layout->AddView(&v2);

  CSize pref;
  layout->GetPreferredSize(&host, &pref);
  EXPECT_TRUE(CSize(100, 20) == pref);

  host.SetBounds(0, 0, pref.cx, pref.cy);
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 50, 20, &v1);
  ExpectViewBoundsEquals(50, 0, 10, 10, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, HorizontalResizeTest1) {
  SettableSizeView v1(CSize(50, 20));
  SettableSizeView v2(CSize(10, 10));
  ColumnSet* c1 = layout->AddColumnSet(0);
  c1->AddColumn(GridLayout::FILL, GridLayout::LEADING,
                1, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  layout->AddView(&v1);
  layout->AddView(&v2);

  host.SetBounds(0, 0, 110, 20);
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 100, 20, &v1);
  ExpectViewBoundsEquals(100, 0, 10, 10, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, HorizontalResizeTest2) {
  SettableSizeView v1(CSize(50, 20));
  SettableSizeView v2(CSize(10, 10));
  ColumnSet* c1 = layout->AddColumnSet(0);
  c1->AddColumn(GridLayout::FILL, GridLayout::LEADING,
                1, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::TRAILING, GridLayout::LEADING,
                1, GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  layout->AddView(&v1);
  layout->AddView(&v2);

  host.SetBounds(0, 0, 120, 20);
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 80, 20, &v1);
  ExpectViewBoundsEquals(110, 0, 10, 10, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, TestVerticalResize1) {
  SettableSizeView v1(CSize(50, 20));
  SettableSizeView v2(CSize(10, 10));
  ColumnSet* c1 = layout->AddColumnSet(0);
  c1->AddColumn(GridLayout::FILL, GridLayout::FILL,
                1, GridLayout::USE_PREF, 0, 0);
  layout->StartRow(1, 0);
  layout->AddView(&v1);
  layout->StartRow(0, 0);
  layout->AddView(&v2);

  GetPreferredSize();
  EXPECT_TRUE(CSize(50, 30) == pref);

  host.SetBounds(0, 0, 50, 100);
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 50, 90, &v1);
  ExpectViewBoundsEquals(0, 90, 50, 10, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, Insets) {
  SettableSizeView v1(CSize(10, 20));
  ColumnSet* c1 = layout->AddColumnSet(0);
  layout->SetInsets(1, 2, 3, 4);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  layout->AddView(&v1);

  GetPreferredSize();
  EXPECT_TRUE(CSize(16, 24) == pref);

  host.SetBounds(0, 0, pref.cx, pref.cy);
  layout->Layout(&host);
  ExpectViewBoundsEquals(2, 1, 10, 20, &v1);

  RemoveAll();
}

TEST_F(GridLayoutTest, FixedSize) {
  layout->SetInsets(2, 2, 2, 2);

  ChromeViews::ColumnSet* set = layout->AddColumnSet(0);

  int column_count = 4;
  int title_width = 100;
  int row_count = 2;
  int pref_width = 10;
  int pref_height = 20;

  for (int i = 0; i < column_count; ++i) {
    set->AddColumn(ChromeViews::GridLayout::CENTER,
                   ChromeViews::GridLayout::CENTER,
                   0,
                   ChromeViews::GridLayout::FIXED,
                   title_width,
                   title_width);
  }

  for (int row = 0; row < row_count; ++row) {
    layout->StartRow(0, 0);
    for (int col = 0; col < column_count; ++col) {
      layout->AddView(new SettableSizeView(CSize(pref_width, pref_height)));
    }
  }

  layout->Layout(&host);

  for (int i = 0; i < column_count; ++i) {
    for (int row = 0; row < row_count; ++row) {
      View* view = host.GetChildViewAt(row * column_count + i);
      ExpectViewBoundsEquals(2 + title_width * i + (title_width - pref_width) / 2,
                             2 + pref_height * row,
                             pref_width, pref_height, view);
    }
  }

  GetPreferredSize();
  EXPECT_TRUE(CSize(column_count * title_width + 4,
                    row_count * pref_height + 4) == pref);
}

TEST_F(GridLayoutTest, RowSpanWithPaddingRow) {
  ChromeViews::ColumnSet* set = layout->AddColumnSet(0);

  set->AddColumn(ChromeViews::GridLayout::CENTER,
                 ChromeViews::GridLayout::CENTER,
                 0,
                 ChromeViews::GridLayout::FIXED,
                 10,
                 10);

  layout->StartRow(0, 0);
  layout->AddView(new SettableSizeView(CSize(10, 10)), 1, 2);
  layout->AddPaddingRow(0, 10);
}

TEST_F(GridLayoutTest, RowSpan) {
  ChromeViews::ColumnSet* set = layout->AddColumnSet(0);

  set->AddColumn(ChromeViews::GridLayout::LEADING,
                 ChromeViews::GridLayout::LEADING,
                 0,
                 ChromeViews::GridLayout::USE_PREF,
                 0,
                 0);
  set->AddColumn(ChromeViews::GridLayout::LEADING,
                 ChromeViews::GridLayout::LEADING,
                 0,
                 ChromeViews::GridLayout::USE_PREF,
                 0,
                 0);

  layout->StartRow(0, 0);
  layout->AddView(new SettableSizeView(CSize(20, 10)));
  layout->AddView(new SettableSizeView(CSize(20, 40)), 1, 2);
  layout->StartRow(1, 0);
  ChromeViews::View* s3 = new SettableSizeView(CSize(20, 10));
  layout->AddView(s3);

  GetPreferredSize();
  EXPECT_TRUE(CSize(40, 40) == pref);

  host.SetBounds(0, 0, pref.cx, pref.cy);
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 10, 20, 10, s3);
}

TEST_F(GridLayoutTest, RowSpan2) {
  ChromeViews::ColumnSet* set = layout->AddColumnSet(0);

  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0, GridLayout::USE_PREF, 0, 0);
  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0,GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(new SettableSizeView(CSize(20, 20)));
  ChromeViews::View* s3 = new SettableSizeView(CSize(64, 64));
  layout->AddView(s3, 1, 3);

  layout->AddPaddingRow(0, 10);

  layout->StartRow(0, 0);
  layout->AddView(new SettableSizeView(CSize(10, 20)));

  GetPreferredSize();
  EXPECT_TRUE(CSize(84, 64) == pref);

  host.SetBounds(0, 0, pref.cx, pref.cy);
  layout->Layout(&host);
  ExpectViewBoundsEquals(20, 0, 64, 64, s3);
}

TEST_F(GridLayoutTest, FixedViewWidth) {
  ChromeViews::ColumnSet* set = layout->AddColumnSet(0);

  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0, GridLayout::USE_PREF, 0, 0);
  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0,GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  View* view = new SettableSizeView(CSize(30, 40));
  layout->AddView(view, 1, 1, GridLayout::LEADING, GridLayout::LEADING, 10, 0);

  GetPreferredSize();
  EXPECT_EQ(10, pref.cx);
  EXPECT_EQ(40, pref.cy);

  host.SetBounds(0, 0, pref.cx, pref.cy);
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 10, 40, view);
}

TEST_F(GridLayoutTest, FixedViewHeight) {
  ChromeViews::ColumnSet* set = layout->AddColumnSet(0);

  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0, GridLayout::USE_PREF, 0, 0);
  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0,GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  View* view = new SettableSizeView(CSize(30, 40));
  layout->AddView(view, 1, 1, GridLayout::LEADING, GridLayout::LEADING, 0, 10);

  GetPreferredSize();
  EXPECT_EQ(30, pref.cx);
  EXPECT_EQ(10, pref.cy);

  host.SetBounds(0, 0, pref.cx, pref.cy);
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 30, 10, view);
}

