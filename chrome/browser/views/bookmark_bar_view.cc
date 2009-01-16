// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/bookmark_bar_view.h"

#include <limits>

#include "base/base_drag_source.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/bookmarks/bookmark_context_menu.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/drag_utils.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/bookmark_editor_view.h"
#include "chrome/browser/views/event_utils.h"
#include "chrome/browser/views/input_window.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/favicon_size.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_types.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/chrome_menu.h"
#include "chrome/views/menu_button.h"
#include "chrome/views/tooltip_manager.h"
#include "chrome/views/view_constants.h"
#include "chrome/views/widget.h"
#include "chrome/views/window.h"
#include "generated_resources.h"
#include "skia/ext/skia_utils.h"

using views::BaseButton;
using views::DropTargetEvent;
using views::MenuButton;
using views::MenuItemView;
using views::View;

// How much we want the bookmark bar to overlap the toolbar when in its
// 'always shown' mode.
static const double kToolbarOverlap = 4.0;

// Margins around the content.
static const int kTopMargin = 1;
static const int kBottomMargin = 2;
static const int kLeftMargin = 1;
static const int kRightMargin = 1;

// Preferred height of the bookmarks bar.
static const int kBarHeight = 29;

// Preferred height of the bookmarks bar when only shown on the new tab page.
static const int kNewtabBarHeight = 57;

// How inset the bookmarks bar is when displayed on the new tab page. This is
// in addition to the margins above.
static const int kNewtabHorizontalPadding = 8;
static const int kNewtabVerticalPadding = 12;

// Padding between buttons.
static const int kButtonPadding = 0;

// Command ids used in the menu allowing the user to choose when we're visible.
static const int kAlwaysShowCommandID = 1;

// Whether the pref height has been calculated.
static bool calcedSize = false;

// The preferred height is the height needed for one button. As it is a constant
// it is calculated once.
static int prefButtonHeight = 0;

// Icon to display when one isn't found for the page.
static SkBitmap* kDefaultFavIcon = NULL;

// Icon used for folders.
static SkBitmap* kFolderIcon = NULL;

// Icon used for most other menu.
static SkBitmap* kBookmarkedOtherIcon = NULL;

// Background color.
static const SkColor kBackgroundColor = SkColorSetRGB(237, 244, 252);

// Border colors for the BookmarBarView.
static const SkColor kTopBorderColor = SkColorSetRGB(222, 234, 248);
static const SkColor kBottomBorderColor = SkColorSetRGB(178, 178, 178);

// Background color for when the bookmarks bar is only being displayed on the
// new tab page - this color should match the background color of the new tab
// page (white, most likely).
static const SkColor kNewtabBackgroundColor = SkColorSetRGB(255, 255, 255);

// Border color for the 'new tab' style bookmarks bar.
static const SkColor kNewtabBorderColor = SkColorSetRGB(195, 206, 224);

// How round the 'new tab' style bookmarks bar is.
static const int kNewtabBarRoundness = 5;

// Offset for where the menu is shown relative to the bottom of the
// BookmarkBarView.
static const int kMenuOffset = 3;

// Delay during drag and drop before the menu pops up. This is only used if
// we can't get the value from the OS.
static const int kShowFolderDropMenuDelay = 400;

// Color of the drop indicator.
static const SkColor kDropIndicatorColor = SK_ColorBLACK;

// Width of the drop indicator.
static const int kDropIndicatorWidth = 2;

// Distance between the bottom of the bar and the separator.
static const int kSeparatorMargin = 1;

// Width of the separator between the recently bookmarked button and the
// overflow indicator.
static const int kSeparatorWidth = 4;

// Starting x-coordinate of the separator line within a separator.
static const int kSeparatorStartX = 2;

// Border color along the left edge of the view representing the most recently
// view pages.
static const SkColor kSeparatorColor = SkColorSetRGB(194, 205, 212);

// Left-padding for the instructional text.
static const int kInstructionsPadding = 6;

// Color of the instructional text.
static const SkColor kInstructionsColor = SkColorSetRGB(128, 128, 142);

// Tag for the other button.
static const int kOtherFolderButtonTag = 1;

namespace {

// Returns the tooltip text for the specified url and title. The returned
// text is clipped to fit within the bounds of the monitor.
//
// Note that we adjust the direction of both the URL and the title based on the
// locale so that pure LTR strings are displayed properly in RTL locales.
static std::wstring CreateToolTipForURLAndTitle(const gfx::Point& screen_loc,
                                                const GURL& url,
                                                const std::wstring& title,
                                                const std::wstring& languages) {
  const gfx::Rect monitor_bounds = win_util::GetMonitorBoundsForRect(
      gfx::Rect(screen_loc.x(), screen_loc.y(), 1, 1));
  ChromeFont tt_font = views::TooltipManager::GetDefaultFont();
  std::wstring result;

  // First the title.
  if (!title.empty()) {
    std::wstring localized_title;
    if (l10n_util::AdjustStringForLocaleDirection(title, &localized_title))
      result.append(gfx::ElideText(localized_title,
                                   tt_font,
                                   monitor_bounds.width()));
    else
      result.append(gfx::ElideText(title, tt_font, monitor_bounds.width()));
  }

  // Only show the URL if the url and title differ.
  if (title != UTF8ToWide(url.spec())) {
    if (!result.empty())
      result.append(views::TooltipManager::GetLineSeparator());

    // We need to explicitly specify the directionality of the URL's text to
    // make sure it is treated as an LTR string when the context is RTL. For
    // example, the URL "http://www.yahoo.com/" appears as
    // "/http://www.yahoo.com" when rendered, as is, in an RTL context since
    // the Unicode BiDi algorithm puts certain characters on the left by
    // default.
    std::wstring elided_url(gfx::ElideUrl(url,
                                          tt_font,
                                          monitor_bounds.width(),
                                          languages));
    if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
      l10n_util::WrapStringWithLTRFormatting(&elided_url);
    result.append(elided_url);
  }
  return result;
}

// Returns the drag operations for the specified node.
static int GetDragOperationsForNode(BookmarkNode* node) {
  if (node->GetType() == history::StarredEntry::URL) {
    return DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_MOVE |
           DragDropTypes::DRAG_LINK;
  }
  return DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_MOVE;
}

// BookmarkButton -------------------------------------------------------------

// Buttons used for the bookmarks on the bookmark bar.

class BookmarkButton : public views::TextButton {
 public:
  BookmarkButton(const GURL& url,
                 const std::wstring& title,
                 Profile* profile)
      : TextButton(title),
        url_(url),
        profile_(profile) {
    show_animation_.reset(new SlideAnimation(this));
    if (BookmarkBarView::testing_) {
      // For some reason during testing the events generated by animating
      // throw off the test. So, don't animate while testing.
      show_animation_->Reset(1);
    } else {
      show_animation_->Show();
    }
  }

  bool GetTooltipText(int x, int y, std::wstring* tooltip) {
    gfx::Point location(x, y);
    ConvertPointToScreen(this, &location);
    *tooltip = CreateToolTipForURLAndTitle(
        gfx::Point(location.x(), location.y()), url_, GetText(),
        profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));
    return !tooltip->empty();
  }

  virtual bool IsTriggerableEvent(const views::MouseEvent& e) {
    return event_utils::IsPossibleDispositionEvent(e);
  }

  virtual void Paint(ChromeCanvas *canvas) {
    views::TextButton::Paint(canvas);

    PaintAnimation(this, canvas, show_animation_->GetCurrentValue());
  }

  static void PaintAnimation(views::View* view,
                             ChromeCanvas* canvas,
                             double animation_value) {
    // Since we can't change the alpha of the button (it contains un-alphable
    // text), we paint the bar background over the front of the button. As the
    // bar background is a gradient, we have to paint the gradient at the
    // size of the parent (hence all the margin math below). We can't use
    // the parent's actual bounds because they differ from what is painted.
    SkPaint paint;
    paint.setAlpha(static_cast<int>((1.0 - animation_value) * 255));
    paint.setShader(skia::CreateGradientShader(0,
        view->height() + kTopMargin + kBottomMargin,
        kTopBorderColor,
        kBackgroundColor))->safeUnref();
    canvas->FillRectInt(0, -kTopMargin, view->width(),
                        view->height() + kTopMargin + kBottomMargin, paint);
  }

 private:
  const GURL& url_;
  Profile* profile_;
  scoped_ptr<SlideAnimation> show_animation_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkButton);
};

// BookmarkFolderButton -------------------------------------------------------

// Buttons used for folders on the bookmark bar, including the 'other folders'
// button.
class BookmarkFolderButton : public views::MenuButton {
 public:
  BookmarkFolderButton(const std::wstring& title,
                       views::ViewMenuDelegate* menu_delegate,
                       bool show_menu_marker)
      : MenuButton(title, menu_delegate, show_menu_marker) {
    show_animation_.reset(new SlideAnimation(this));
    if (BookmarkBarView::testing_) {
      // For some reason during testing the events generated by animating
      // throw off the test. So, don't animate while testing.
      show_animation_->Reset(1);
    } else {
      show_animation_->Show();
    }
  }

  virtual bool IsTriggerableEvent(const views::MouseEvent& e) {
    // This is hard coded to avoid potential notification on left mouse down,
    // which we want to show the menu.
    return e.IsMiddleMouseButton();
  }

  virtual void Paint(ChromeCanvas *canvas) {
    views::MenuButton::Paint(canvas, false);

    BookmarkButton::PaintAnimation(this, canvas,
                                   show_animation_->GetCurrentValue());
  }

 private:
  scoped_ptr<SlideAnimation> show_animation_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkFolderButton);
};

// DropInfo -------------------------------------------------------------------

// Tracks drops on the BookmarkBarView.

struct DropInfo {
  DropInfo() : drop_index(-1), is_menu_showing(false), valid(false) {}

  // Whether the data is valid.
  bool valid;

  // Index into the model the drop is over. This is relative to the root node.
  int drop_index;

  // If true, the menu is being shown.
  bool is_menu_showing;

  // If true, the user is dropping on a node. This is only used for group
  // nodes.
  bool drop_on;

  // If true, the user is over the overflow button.
  bool is_over_overflow;

  // If true, the user is over the other button.
  bool is_over_other;

  // Coordinates of the drag (in terms of the BookmarkBarView).
  int x;
  int y;

  // The current drag operation.
  int drag_operation;

  // DropData for the drop.
  BookmarkDragData data;
};

// MenuRunner -----------------------------------------------------------------

// MenuRunner manages creation and showing of a menu containing BookmarkNodes.
// MenuRunner is used to show the contents of bookmark folders on the
// bookmark bar, other folder, or overflow bookmarks.
//
class MenuRunner : public views::MenuDelegate,
                   public BookmarkBarView::ModelChangedListener {
 public:
  // start_child_index is the index of the first child in node to add to the
  // menu.
  MenuRunner(BookmarkBarView* view, BookmarkNode* node, int start_child_index)
      : view_(view),
        node_(node),
        menu_(this) {
    int next_menu_id = 1;
    menu_id_to_node_map_[menu_.GetCommand()] = node;
    menu_.set_has_icons(true);
    BuildMenu(node, start_child_index, &menu_, &next_menu_id);
  }

  // Returns the node the menu is being run for.
  BookmarkNode* GetNode() {
    return node_;
  }

  void RunMenuAt(HWND hwnd,
                 const gfx::Rect& bounds,
                 MenuItemView::AnchorPosition position,
                 bool for_drop) {
    view_->SetModelChangedListener(this);
    if (for_drop)
      menu_.RunMenuForDropAt(hwnd, bounds, position);
    else
      menu_.RunMenuAt(hwnd, bounds, position, false);
    view_->ClearModelChangedListenerIfEquals(this);
  }

  // Notification that the favicon has finished loading. Reset the icon
  // of the menu item.
  void FavIconLoaded(BookmarkNode* node) {
    if (node_to_menu_id_map_.find(node) !=
        node_to_menu_id_map_.end()) {
      menu_.SetIcon(node->GetFavIcon(), node_to_menu_id_map_[node]);
    }
  }

  virtual void ModelChanged() {
    menu_.Cancel();
  }

  MenuItemView* menu() { return &menu_; }

  MenuItemView* context_menu() {
    return context_menu_.get() ? context_menu_->menu() : NULL;
  }

 private:
  // Creates an entry in menu for each child node of parent starting at
  // start_child_index, recursively invoking this for any star groups.
  void BuildMenu(BookmarkNode* parent,
                 int start_child_index,
                 MenuItemView* menu,
                 int* next_menu_id) {
    DCHECK(!parent->GetChildCount() ||

           start_child_index < parent->GetChildCount());
    for (int i = start_child_index; i < parent->GetChildCount(); ++i) {
      BookmarkNode* node = parent->GetChild(i);
      int id = *next_menu_id;

      (*next_menu_id)++;
      if (node->GetType() == history::StarredEntry::URL) {
        SkBitmap icon = node->GetFavIcon();
        if (icon.width() == 0)
          icon = *kDefaultFavIcon;
        menu->AppendMenuItemWithIcon(id, node->GetTitle(), icon);
        node_to_menu_id_map_[node] = id;
      } else {
        SkBitmap* folder_icon =
            ResourceBundle::GetSharedInstance().GetBitmapNamed(
                IDR_BOOKMARK_BAR_FOLDER);
        MenuItemView* submenu = menu->AppendSubMenuWithIcon(
            id, node->GetTitle(), *folder_icon);
        BuildMenu(node, 0, submenu, next_menu_id);
      }
      menu_id_to_node_map_[id] = node;
    }
  }

  // ViewMenuDelegate method. Overridden to forward to the PageNavigator so
  // that we accept any events that may trigger opening a url.
  virtual bool IsTriggerableEvent(const views::MouseEvent& e) {
    return event_utils::IsPossibleDispositionEvent(e);
  }

  // Invoked when a menu item is selected. Uses the PageNavigator set on
  // the BookmarkBarView to open the URL.
  virtual void ExecuteCommand(int id, int mouse_event_flags) {
    DCHECK(view_->GetPageNavigator());
    GURL url;
    DCHECK(menu_id_to_node_map_.find(id) != menu_id_to_node_map_.end());
    url = menu_id_to_node_map_[id]->GetURL();
    view_->GetPageNavigator()->OpenURL(
        url, GURL(), event_utils::DispositionFromEventFlags(mouse_event_flags),
        PageTransition::AUTO_BOOKMARK);
  }

  virtual bool CanDrop(MenuItemView* menu, const OSExchangeData& data) {
    // Only accept drops of 1 node, which is the case for all data dragged from
    // bookmark bar and menus.
    if (!drop_data_.Read(data) || drop_data_.elements.size() != 1)
      return false;

    if (drop_data_.has_single_url())
      return true;

    BookmarkNode* drag_node = drop_data_.GetFirstNode(view_->GetProfile());
    if (!drag_node) {
      // Dragging a group from another profile, always accept.
      return true;
    }
    // Drag originated from same profile and is not a URL. Only accept it if
    // the dragged node is not a parent of the node menu represents.
    BookmarkNode* drop_node = menu_id_to_node_map_[menu->GetCommand()];
    DCHECK(drop_node);
    BookmarkNode* node = drop_node;
    while (drop_node && drop_node != drag_node)
      drop_node = drop_node->GetParent();
    return (drop_node == NULL);
  }

  virtual int GetDropOperation(MenuItemView* item,
                               const views::DropTargetEvent& event,
                               DropPosition* position) {
    DCHECK(drop_data_.is_valid());
    BookmarkNode* node = menu_id_to_node_map_[item->GetCommand()];
    BookmarkNode* drop_parent = node->GetParent();
    int index_to_drop_at = drop_parent->IndexOfChild(node);
    if (*position == DROP_AFTER) {
      index_to_drop_at++;
    } else if (*position == DROP_ON) {
      drop_parent = node;
      index_to_drop_at = node->GetChildCount();
    }
    DCHECK(drop_parent);
    return view_->CalculateDropOperation(event, drop_data_, drop_parent,
                                         index_to_drop_at);
  }

  virtual int OnPerformDrop(MenuItemView* menu,
                            DropPosition position,
                            const DropTargetEvent& event) {
    BookmarkNode* drop_node = menu_id_to_node_map_[menu->GetCommand()];
    DCHECK(drop_node);
    BookmarkModel* model = view_->GetModel();
    DCHECK(model);
    BookmarkNode* drop_parent = drop_node->GetParent();
    DCHECK(drop_parent);
    int index_to_drop_at = drop_parent->IndexOfChild(drop_node);
    if (position == DROP_AFTER) {
      index_to_drop_at++;
    } else if (position == DROP_ON) {
      DCHECK(drop_node->GetType() != history::StarredEntry::URL);
      drop_parent = drop_node;
      index_to_drop_at = drop_node->GetChildCount();
    }

    const int result = view_->PerformDropImpl(drop_data_, drop_parent,
                                              index_to_drop_at);
    if (view_->drop_menu_runner_.get() == this)
      view_->drop_menu_runner_.reset();
    // WARNING: we've been deleted!
    return result;
  }

  virtual bool ShowContextMenu(MenuItemView* source,
                               int id,
                               int x,
                               int y,
                               bool is_mouse_gesture) {
    DCHECK(menu_id_to_node_map_.find(id) != menu_id_to_node_map_.end());
    std::vector<BookmarkNode*> nodes;
    nodes.push_back(menu_id_to_node_map_[id]);
    context_menu_.reset(
        new BookmarkContextMenu(view_->GetWidget()->GetHWND(),
                                view_->GetProfile(),
                                view_->browser(),
                                view_->GetPageNavigator(),
                                nodes[0]->GetParent(),
                                nodes,
                                BookmarkContextMenu::BOOKMARK_BAR));
    context_menu_->RunMenuAt(x, y);
    context_menu_.reset(NULL);
    return true;
  }

  virtual void DropMenuClosed(MenuItemView* menu) {
    if (view_->drop_menu_runner_.get() == this)
      view_->drop_menu_runner_.reset();
  }

  virtual bool CanDrag(MenuItemView* menu) {
    DCHECK(menu);
    return true;
  }

  virtual void WriteDragData(MenuItemView* sender, OSExchangeData* data) {
    DCHECK(sender && data);

    UserMetrics::RecordAction(L"BookmarkBar_DragFromFolder",
                              view_->GetProfile());

    view_->WriteDragData(menu_id_to_node_map_[sender->GetCommand()], data);
  }

  virtual int GetDragOperations(MenuItemView* sender) {
    return GetDragOperationsForNode(
        menu_id_to_node_map_[sender->GetCommand()]);
  }

  // The node we're showing the contents of.
  BookmarkNode* node_;

  // The view that created us.
  BookmarkBarView* view_;

  // The menu.
  MenuItemView menu_;

  // Mapping from menu id to the BookmarkNode.
  std::map<int, BookmarkNode*> menu_id_to_node_map_;

  // Mapping from node to menu id. This only contains entries for nodes of type
  // URL.
  std::map<BookmarkNode*, int> node_to_menu_id_map_;

  // Data for the drop.
  BookmarkDragData drop_data_;

  scoped_ptr<BookmarkContextMenu> context_menu_;

  DISALLOW_COPY_AND_ASSIGN(MenuRunner);
};

// ButtonSeparatorView  --------------------------------------------------------

// TODO(sky/glen): this is temporary, need to decide on what this should
// look like.
class ButtonSeparatorView : public views::View {
 public:
  ButtonSeparatorView() {}
  virtual ~ButtonSeparatorView() {}

  virtual void Paint(ChromeCanvas* canvas) {
    SkPaint paint;
    paint.setShader(skia::CreateGradientShader(0,
                                               height() / 2,
                                               kTopBorderColor,
                                               kSeparatorColor))->safeUnref();
    SkRect rc = {SkIntToScalar(kSeparatorStartX),  SkIntToScalar(0),
                 SkIntToScalar(1), SkIntToScalar(height() / 2) };
    canvas->drawRect(rc, paint);

    SkPaint paint_down;
    paint_down.setShader(skia::CreateGradientShader(height() / 2,
        height(),
        kSeparatorColor,
        kBackgroundColor))->safeUnref();
    SkRect rc_down = {
        SkIntToScalar(kSeparatorStartX),  SkIntToScalar(height() / 2),
        SkIntToScalar(1), SkIntToScalar(height() - 1) };
    canvas->drawRect(rc_down, paint_down);
  }

  virtual gfx::Size GetPreferredSize() {
    // We get the full height of the bookmark bar, so that the height returned
    // here doesn't matter.
    return gfx::Size(kSeparatorWidth, 1);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ButtonSeparatorView);
};

}  // namespace

// BookmarkBarView ------------------------------------------------------------

// static
const int BookmarkBarView::kMaxButtonWidth = 150;

// static
bool BookmarkBarView::testing_ = false;

// Returns the bitmap to use for starred groups.
static const SkBitmap& GetGroupIcon() {
  if (!kFolderIcon) {
    kFolderIcon = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_BOOKMARK_BAR_FOLDER);
  }
  return *kFolderIcon;
}

// static
void BookmarkBarView::ToggleWhenVisible(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  const bool always_show = !prefs->GetBoolean(prefs::kShowBookmarkBar);

  // The user changed when the bookmark bar is shown, update the preferences.
  prefs->SetBoolean(prefs::kShowBookmarkBar, always_show);
  prefs->ScheduleSavePersistentPrefs(g_browser_process->file_thread());

  // And notify the notification service.
  Source<Profile> source(profile);
  NotificationService::current()->Notify(
      NOTIFY_BOOKMARK_BAR_VISIBILITY_PREF_CHANGED, source,
      NotificationService::NoDetails());
}

// static
void BookmarkBarView::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kShowBookmarkBar, false);
}

BookmarkBarView::BookmarkBarView(Profile* profile, Browser* browser)
    : profile_(NULL),
      browser_(browser),
      page_navigator_(NULL),
      model_(NULL),
      other_bookmarked_button_(NULL),
      model_changed_listener_(NULL),
      show_folder_drop_menu_task_(NULL),
      overflow_button_(NULL),
      instructions_(NULL),
      bookmarks_separator_view_(NULL),
      throbbing_view_(NULL) {
  SetID(VIEW_ID_BOOKMARK_BAR);
  Init();
  SetProfile(profile);


  if (IsAlwaysShown()) {
    size_animation_->Reset(1);
  } else {
    size_animation_->Reset(0);
  }
}

BookmarkBarView::~BookmarkBarView() {
  NotifyModelChanged();
  RemoveNotificationObservers();
  if (model_)
    model_->RemoveObserver(this);
  StopShowFolderDropMenuTimer();
}

void BookmarkBarView::SetProfile(Profile* profile) {
  DCHECK(profile);
  if (profile_ == profile)
    return;

  StopThrobbing(true);

  // Cancels the current cancelable.
  NotifyModelChanged();

  // Remove the current buttons.
  for (int i = GetBookmarkButtonCount() - 1; i >= 0; --i) {
    View* child = GetChildViewAt(i);
    RemoveChildView(child);
    delete child;
  }

  profile_ = profile;

  if (model_)
    model_->RemoveObserver(this);

  // Disable the other bookmarked button, we'll re-enable when the model is
  // loaded.
  other_bookmarked_button_->SetEnabled(false);

  NotificationService* ns = NotificationService::current();
  Source<Profile> ns_source(profile_->GetOriginalProfile());
  ns->AddObserver(this, NOTIFY_BOOKMARK_BUBBLE_SHOWN, ns_source);
  ns->AddObserver(this, NOTIFY_BOOKMARK_BUBBLE_HIDDEN, ns_source);
  ns->AddObserver(this, NOTIFY_BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
                  NotificationService::AllSources());

  model_ = profile_->GetBookmarkModel();
  model_->AddObserver(this);
  if (model_->IsLoaded())
    Loaded(model_);
  // else case: we'll receive notification back from the BookmarkModel when done
  // loading, then we'll populate the bar.
}

void BookmarkBarView::SetPageNavigator(PageNavigator* navigator) {
  page_navigator_ = navigator;
}

gfx::Size BookmarkBarView::GetPreferredSize() {
  if (!prefButtonHeight) {
    views::TextButton text_button(L"X");
    gfx::Size text_button_pref = text_button.GetMinimumSize();
    prefButtonHeight = static_cast<int>(text_button_pref.height());
  }

  gfx::Size prefsize;
  if (OnNewTabPage()) {
    prefsize.set_height(kBarHeight + static_cast<int>(static_cast<double>
                        (kNewtabBarHeight - kBarHeight) *
                        (1 - size_animation_->GetCurrentValue())));
  } else {
    prefsize.set_height(
        std::max(static_cast<int>(static_cast<double>(kBarHeight) *
                 size_animation_->GetCurrentValue()), 1));
  }

  // Width doesn't matter, we're always given a width based on the browser
  // size.
  prefsize.set_width(1);

  return prefsize;
}

void BookmarkBarView::Layout() {
  if (!GetParent())
    return;

  // First layout out the buttons. Any buttons that are placed beyond the
  // visible region and made invisible.
  int x = kLeftMargin;
  int y = kTopMargin;
  int width = View::width() - kRightMargin - kLeftMargin;
  int height = View::height() - kTopMargin - kBottomMargin;
  int separator_margin = kSeparatorMargin;

  if (OnNewTabPage()) {
    double current_state = 1 - size_animation_->GetCurrentValue();
    x += static_cast<int>(static_cast<double>
        (kNewtabHorizontalPadding) * current_state);
    y += static_cast<int>(static_cast<double>
        (kNewtabVerticalPadding) * current_state);
    width -= static_cast<int>(static_cast<double>
        (kNewtabHorizontalPadding) * current_state);
    height -= static_cast<int>(static_cast<double>
        (kNewtabVerticalPadding * 2) * current_state);
    separator_margin -= static_cast<int>(static_cast<double>
        (kSeparatorMargin) * current_state);
  }

  gfx::Size other_bookmarked_pref =
      other_bookmarked_button_->GetPreferredSize();
  gfx::Size overflow_pref = overflow_button_->GetPreferredSize();
  gfx::Size bookmarks_separator_pref =
      bookmarks_separator_view_->GetPreferredSize();
  const int max_x = width - other_bookmarked_pref.width() - kButtonPadding -
              overflow_pref.width() - kButtonPadding -
              bookmarks_separator_pref.width();

  if (GetBookmarkButtonCount() == 0 && model_ && model_->IsLoaded()) {
    gfx::Size pref = instructions_->GetPreferredSize();
    instructions_->SetBounds(
        x + kInstructionsPadding, y,
        std::min(static_cast<int>(pref.width()),
        max_x - x),
        height);
    instructions_->SetVisible(true);
  } else {
    instructions_->SetVisible(false);

    for (int i = 0; i < GetBookmarkButtonCount(); ++i) {
      views::View* child = GetChildViewAt(i);
      gfx::Size pref = child->GetPreferredSize();
      int next_x = x + pref.width() + kButtonPadding;
      child->SetVisible(next_x < max_x);
      child->SetBounds(x, y, pref.width(), height);
      x = next_x;
    }
  }

  // Layout the right side of the bar.
  const bool all_visible =
      (GetBookmarkButtonCount() == 0 ||
       GetChildViewAt(GetBookmarkButtonCount() - 1)->IsVisible());

  // Layout the right side buttons.
  x = max_x + kButtonPadding;

  // The overflow button.
  overflow_button_->SetBounds(x, y, overflow_pref.width(), height);
  overflow_button_->SetVisible(!all_visible);

  x += overflow_pref.width();

  // Separator.
  bookmarks_separator_view_->SetBounds(x,
                                       y - kTopMargin,
                                       bookmarks_separator_pref.width(),
                                       height + kTopMargin + kBottomMargin -
                                       separator_margin);
  x += bookmarks_separator_pref.width();

  // The other bookmarks button.
  other_bookmarked_button_->SetBounds(x, y, other_bookmarked_pref.width(),
                                      height);
  x += other_bookmarked_pref.width() + kButtonPadding;
}

void BookmarkBarView::DidChangeBounds(const gfx::Rect& previous,
                                      const gfx::Rect& current) {
  Layout();
}

void BookmarkBarView::ViewHierarchyChanged(bool is_add,
                                           View* parent, View* child) {
  if (is_add && child == this && height() > 0) {
    // We only layout while parented. When we become parented, if our bounds
    // haven't changed, DidChangeBounds won't get invoked and we won't layout.
    // Therefore we always force a layout when added.
    Layout();
  }
}

void BookmarkBarView::Paint(ChromeCanvas* canvas) {
  if (OnNewTabPage() && (!IsAlwaysShown() || size_animation_->IsAnimating())) {
    // Draw the background to match the new tab page.
    canvas->FillRectInt(kNewtabBackgroundColor, 0, 0, width(), height());

    // Draw the 'bottom' of the toolbar above our bubble.
    canvas->FillRectInt(kBottomBorderColor, 0, 0, width(), 1);

    SkRect rect;

    // As 'hidden' according to the animation is the full in-tab state,
    // we invert the value - when current_state is at '0', we expect the
    // bar to be docked.
    double current_state = 1 - size_animation_->GetCurrentValue();

    // The 0.5 is to correct for Skia's "draw on pixel boundaries"ness.
    double h_padding = static_cast<double>
        (kNewtabHorizontalPadding) * current_state;
    double v_padding = static_cast<double>
        (kNewtabVerticalPadding) * current_state;
    rect.set(SkDoubleToScalar(h_padding - 0.5),
             SkDoubleToScalar(v_padding - 0.5),
             SkDoubleToScalar(width() - h_padding - 0.5),
             SkDoubleToScalar(height() - v_padding - 0.5));

    double roundness = static_cast<double>
        (kNewtabBarRoundness) * current_state;

    // Draw our background.
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setShader(skia::CreateGradientShader(0,
                                               height(),
                                               kTopBorderColor,
                                               kBackgroundColor))->safeUnref();

    canvas->drawRoundRect(rect,
                          SkDoubleToScalar(roundness),
                          SkDoubleToScalar(roundness), paint);

    // Draw border
    SkPaint border_paint;
    border_paint.setColor(kNewtabBorderColor);
    border_paint.setStyle(SkPaint::kStroke_Style);
    border_paint.setAntiAlias(true);

    canvas->drawRoundRect(rect,
                          SkDoubleToScalar(roundness),
                          SkDoubleToScalar(roundness), border_paint);
  } else {
    SkPaint paint;
    paint.setShader(skia::CreateGradientShader(0,
                                               height(),
                                               kTopBorderColor,
                                               kBackgroundColor))->safeUnref();
    canvas->FillRectInt(0, 0, width(), height(), paint);

    canvas->FillRectInt(kTopBorderColor, 0, 0, width(), 1);
    canvas->FillRectInt(kBottomBorderColor, 0, height() - 1, width(), 1);
  }
}

void BookmarkBarView::PaintChildren(ChromeCanvas* canvas) {
  View::PaintChildren(canvas);

  if (drop_info_.get() && drop_info_->valid &&
      drop_info_->drag_operation != 0 && drop_info_->drop_index != -1 &&
      !drop_info_->is_over_overflow && !drop_info_->drop_on) {
    int index = drop_info_->drop_index;
    DCHECK(index <= GetBookmarkButtonCount());
    int x = 0;
    int y = 0;
    int h = height();
    if (index == GetBookmarkButtonCount()) {
      if (index == 0) {
        x = kLeftMargin;
      } else {
        x = GetBookmarkButton(index - 1)->x() +
            GetBookmarkButton(index - 1)->width();
      }
    } else {
      x = GetBookmarkButton(index)->x();
    }
    if (GetBookmarkButtonCount() > 0 && GetBookmarkButton(0)->IsVisible()) {
      y = GetBookmarkButton(0)->y();
      h = GetBookmarkButton(0)->height();
    }

    // Since the drop indicator is painted directly onto the canvas, we must
    // make sure it is painted in the right location if the locale is RTL.
    gfx::Rect indicator_bounds(x - kDropIndicatorWidth / 2,
                               y,
                               kDropIndicatorWidth,
                               h);
    indicator_bounds.set_x(MirroredLeftPointForRect(indicator_bounds));

    // TODO(sky/glen): make me pretty!
    canvas->FillRectInt(kDropIndicatorColor, indicator_bounds.x(),
                        indicator_bounds.y(), indicator_bounds.width(),
                        indicator_bounds.height());
  }
}

bool BookmarkBarView::CanDrop(const OSExchangeData& data) {
  if (!model_ || !model_->IsLoaded())
    return false;

  if (!drop_info_.get())
    drop_info_.reset(new DropInfo());

  // Only accept drops of 1 node, which is the case for all data dragged from
  // bookmark bar and menus.
  return drop_info_->data.Read(data) && drop_info_->data.size() == 1;
}

void BookmarkBarView::OnDragEntered(const DropTargetEvent& event) {
}

int BookmarkBarView::OnDragUpdated(const DropTargetEvent& event) {
  if (!drop_info_.get())
    return 0;

  if (drop_info_->valid &&
      (drop_info_->x == event.x() && drop_info_->y == event.y())) {
    // The location of the mouse didn't change, return the last operation.
    return drop_info_->drag_operation;
  }

  drop_info_->x = event.x();
  drop_info_->y = event.y();

  int drop_index;
  bool drop_on;
  bool is_over_overflow;
  bool is_over_other;

  drop_info_->drag_operation = CalculateDropOperation(
      event, drop_info_->data, &drop_index, &drop_on, &is_over_overflow,
      &is_over_other);

  if (drop_info_->valid && drop_info_->drop_index == drop_index &&
      drop_info_->drop_on == drop_on &&
      drop_info_->is_over_overflow == is_over_overflow &&
      drop_info_->is_over_other == is_over_other) {
    // The position we're going to drop didn't change, return the last drag
    // operation we calculated.
    return drop_info_->drag_operation;
  }

  drop_info_->valid = true;

  StopShowFolderDropMenuTimer();

  // TODO(sky): Optimize paint region.
  SchedulePaint();
  drop_info_->drop_index = drop_index;
  drop_info_->drop_on = drop_on;
  drop_info_->is_over_overflow = is_over_overflow;
  drop_info_->is_over_other = is_over_other;

  if (drop_info_->is_menu_showing) {
    drop_menu_runner_.reset();
    drop_info_->is_menu_showing = false;
  }

  if (drop_on || is_over_overflow || is_over_other) {
    BookmarkNode* node;
    if (is_over_other)
      node = model_->other_node();
    else if (is_over_overflow)
      node = model_->GetBookmarkBarNode();
    else
      node = model_->GetBookmarkBarNode()->GetChild(drop_index);
    StartShowFolderDropMenuTimer(node);
  }

  return drop_info_->drag_operation;
}

void BookmarkBarView::OnDragExited() {
  StopShowFolderDropMenuTimer();

  // NOTE: we don't hide the menu on exit as it's possible the user moved the
  // mouse over the menu, which triggers an exit on us.

  drop_info_->valid = false;

  if (drop_info_->drop_index != -1) {
    // TODO(sky): optimize the paint region.
    SchedulePaint();
  }
  drop_info_.reset();
}

int BookmarkBarView::OnPerformDrop(const DropTargetEvent& event) {
  StopShowFolderDropMenuTimer();

  drop_menu_runner_.reset();

  if (!drop_info_.get() || !drop_info_->drag_operation)
    return DragDropTypes::DRAG_NONE;

  BookmarkNode* root = drop_info_->is_over_other ? model_->other_node() :
                                                   model_->GetBookmarkBarNode();
  int index = drop_info_->drop_index;
  const bool drop_on = drop_info_->drop_on;
  const BookmarkDragData data = drop_info_->data;
  const bool is_over_other = drop_info_->is_over_other;
  DCHECK(data.is_valid());

  if (drop_info_->drop_index != -1) {
    // TODO(sky): optimize the SchedulePaint region.
    SchedulePaint();
  }
  drop_info_.reset();

  BookmarkNode* parent_node;
  if (is_over_other) {
    parent_node = root;
    index = parent_node->GetChildCount();
  } else if (drop_on) {
    parent_node = root->GetChild(index);
    index = parent_node->GetChildCount();
  } else {
    parent_node = root;
  }
  return PerformDropImpl(data, parent_node, index);
}

bool BookmarkBarView::IsAlwaysShown() {
  return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

bool BookmarkBarView::OnNewTabPage() {
  return (browser_ && browser_->GetSelectedTabContents() &&
          browser_->GetSelectedTabContents()->IsBookmarkBarAlwaysVisible());
}

int BookmarkBarView::GetToolbarOverlap() {
  return static_cast<int>(size_animation_->GetCurrentValue() * kToolbarOverlap);
}

void BookmarkBarView::AnimationProgressed(const Animation* animation) {
  if (browser_)
    browser_->ToolbarSizeChanged(NULL, true);
}

void BookmarkBarView::AnimationEnded(const Animation* animation) {
  if (browser_)
    browser_->ToolbarSizeChanged(NULL, false);
  SchedulePaint();
}

views::TextButton* BookmarkBarView::GetBookmarkButton(int index) {
  DCHECK(index >= 0 && index < GetBookmarkButtonCount());
  return static_cast<views::TextButton*>(GetChildViewAt(index));
}

views::MenuItemView* BookmarkBarView::GetMenu() {
  return menu_runner_.get() ? menu_runner_->menu() : NULL;
}

views::MenuItemView* BookmarkBarView::GetContextMenu() {
  return menu_runner_.get() ? menu_runner_->context_menu() : NULL;
}

views::MenuItemView* BookmarkBarView::GetDropMenu() {
  return drop_menu_runner_.get() ? drop_menu_runner_->menu() : NULL;
}

void BookmarkBarView::Init() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  if (!calcedSize)
    calcedSize = false;

  if (!kDefaultFavIcon)
    kDefaultFavIcon = rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);

  other_bookmarked_button_ = CreateOtherBookmarkedButton();
  AddChildView(other_bookmarked_button_);

  overflow_button_ = CreateOverflowButton();
  AddChildView(overflow_button_);

  bookmarks_separator_view_ = new ButtonSeparatorView();
  AddChildView(bookmarks_separator_view_);

  instructions_ = new views::Label(
      l10n_util::GetString(IDS_BOOKMARKS_NO_ITEMS),
      rb.GetFont(ResourceBundle::BaseFont));
  instructions_->SetColor(kInstructionsColor);
  AddChildView(instructions_);

  SetContextMenuController(this);

  size_animation_.reset(new SlideAnimation(this));
}

MenuButton* BookmarkBarView::CreateOtherBookmarkedButton() {
  MenuButton* button = new BookmarkFolderButton(
      l10n_util::GetString(IDS_BOOMARK_BAR_OTHER_BOOKMARKED), this, false);
  button->SetIcon(GetGroupIcon());
  button->SetContextMenuController(this);
  button->SetListener(this, kOtherFolderButtonTag);
  return button;
}

MenuButton* BookmarkBarView::CreateOverflowButton() {
  MenuButton* button = new MenuButton(std::wstring(), this, false);
  button->SetIcon(*ResourceBundle::GetSharedInstance().
                  GetBitmapNamed(IDR_BOOKMARK_BAR_CHEVRONS));

  // The overflow button's image contains an arrow and therefore it is a
  // direction sensitive image and we need to flip it if the UI layout is
  // right-to-left.
  //
  // By default, menu buttons are not flipped because they generally contain
  // text and flipping the ChromeCanvas object will break text rendering. Since
  // the overflow button does not contain text, we can safely flip it.
  button->EnableCanvasFlippingForRTLUI(true);

  // Make visible as necessary.
  button->SetVisible(false);
  return button;
}

int BookmarkBarView::GetBookmarkButtonCount() {
  // We contain four non-bookmark button views: recently bookmarked,
  // bookmarks separator, chevrons (for overflow) and the instruction
  // label.
  return GetChildViewCount() - 4;
}

void BookmarkBarView::Loaded(BookmarkModel* model) {
  BookmarkNode* node = model_->GetBookmarkBarNode();
  DCHECK(node && model_->other_node());
  // Create a button for each of the children on the bookmark bar.
  for (int i = 0; i < node->GetChildCount(); ++i)
    AddChildView(i, CreateBookmarkButton(node->GetChild(i)));
  other_bookmarked_button_->SetEnabled(true);
  Layout();
  SchedulePaint();
}

void BookmarkBarView::BookmarkModelBeingDeleted(BookmarkModel* model) {
  // The bookmark model should never be deleted before us. This code exists
  // to check for regressions in shutdown code and not crash.
  NOTREACHED();

  // Do minimal cleanup, presumably we'll be deleted shortly.
  NotifyModelChanged();
  model_->RemoveObserver(this);
  model_ = NULL;
}

void BookmarkBarView::BookmarkNodeMoved(BookmarkModel* model,
                                        BookmarkNode* old_parent,
                                        int old_index,
                                        BookmarkNode* new_parent,
                                        int new_index) {
  StopThrobbing(true);
  BookmarkNodeRemovedImpl(model, old_parent, old_index);
  BookmarkNodeAddedImpl(model, new_parent, new_index);
  StartThrobbing();
}

void BookmarkBarView::BookmarkNodeAdded(BookmarkModel* model,
                                        BookmarkNode* parent,
                                        int index) {
  StopThrobbing(true);
  BookmarkNodeAddedImpl(model, parent, index);
  StartThrobbing();
}

void BookmarkBarView::BookmarkNodeAddedImpl(BookmarkModel* model,
                                            BookmarkNode* parent,
                                            int index) {
  NotifyModelChanged();
  if (parent != model_->GetBookmarkBarNode()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  DCHECK(index >= 0 && index <= GetBookmarkButtonCount());
  AddChildView(index, CreateBookmarkButton(parent->GetChild(index)));
  Layout();
  SchedulePaint();
}

void BookmarkBarView::BookmarkNodeRemoved(BookmarkModel* model,
                                          BookmarkNode* parent,
                                          int index) {
  StopThrobbing(true);
  BookmarkNodeRemovedImpl(model, parent, index);
  StartThrobbing();
}

void BookmarkBarView::BookmarkNodeRemovedImpl(BookmarkModel* model,
                                              BookmarkNode* parent,
                                              int index) {
  NotifyModelChanged();
  if (parent != model_->GetBookmarkBarNode()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  DCHECK(index >= 0 && index < GetBookmarkButtonCount());
  views::View* button = GetChildViewAt(index);
  RemoveChildView(button);
  MessageLoop::current()->DeleteSoon(FROM_HERE, button);
  Layout();
  SchedulePaint();
}

void BookmarkBarView::BookmarkNodeChanged(BookmarkModel* model,
                                          BookmarkNode* node) {
  NotifyModelChanged();
  BookmarkNodeChangedImpl(model, node);
}

void BookmarkBarView::BookmarkNodeChangedImpl(BookmarkModel* model,
                                              BookmarkNode* node) {
  if (node->GetParent() != model_->GetBookmarkBarNode()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  int index = model_->GetBookmarkBarNode()->IndexOfChild(node);
  DCHECK(index != -1);
  views::TextButton* button = GetBookmarkButton(index);
  gfx::Size old_pref = button->GetPreferredSize();
  ConfigureButton(node, button);
  gfx::Size new_pref = button->GetPreferredSize();
  if (old_pref.width() != new_pref.width()) {
    Layout();
    SchedulePaint();
  } else if (button->IsVisible()) {
    button->SchedulePaint();
  }
}

void BookmarkBarView::BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                                BookmarkNode* node) {
  if (menu_runner_.get())
    menu_runner_->FavIconLoaded(node);
  if (drop_menu_runner_.get())
    drop_menu_runner_->FavIconLoaded(node);
  BookmarkNodeChangedImpl(model, node);
}

void BookmarkBarView::WriteDragData(View* sender,
                                    int press_x,
                                    int press_y,
                                    OSExchangeData* data) {
  UserMetrics::RecordAction(L"BookmarkBar_DragButton", profile_);

  for (int i = 0; i < GetBookmarkButtonCount(); ++i) {
    if (sender == GetBookmarkButton(i)) {
      views::TextButton* button = GetBookmarkButton(i);
      ChromeCanvas canvas(button->width(), button->height(), false);
      button->Paint(&canvas, true);
      drag_utils::SetDragImageOnDataObject(canvas, button->width(),
                                           button->height(), press_x,
                                           press_y, data);
      WriteDragData(model_->GetBookmarkBarNode()->GetChild(i), data);
      return;
    }
  }
  NOTREACHED();
}

void BookmarkBarView::WriteDragData(BookmarkNode* node,
                                    OSExchangeData* data) {
  DCHECK(node && data);
  BookmarkDragData drag_data(node);
  drag_data.Write(profile_, data);
}

int BookmarkBarView::GetDragOperations(View* sender, int x, int y) {
  for (int i = 0; i < GetBookmarkButtonCount(); ++i) {
    if (sender == GetBookmarkButton(i)) {
      return GetDragOperationsForNode(
          model_->GetBookmarkBarNode()->GetChild(i));
    }
  }
  NOTREACHED();
  return DragDropTypes::DRAG_NONE;
}

void BookmarkBarView::RunMenu(views::View* view,
                              const CPoint& pt,
                              HWND hwnd) {
  BookmarkNode* node;
  MenuItemView::AnchorPosition anchor_point = MenuItemView::TOPLEFT;

  // When we set the menu's position, we must take into account the mirrored
  // position of the View relative to its parent. This can be easily done by
  // passing the right flag to View::x().
  int x = view->GetX(APPLY_MIRRORING_TRANSFORMATION);
  int bar_height = height() - kMenuOffset;

  if (OnNewTabPage() && !IsAlwaysShown())
    bar_height -= kNewtabVerticalPadding;

  int start_index = 0;
  if (view == other_bookmarked_button_) {
    UserMetrics::RecordAction(L"BookmarkBar_ShowOtherBookmarks", profile_);

    node = model_->other_node();
    if (UILayoutIsRightToLeft())
      anchor_point = MenuItemView::TOPLEFT;
    else
      anchor_point = MenuItemView::TOPRIGHT;
  } else if (view == overflow_button_) {
    node = model_->GetBookmarkBarNode();
    start_index = GetFirstHiddenNodeIndex();
    if (UILayoutIsRightToLeft())
      anchor_point = MenuItemView::TOPLEFT;
    else
      anchor_point = MenuItemView::TOPRIGHT;
  } else {
    int button_index = GetChildIndex(view);
    DCHECK(button_index != -1);
    node = model_->GetBookmarkBarNode()->GetChild(button_index);

    // When the UI layout is RTL, the bookmarks are laid out from right to left
    // and therefore when we display the menu we want it to be aligned with the
    // bottom right corner of the bookmark item.
    if (UILayoutIsRightToLeft())
      anchor_point = MenuItemView::TOPRIGHT;
    else
      anchor_point = MenuItemView::TOPLEFT;
  }
  gfx::Point screen_loc(x, 0);
  View::ConvertPointToScreen(this, &screen_loc);
  menu_runner_.reset(new MenuRunner(this, node, start_index));
  HWND parent_hwnd = GetWidget()->GetHWND();
  menu_runner_->RunMenuAt(parent_hwnd,
                          gfx::Rect(screen_loc.x(), screen_loc.y(),
                                    view->width(), bar_height),
                          anchor_point,
                          false);
}

void BookmarkBarView::ButtonPressed(views::BaseButton* sender) {
  BookmarkNode* node;
  if (sender->GetTag() == kOtherFolderButtonTag) {
    node = model_->other_node();
  } else {
    int index = GetChildIndex(sender);
    DCHECK(index != -1);
    node = model_->GetBookmarkBarNode()->GetChild(index);
  }
  DCHECK(page_navigator_);
  if (node->is_url()) {
    page_navigator_->OpenURL(
        node->GetURL(), GURL(),
        event_utils::DispositionFromEventFlags(sender->mouse_event_flags()),
        PageTransition::AUTO_BOOKMARK);
  } else {
    bookmark_utils::OpenAll(
        GetWidget()->GetHWND(), profile_, GetPageNavigator(), node,
        event_utils::DispositionFromEventFlags(sender->mouse_event_flags()));
  }
  UserMetrics::RecordAction(L"ClickedBookmarkBarURLButton", profile_);
}

void BookmarkBarView::ShowContextMenu(View* source,
                                      int x,
                                      int y,
                                      bool is_mouse_gesture) {
  if (!model_->IsLoaded()) {
    // Don't do anything if the model isn't loaded.
    return;
  }

  BookmarkNode* parent = NULL;
  std::vector<BookmarkNode*> nodes;
  if (source == other_bookmarked_button_) {
    parent = model_->other_node();
    // Do this so the user can open all bookmarks. BookmarkContextMenu makes
    // sure the user can edit/delete the node in this case.
    nodes.push_back(parent);
  } else if (source != this) {
    // User clicked on one of the bookmark buttons, find which one they
    // clicked on.
    int bookmark_button_index = GetChildIndex(source);
    DCHECK(bookmark_button_index != -1 &&
           bookmark_button_index < GetBookmarkButtonCount());
    BookmarkNode* node =
        model_->GetBookmarkBarNode()->GetChild(bookmark_button_index);
    nodes.push_back(node);
    parent = node->GetParent();
  } else {
    parent = model_->GetBookmarkBarNode();
    nodes.push_back(parent);
  }
  BookmarkContextMenu controller(GetWidget()->GetHWND(),
                                 GetProfile(), browser(), GetPageNavigator(),
                                 parent, nodes,
                                 BookmarkContextMenu::BOOKMARK_BAR);
  controller.RunMenuAt(x, y);
}

views::View* BookmarkBarView::CreateBookmarkButton(BookmarkNode* node) {
  if (node->GetType() == history::StarredEntry::URL) {
    BookmarkButton* button = new BookmarkButton(node->GetURL(),
                                                node->GetTitle(),
                                                GetProfile());
    button->SetListener(this, 0);
    ConfigureButton(node, button);
    return button;
  } else {
    views::MenuButton* button =
        new BookmarkFolderButton(node->GetTitle(), this, false);
    button->SetIcon(GetGroupIcon());
    button->SetListener(this, 0);
    ConfigureButton(node, button);
    return button;
  }
}

void BookmarkBarView::ConfigureButton(BookmarkNode* node,
                                      views::TextButton* button) {
  button->SetText(node->GetTitle());
  button->ClearMaxTextSize();
  button->SetContextMenuController(this);
  button->SetDragController(this);
  if (node->GetType() == history::StarredEntry::URL) {
    if (node->GetFavIcon().width() != 0)
      button->SetIcon(node->GetFavIcon());
    else
      button->SetIcon(*kDefaultFavIcon);
  }
  button->set_max_width(kMaxButtonWidth);
}

bool BookmarkBarView::IsItemChecked(int id) const {
  DCHECK(id == kAlwaysShowCommandID);
  return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

void BookmarkBarView::ExecuteCommand(int id) {
  ToggleWhenVisible(profile_);
}

void BookmarkBarView::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK(profile_);
  switch (type) {
    case NOTIFY_BOOKMARK_BAR_VISIBILITY_PREF_CHANGED:
      if (IsAlwaysShown()) {
        size_animation_->Show();
      } else {
        size_animation_->Hide();
      }
      break;

    case NOTIFY_BOOKMARK_BUBBLE_SHOWN:
      StopThrobbing(true);
      bubble_url_ = *(Details<GURL>(details).ptr());
      StartThrobbing();
      break;

    case NOTIFY_BOOKMARK_BUBBLE_HIDDEN:
      StopThrobbing(false);
      bubble_url_ = GURL();
      break;
  }
}

void BookmarkBarView::RemoveNotificationObservers() {
  NotificationService* ns = NotificationService::current();
  Source<Profile> ns_source(profile_->GetOriginalProfile());
  ns->RemoveObserver(this, NOTIFY_BOOKMARK_BUBBLE_SHOWN, ns_source);
  ns->RemoveObserver(this, NOTIFY_BOOKMARK_BUBBLE_HIDDEN, ns_source);
  ns->RemoveObserver(this, NOTIFY_BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
                     NotificationService::AllSources());
}

void BookmarkBarView::NotifyModelChanged() {
  if (model_changed_listener_)
    model_changed_listener_->ModelChanged();
}

void BookmarkBarView::ShowDropFolderForNode(BookmarkNode* node) {
  if (drop_menu_runner_.get() && drop_menu_runner_->GetNode() == node) {
    // Already showing for the specified node.
    return;
  }

  int start_index = 0;
  View* view_to_position_menu_from;

  // Note that both the anchor position and the position of the menu itself
  // change depending on the locale. Also note that we must apply the
  // mirroring transformation when querying for the child View bounds
  // (View::x(), specifically) so that we end up with the correct screen
  // coordinates if the View in question is mirrored.
  MenuItemView::AnchorPosition anchor = MenuItemView::TOPLEFT;
  if (node == model_->other_node()) {
    view_to_position_menu_from = other_bookmarked_button_;
    if (!UILayoutIsRightToLeft())
      anchor = MenuItemView::TOPRIGHT;
  } else if (node == model_->GetBookmarkBarNode()) {
    DCHECK(overflow_button_->IsVisible());
    view_to_position_menu_from = overflow_button_;
    start_index = GetFirstHiddenNodeIndex();
    if (!UILayoutIsRightToLeft())
      anchor = MenuItemView::TOPRIGHT;
  } else {
    // Make sure node is still valid.
    int index = -1;
    for (int i = 0; i < GetBookmarkButtonCount(); ++i) {
      if (model_->GetBookmarkBarNode()->GetChild(i) == node) {
        index = i;
        break;
      }
    }
    if (index == -1)
      return;
    view_to_position_menu_from = GetBookmarkButton(index);
    if (UILayoutIsRightToLeft())
      anchor = MenuItemView::TOPRIGHT;
  }

  drop_info_->is_menu_showing = true;
  drop_menu_runner_.reset(new MenuRunner(this, node, start_index));
  gfx::Point screen_loc;
  View::ConvertPointToScreen(view_to_position_menu_from, &screen_loc);
  drop_menu_runner_->RunMenuAt(
      GetWidget()->GetHWND(),
      gfx::Rect(screen_loc.x(), screen_loc.y(),
                view_to_position_menu_from->width(),
                view_to_position_menu_from->height()),
      anchor, true);
}

void BookmarkBarView::StopShowFolderDropMenuTimer() {
  if (show_folder_drop_menu_task_)
    show_folder_drop_menu_task_->Cancel();
}

void BookmarkBarView::StartShowFolderDropMenuTimer(BookmarkNode* node) {
  if (testing_) {
    // So that tests can run as fast as possible disable the delay during
    // testing.
    ShowDropFolderForNode(node);
    return;
  }
  DCHECK(!show_folder_drop_menu_task_);
  show_folder_drop_menu_task_ = new ShowFolderDropMenuTask(this, node);
  static DWORD delay = 0;
  if (!delay && !SystemParametersInfo(SPI_GETMENUSHOWDELAY, 0, &delay, 0)) {
    delay = kShowFolderDropMenuDelay;
  }
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          show_folder_drop_menu_task_, delay);
}

int BookmarkBarView::CalculateDropOperation(const DropTargetEvent& event,
                                            const BookmarkDragData& data,
                                            int* index,
                                            bool* drop_on,
                                            bool* is_over_overflow,
                                            bool* is_over_other) {
  DCHECK(model_);
  DCHECK(model_->IsLoaded());
  DCHECK(data.is_valid());

  // The drop event uses the screen coordinates while the child Views are
  // always laid out from left to right (even though they are rendered from
  // right-to-left on RTL locales). Thus, in order to make sure the drop
  // coordinates calculation works, we mirror the event's X coordinate if the
  // locale is RTL.
  int mirrored_x = MirroredXCoordinateInsideView(event.x());

  *index = -1;
  *drop_on = false;
  *is_over_other = *is_over_overflow = false;

  if (event.y() < other_bookmarked_button_->y() ||
      event.y() >= other_bookmarked_button_->y() +
                      other_bookmarked_button_->height()) {
    // Mouse isn't over a button.
    return DragDropTypes::DRAG_NONE;
  }

  bool found = false;
  const int other_delta_x = mirrored_x - other_bookmarked_button_->x();
  if (other_delta_x >= 0 &&
      other_delta_x < other_bookmarked_button_->width()) {
    // Mouse is over 'other' folder.
    *is_over_other = true;
    *drop_on = true;
    found = true;
  } else if (!GetBookmarkButtonCount()) {
    // No bookmarks, accept the drop.
    *index = 0;
    int ops = data.GetFirstNode(profile_)
        ? DragDropTypes::DRAG_MOVE
        : DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_LINK;
    return
        bookmark_utils::PreferredDropOperation(event.GetSourceOperations(),
                                               ops);
  }

  for (int i = 0; i < GetBookmarkButtonCount() &&
       GetBookmarkButton(i)->IsVisible() && !found; i++) {
    views::TextButton* button = GetBookmarkButton(i);
    int button_x = mirrored_x - button->x();
    int button_w = button->width();
    if (button_x < button_w) {
      found = true;
      BookmarkNode* node = model_->GetBookmarkBarNode()->GetChild(i);
      if (node->GetType() != history::StarredEntry::URL) {
        if (button_x <= views::kDropBetweenPixels) {
          *index = i;
        } else if (button_x < button_w - views::kDropBetweenPixels) {
          *index = i;
          *drop_on = true;
        } else {
          *index = i + 1;
        }
      } else if (button_x < button_w / 2) {
        *index = i;
      } else {
        *index = i + 1;
      }
      break;
    }
  }

  if (!found) {
    if (overflow_button_->IsVisible()) {
      // Are we over the overflow button?
      int overflow_delta_x = mirrored_x - overflow_button_->x();
      if (overflow_delta_x >= 0 &&
          overflow_delta_x < overflow_button_->width()) {
        // Mouse is over overflow button.
        *index = GetFirstHiddenNodeIndex();
        *is_over_overflow = true;
      } else if (overflow_delta_x < 0) {
        // Mouse is after the last visible button but before overflow button;
        // use the last visible index.
        *index = GetFirstHiddenNodeIndex();
      } else {
        return DragDropTypes::DRAG_NONE;
      }
    } else if (mirrored_x < other_bookmarked_button_->x()) {
      // Mouse is after the last visible button but before more recently
      // bookmarked; use the last visible index.
      *index = GetFirstHiddenNodeIndex();
    } else {
      return DragDropTypes::DRAG_NONE;
    }
  }

  if (*drop_on) {
    BookmarkNode* parent =
        *is_over_other ? model_->other_node() :
                         model_->GetBookmarkBarNode()->GetChild(*index);
    int operation =
        CalculateDropOperation(event, data, parent, parent->GetChildCount());
    if (!operation && !data.has_single_url() &&
        data.GetFirstNode(profile_) == parent) {
      // Don't open a menu if the node being dragged is the the menu to
      // open.
      *drop_on = false;
    }
    return operation;
  } else {
    return CalculateDropOperation(event, data, model_->GetBookmarkBarNode(),
                                  *index);
  }
}

int BookmarkBarView::CalculateDropOperation(const DropTargetEvent& event,
                                            const BookmarkDragData& data,
                                            BookmarkNode* parent,
                                            int index) {
  if (data.IsFromProfile(profile_) && data.size() > 1)
    // Currently only accept one dragged node at a time.
    return DragDropTypes::DRAG_NONE;

  if (!bookmark_utils::IsValidDropLocation(profile_, data, parent, index))
    return DragDropTypes::DRAG_NONE;

  if (data.GetFirstNode(profile_)) {
    // User is dragging from this profile: move.
    return DragDropTypes::DRAG_MOVE;
  } else {
    // User is dragging from another app, copy.
    return bookmark_utils::PreferredDropOperation(
        event.GetSourceOperations(),
        DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_LINK);
  }
}

int BookmarkBarView::PerformDropImpl(const BookmarkDragData& data,
                                     BookmarkNode* parent_node,
                                     int index) {
  BookmarkNode* dragged_node = data.GetFirstNode(profile_);
  if (dragged_node) {
    // Drag from same profile, do a move.
    model_->Move(dragged_node, parent_node, index);
    return DragDropTypes::DRAG_MOVE;
  } else if (data.has_single_url()) {
    // New URL, add it at the specified location.
    std::wstring title = data.elements[0].title;
    if (title.empty()) {
      // No title, use the host.
      title = UTF8ToWide(data.elements[0].url.host());
      if (title.empty())
        title = l10n_util::GetString(IDS_BOOMARK_BAR_UNKNOWN_DRAG_TITLE);
    }
    model_->AddURL(parent_node, index, title, data.elements[0].url);
    return DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_LINK;
  } else {
    // Dropping a group from different profile. Always accept.
    bookmark_utils::CloneDragData(model_, data.elements, parent_node, index);
    return DragDropTypes::DRAG_COPY;
  }
}

int BookmarkBarView::GetFirstHiddenNodeIndex() {
  const int bb_count = GetBookmarkButtonCount();
  for (int i = 0; i < bb_count; ++i) {
    if (!GetBookmarkButton(i)->IsVisible())
      return i;
  }
  return bb_count;
}

void BookmarkBarView::StartThrobbing() {
  DCHECK(!throbbing_view_);

  if (bubble_url_.is_empty())
    return;  // Bubble isn't showing; nothing to throb.

  if (!GetWidget())
    return;  // We're not showing, don't do anything.

  BookmarkNode* node = model_->GetMostRecentlyAddedNodeForURL(bubble_url_);
  if (!node)
    return;  // Generally shouldn't happen.

  // Determine which visible button is showing the url (or is an ancestor of
  // the url).
  if (node->HasAncestor(model_->GetBookmarkBarNode())) {
    BookmarkNode* bbn = model_->GetBookmarkBarNode();
    BookmarkNode* parent_on_bb = node;
    while (parent_on_bb->GetParent() != bbn)
      parent_on_bb = parent_on_bb->GetParent();
    int index = bbn->IndexOfChild(parent_on_bb);
    if (index >= GetFirstHiddenNodeIndex()) {
      // Node is hidden, animate the overflow button.
      throbbing_view_ = overflow_button_;
    } else {
      throbbing_view_ = static_cast<BaseButton*>(GetChildViewAt(index));
    }
  } else {
    throbbing_view_ = other_bookmarked_button_;
  }

  // Use a large number so that the button continues to throb.
  throbbing_view_->StartThrobbing(std::numeric_limits<int>::max());
}

void BookmarkBarView::StopThrobbing(bool immediate) {
  if (!throbbing_view_)
    return;

  // If not immediate, cycle through 2 more complete cycles.
  throbbing_view_->StartThrobbing(immediate ? 0 : 4);
  throbbing_view_ = NULL;
}
