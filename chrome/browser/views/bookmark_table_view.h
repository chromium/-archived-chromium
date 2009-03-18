// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BOOKMARK_TABLE_VIEW_H_
#define CHROME_BROWSER_VIEWS_BOOKMARK_TABLE_VIEW_H_

#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_drop_info.h"
#include "chrome/views/controls/menu/menu.h"
#include "chrome/views/controls/table/table_view.h"

class BookmarkModel;
class BookmarkNode;
class BookmarkTableModel;
class ChromeFont;
class OSExchangeData;
class PrefService;
class Profile;

// A TableView implementation that shows a BookmarkTableModel.
// BookmarkTableView provides drag and drop support as well as showing a
// separate set of columns when showing search results.
class BookmarkTableView : public views::TableView {
 public:
  BookmarkTableView(Profile* profile, BookmarkTableModel* model);

  // Drag and drop methods.
  virtual bool CanDrop(const OSExchangeData& data);
  virtual void OnDragEntered(const views::DropTargetEvent& event);
  virtual int OnDragUpdated(const views::DropTargetEvent& event);
  virtual void OnDragExited();
  virtual int OnPerformDrop(const views::DropTargetEvent& event);

  // Sets the parent of the nodes being displayed. For search and recently
  // found results |parent| is NULL.
  void set_parent_node(BookmarkNode* parent) { parent_node_ = parent; }

  // Sets whether the path column should be shown. The path column is shown
  // for search results and recently bookmarked.
  void SetShowPathColumn(bool show_path_column);

  // The model as a BookmarkTableModel.
  BookmarkTableModel* bookmark_table_model() const;

  // Saves the widths of the table columns.
  void SaveColumnConfiguration();

  // Sets the text to display on top of the table. This is useful if the table
  // is empty and you want to inform the user why.
  void SetAltText(const std::wstring& alt_text);

 protected:
  // Overriden to draw a drop indicator when dropping between rows.
  virtual void PostPaint();

  // Overriden to start a drag.
  virtual LRESULT OnNotify(int w_param, LPNMHDR l_param);

 private:
  // DropPosition identifies where the drop should occur.
  struct DropPosition {
    DropPosition() : index(-1), on(false) {}
    DropPosition(int index, bool on) : index(index), on(on) {}

    bool equals(const DropPosition& position) const {
      return index == position.index && on == position.on;
    }

    // The index into the table model as to where the drop should occur. This
    // may == the row count of the table.
    int index;

    // Whether drop is on the item at the specified index. If false, the drop
    // is at the specified index.
    bool on;
  };

  // Information used when we're the drop target of a drag and drop operation.
  class DropInfo : public BookmarkDropInfo {
   public:
    explicit DropInfo(BookmarkTableView* view)
        : BookmarkDropInfo(view->GetNativeControlHWND(),
                           view->content_offset()),
          view_(view) {}

    // Overriden to invoke UpdateDropInfo.
    virtual void Scrolled();

    // The position the drop is to occur at.
    void set_position(const DropPosition& position) {
      position_ = position;
    }
    const DropPosition& position() { return position_; }

   private:
    DropPosition position_;
    BookmarkTableView* view_;

    DISALLOW_COPY_AND_ASSIGN(DropInfo);
  };
  friend class DropInfo;

  // Updates drop info. This is invoked both from OnDragUpdated and when we
  // autoscroll during a drop.
  int UpdateDropInfo();

  // Starts a drop operation.
  void BeginDrag();

  // Returns the drop operation for the specified position.
  int CalculateDropOperation(const DropPosition& position);

  // Performs the drop operation.
  void OnPerformDropImpl();

  // Sets the position of the drop. If this differs from the current position
  // UpdateDropIndicator is invoked for old and new values.
  void SetDropPosition(const DropPosition& position);

  // Invoked from SetDropPosition to update the visual indicator. |turn_on|
  // indicates whether the indicator is to be turned on or off.
  void UpdateDropIndicator(const DropPosition& position, bool turn_on);

  // Determines the drop position for the specified location.
  DropPosition CalculateDropPosition(int y);

  // Returns the BookmarkNode the drop should occur on.
  BookmarkNode* GetDropParentAndIndex(const DropPosition& position,
                                      int* index);

  // Returns the bounds of drop indicator shown when the drop is to occur
  // between rows (drop_on is false).
  RECT GetDropBetweenHighlightRect(int index);

  // Resets the columns. BookmarkTableView shows different sets of columns.
  // See ShowPathColumn for details.
  void UpdateColumns();

  // Draws the alt_text_. Does nothing if there is no alt_text_.
  void PaintAltText();

  // Returns the bounds of the alt text.
  gfx::Rect GetAltTextBounds();

  // Returns the font used for alt text.
  ChromeFont GetAltTextFont();

  Profile* profile_;

  BookmarkNode* parent_node_;

  scoped_ptr<DropInfo> drop_info_;

  bool show_path_column_;

  std::wstring alt_text_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkTableView);
};

#endif  // CHROME_BROWSER_VIEWS_BOOKMARK_TABLE_VIEW_H_
