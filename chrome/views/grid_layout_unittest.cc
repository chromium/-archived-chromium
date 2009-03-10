// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/grid_layout.h"
#include "chrome/views/view.h"

#include "testing/gtest/include/gtest/gtest.h"

using views::ColumnSet;
using views::GridLayout;
using views::View;

static void ExpectViewBoundsEquals(int x, int y, int w, int h,
                                   const View* view) {
  EXPECT_EQ(x, view->x());
  EXPECT_EQ(y, view->y());
  EXPECT_EQ(w, view->width());
  EXPECT_EQ(h, view->height());
}

class SettableSizeView : public View {
 public:
  explicit SettableSizeView(const gfx::Size& pref) {
    pref_ = pref;
  }

  virtual gfx::Size GetPreferredSize() {
    return pref_;
  }

 private:
   gfx::Size pref_;
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
    pref = layout->GetPreferredSize(&host);
  }

  gfx::Size pref;
  CRect bounds;
  View host;
  GridLayout* layout;
};

class GridLayoutAlignmentTest : public testing::Test {
 public:
   GridLayoutAlignmentTest() :
       host(),
         v1(gfx::Size(10, 20)),
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
    gfx::Size pref = layout->GetPreferredSize(&host);
    EXPECT_TRUE(gfx::Size(10, 20) == pref);
    host.SetBounds(0, 0, 100, 100);
    layout->Layout(&host);
    *bounds = v1.bounds().ToRECT();
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
  SettableSizeView v1(gfx::Size(10, 20));
  SettableSizeView v2(gfx::Size(20, 20));
  ColumnSet* c1 = layout->AddColumnSet(0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  layout->AddView(&v1);
  layout->AddView(&v2);

  GetPreferredSize();
  EXPECT_TRUE(gfx::Size(30, 20) == pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 10, 20, &v1);
  ExpectViewBoundsEquals(10, 0, 20, 20, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, ColSpan1) {
  SettableSizeView v1(gfx::Size(100, 20));
  SettableSizeView v2(gfx::Size(10, 40));
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
  EXPECT_TRUE(gfx::Size(100, 60) == pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 100, 20, &v1);
  ExpectViewBoundsEquals(0, 20, 10, 40, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, ColSpan2) {
  SettableSizeView v1(gfx::Size(100, 20));
  SettableSizeView v2(gfx::Size(10, 20));
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
  EXPECT_TRUE(gfx::Size(100, 40) == pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 100, 20, &v1);
  ExpectViewBoundsEquals(90, 20, 10, 20, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, ColSpan3) {
  SettableSizeView v1(gfx::Size(100, 20));
  SettableSizeView v2(gfx::Size(10, 20));
  SettableSizeView v3(gfx::Size(10, 20));
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
  EXPECT_TRUE(gfx::Size(100, 40) == pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 100, 20, &v1);
  ExpectViewBoundsEquals(0, 20, 10, 20, &v2);
  ExpectViewBoundsEquals(50, 20, 10, 20, &v3);

  RemoveAll();
}


TEST_F(GridLayoutTest, ColSpan4) {
  views::ColumnSet* set = layout->AddColumnSet(0);

  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                 GridLayout::USE_PREF, 0, 0);
  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                 GridLayout::USE_PREF, 0, 0);

  SettableSizeView v1(gfx::Size(10, 10));
  SettableSizeView v2(gfx::Size(10, 10));
  SettableSizeView v3(gfx::Size(25, 20));
  layout->StartRow(0, 0);
  layout->AddView(&v1);
  layout->AddView(&v2);
  layout->StartRow(0, 0);
  layout->AddView(&v3, 2, 1);

  GetPreferredSize();
  EXPECT_TRUE(gfx::Size(25, 30) == pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 10, 10, &v1);
  ExpectViewBoundsEquals(12, 0, 10, 10, &v2);
  ExpectViewBoundsEquals(0, 10, 25, 20, &v3);

  RemoveAll();
}

TEST_F(GridLayoutTest, SameSizeColumns) {
  SettableSizeView v1(gfx::Size(50, 20));
  SettableSizeView v2(gfx::Size(10, 10));
  ColumnSet* c1 = layout->AddColumnSet(0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  c1->LinkColumnSizes(0, 1, -1);
  layout->StartRow(0, 0);
  layout->AddView(&v1);
  layout->AddView(&v2);

  gfx::Size pref = layout->GetPreferredSize(&host);
  EXPECT_TRUE(gfx::Size(100, 20) == pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 50, 20, &v1);
  ExpectViewBoundsEquals(50, 0, 10, 10, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, HorizontalResizeTest1) {
  SettableSizeView v1(gfx::Size(50, 20));
  SettableSizeView v2(gfx::Size(10, 10));
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
  SettableSizeView v1(gfx::Size(50, 20));
  SettableSizeView v2(gfx::Size(10, 10));
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
  SettableSizeView v1(gfx::Size(50, 20));
  SettableSizeView v2(gfx::Size(10, 10));
  ColumnSet* c1 = layout->AddColumnSet(0);
  c1->AddColumn(GridLayout::FILL, GridLayout::FILL,
                1, GridLayout::USE_PREF, 0, 0);
  layout->StartRow(1, 0);
  layout->AddView(&v1);
  layout->StartRow(0, 0);
  layout->AddView(&v2);

  GetPreferredSize();
  EXPECT_TRUE(gfx::Size(50, 30) == pref);

  host.SetBounds(0, 0, 50, 100);
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 50, 90, &v1);
  ExpectViewBoundsEquals(0, 90, 50, 10, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, Insets) {
  SettableSizeView v1(gfx::Size(10, 20));
  ColumnSet* c1 = layout->AddColumnSet(0);
  layout->SetInsets(1, 2, 3, 4);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  layout->AddView(&v1);

  GetPreferredSize();
  EXPECT_TRUE(gfx::Size(16, 24) == pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout->Layout(&host);
  ExpectViewBoundsEquals(2, 1, 10, 20, &v1);

  RemoveAll();
}

TEST_F(GridLayoutTest, FixedSize) {
  layout->SetInsets(2, 2, 2, 2);

  views::ColumnSet* set = layout->AddColumnSet(0);

  int column_count = 4;
  int title_width = 100;
  int row_count = 2;
  int pref_width = 10;
  int pref_height = 20;

  for (int i = 0; i < column_count; ++i) {
    set->AddColumn(views::GridLayout::CENTER,
                   views::GridLayout::CENTER,
                   0,
                   views::GridLayout::FIXED,
                   title_width,
                   title_width);
  }

  for (int row = 0; row < row_count; ++row) {
    layout->StartRow(0, 0);
    for (int col = 0; col < column_count; ++col) {
      layout->AddView(new SettableSizeView(gfx::Size(pref_width, pref_height)));
    }
  }

  layout->Layout(&host);

  for (int i = 0; i < column_count; ++i) {
    for (int row = 0; row < row_count; ++row) {
      View* view = host.GetChildViewAt(row * column_count + i);
      ExpectViewBoundsEquals(
          2 + title_width * i + (title_width - pref_width) / 2,
          2 + pref_height * row,
          pref_width,
          pref_height, view);
    }
  }

  GetPreferredSize();
  EXPECT_TRUE(gfx::Size(column_count * title_width + 4,
                        row_count * pref_height + 4) == pref);
}

TEST_F(GridLayoutTest, RowSpanWithPaddingRow) {
  views::ColumnSet* set = layout->AddColumnSet(0);

  set->AddColumn(views::GridLayout::CENTER,
                 views::GridLayout::CENTER,
                 0,
                 views::GridLayout::FIXED,
                 10,
                 10);

  layout->StartRow(0, 0);
  layout->AddView(new SettableSizeView(gfx::Size(10, 10)), 1, 2);
  layout->AddPaddingRow(0, 10);
}

TEST_F(GridLayoutTest, RowSpan) {
  views::ColumnSet* set = layout->AddColumnSet(0);

  set->AddColumn(views::GridLayout::LEADING,
                 views::GridLayout::LEADING,
                 0,
                 views::GridLayout::USE_PREF,
                 0,
                 0);
  set->AddColumn(views::GridLayout::LEADING,
                 views::GridLayout::LEADING,
                 0,
                 views::GridLayout::USE_PREF,
                 0,
                 0);

  layout->StartRow(0, 0);
  layout->AddView(new SettableSizeView(gfx::Size(20, 10)));
  layout->AddView(new SettableSizeView(gfx::Size(20, 40)), 1, 2);
  layout->StartRow(1, 0);
  views::View* s3 = new SettableSizeView(gfx::Size(20, 10));
  layout->AddView(s3);

  GetPreferredSize();
  EXPECT_TRUE(gfx::Size(40, 40) == pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 10, 20, 10, s3);
}

TEST_F(GridLayoutTest, RowSpan2) {
  views::ColumnSet* set = layout->AddColumnSet(0);

  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0, GridLayout::USE_PREF, 0, 0);
  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0,GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(new SettableSizeView(gfx::Size(20, 20)));
  views::View* s3 = new SettableSizeView(gfx::Size(64, 64));
  layout->AddView(s3, 1, 3);

  layout->AddPaddingRow(0, 10);

  layout->StartRow(0, 0);
  layout->AddView(new SettableSizeView(gfx::Size(10, 20)));

  GetPreferredSize();
  EXPECT_TRUE(gfx::Size(84, 64) == pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout->Layout(&host);
  ExpectViewBoundsEquals(20, 0, 64, 64, s3);
}

TEST_F(GridLayoutTest, FixedViewWidth) {
  views::ColumnSet* set = layout->AddColumnSet(0);

  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0, GridLayout::USE_PREF, 0, 0);
  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0,GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  View* view = new SettableSizeView(gfx::Size(30, 40));
  layout->AddView(view, 1, 1, GridLayout::LEADING, GridLayout::LEADING, 10, 0);

  GetPreferredSize();
  EXPECT_EQ(10, pref.width());
  EXPECT_EQ(40, pref.height());

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 10, 40, view);
}

TEST_F(GridLayoutTest, FixedViewHeight) {
  views::ColumnSet* set = layout->AddColumnSet(0);

  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0, GridLayout::USE_PREF, 0, 0);
  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0,GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  View* view = new SettableSizeView(gfx::Size(30, 40));
  layout->AddView(view, 1, 1, GridLayout::LEADING, GridLayout::LEADING, 0, 10);

  GetPreferredSize();
  EXPECT_EQ(30, pref.width());
  EXPECT_EQ(10, pref.height());

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout->Layout(&host);
  ExpectViewBoundsEquals(0, 0, 30, 10, view);
}
