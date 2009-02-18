// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BOOKMARK_BUBBLE_VIEW_H_
#define CHROME_BROWSER_VIEWS_BOOKMARK_BUBBLE_VIEW_H_

#include "base/gfx/rect.h"
#include "chrome/browser/views/info_bubble.h"
#include "chrome/views/combo_box.h"
#include "chrome/views/link.h"
#include "chrome/views/native_button.h"
#include "chrome/views/view.h"
#include "googleurl/src/gurl.h"

class Profile;

class BookmarkModel;
class BookmarkNode;

namespace views {
class CheckBox;
class TextField;
}

// BookmarkBubbleView is a view intended to be used as the content of an
// InfoBubble. BookmarkBubbleView provides views for unstarring and editting
// the bookmark it is created with. Don't create a BookmarkBubbleView directly,
// instead use the static Show method.
class BookmarkBubbleView : public views::View,
                           public views::LinkController,
                           public views::NativeButton::Listener,
                           public views::ComboBox::Listener,
                           public InfoBubbleDelegate {
 public:
   static void Show(HWND parent,
                    const gfx::Rect& bounds,
                    InfoBubbleDelegate* delegate,
                    Profile* profile,
                    const GURL& url,
                    bool newly_bookmarked);

  static bool IsShowing();

  static void Hide();

  virtual ~BookmarkBubbleView();

  // Overriden to force a layout.
  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);

  // Invoked after the bubble has been shown.
  virtual void BubbleShown();

  // Override to close on return.
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

 private:
  // Model for the combobox showing the list of folders to choose from. The
  // list always contains the bookmark bar, other node and parent. The list
  // also contains an extra item that shows the text 'Choose another folder...'.
  class RecentlyUsedFoldersModel : public views::ComboBox::Model {
   public:
    RecentlyUsedFoldersModel(BookmarkModel* bb_model, BookmarkNode* node);

    // ComboBox::Model methods. Call through to nodes_.
    virtual int GetItemCount(views::ComboBox* source);
    virtual std::wstring GetItemAt(views::ComboBox* source, int index);

    // Returns the node at the specified index.
    BookmarkNode* GetNodeAt(int index);

    // Returns the index of the original parent folder.
    int node_parent_index() const { return node_parent_index_; }

   private:
    // Removes node from nodes_. Does nothing if node is not in nodes_.
    void RemoveNode(BookmarkNode* node);

    std::vector<BookmarkNode*> nodes_;
    int node_parent_index_;

    DISALLOW_COPY_AND_ASSIGN(RecentlyUsedFoldersModel);
  };

  // Creates a BookmarkBubbleView.
  // |title| is the title of the page. If newly_bookmarked is false, title is
  // ignored and the title of the bookmark is fetched from the database.
  BookmarkBubbleView(InfoBubbleDelegate* delegate,
                     Profile* profile,
                     const GURL& url,
                     bool newly_bookmarked);
  // Creates the child views.
  void Init();

  // Returns the title to display.
  std::wstring GetTitle();

  // LinkController method, either unstars the item or shows the bookmark
  // editor (depending upon which link was clicked).
  virtual void LinkActivated(views::Link* source, int event_flags);

  // ButtonListener method, closes the bubble or opens the edit dialog.		
  virtual void ButtonPressed(views::NativeButton* sender);

  // ComboBox::Listener method. Changes the parent of the bookmark.
  virtual void ItemChanged(views::ComboBox* combo_box,
                           int prev_index,
                           int new_index);

  // InfoBubbleDelegate methods. These forward to the InfoBubbleDelegate
  // supplied in the constructor as well as sending out the necessary
  // notification.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape);
  virtual bool CloseOnEscape();

  // Closes the bubble.
  void Close();

  // Shows the BookmarkEditor.
  void ShowEditor();

  // Sets the title and parent of the node.
  void ApplyEdits();

  // The bookmark bubble, if we're showing one.
  static BookmarkBubbleView* bubble_;

  // Delegate for the bubble, may be null.
  InfoBubbleDelegate* delegate_;

  // The profile.
  Profile* profile_;

  // The bookmark URL.
  const GURL url_;

  // Title of the bookmark. This is initially the title supplied to the
  // constructor, which is typically the title of the page.
  std::wstring title_;

  // If true, the page was just bookmarked.
  const bool newly_bookmarked_;

  RecentlyUsedFoldersModel parent_model_;

  // Link for removing/unstarring the bookmark.
  views::Link* remove_link_;

  // Button to bring up the editor.
  views::NativeButton* edit_button_;

  // Button to close the window.
  views::NativeButton* close_button_;

  // TextField showing the title of the bookmark.
  views::TextField* title_tf_;

  // ComboBox showing a handful of folders the user can choose from, including
  // the current parent.
  views::ComboBox* parent_combobox_;

  // When the destructor is invoked should the bookmark be removed?
  bool remove_bookmark_;

  // When the destructor is invoked should edits be applied?
  bool apply_edits_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBubbleView);
};

#endif  // CHROME_BROWSER_VIEWS_BOOKMARK_BUBBLE_VIEW_H_
