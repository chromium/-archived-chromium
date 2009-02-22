// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/native_ui_contents.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/history_tab_ui.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/views/download_tab_view.h"
#include "chrome/common/drag_drop_types.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/background.h"
#include "chrome/views/checkbox.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/image_view.h"
#include "chrome/views/root_view.h"
#include "chrome/views/scroll_view.h"
#include "chrome/views/throbber.h"
#include "chrome/views/widget_win.h"
#include "grit/generated_resources.h"

using views::ColumnSet;
using views::GridLayout;

//static
bool NativeUIContents::g_ui_factories_initialized = false;

// The URL scheme currently used.
static const char kNativeUIContentsScheme[] = "chrome-nativeui";

// Unique page id generator.
static int g_next_page_id = 0;

// The x-position of the title.
static const int kDestinationTitleOffset = 38;

// The x-position of the search field.
static const int kDestinationSearchOffset = 128;

// The width of the search field.
static const int kDestinationSearchWidth = 360;

// Padding between columns
static const int kDestinationSmallerMargin = 8;

// The background color.
static const SkColor kBackground = SkColorSetRGB(255, 255, 255);

// The color of the bottom margin.
static const SkColor kBottomMarginColor = SkColorSetRGB(246, 249, 255);

// The height of the bottom margin.
static const int kBottomMargin = 5;

// The Chrome product logo.
static const SkBitmap* kProductLogo = NULL;

// Padding around the product logo.
static const int kProductLogoPadding = 8;

namespace {

// NativeRootView --------------------------------------------------------------

// NativeRootView is a trivial RootView subclass that allows URL drops and
// forwards them to the NavigationController to open.

class NativeRootView : public views::RootView {
 public:
  explicit NativeRootView(NativeUIContents* host)
      : RootView(host),
        host_(host) { }

  virtual ~NativeRootView() { }

  virtual bool CanDrop(const OSExchangeData& data) {
    return data.HasURL();
  }

  virtual int OnDragUpdated(const views::DropTargetEvent& event) {
    if (event.GetSourceOperations() & DragDropTypes::DRAG_COPY)
      return DragDropTypes::DRAG_COPY;
    if (event.GetSourceOperations() & DragDropTypes::DRAG_LINK)
      return DragDropTypes::DRAG_LINK;
    return DragDropTypes::DRAG_NONE;
  }

  virtual int OnPerformDrop(const views::DropTargetEvent& event) {
    GURL url;
    std::wstring title;
    if (!event.GetData().GetURLAndTitle(&url, &title) || !url.is_valid())
      return DragDropTypes::DRAG_NONE;
    host_->controller()->LoadURL(url, GURL(), PageTransition::GENERATED);
    return OnDragUpdated(event);
  }

 private:
  NativeUIContents* host_;

  DISALLOW_EVIL_CONSTRUCTORS(NativeRootView);
};

}  // namespace


// Returns the end of the scheme and end of the host. This is temporary until
// bug 772411 is fixed.
static void GetSchemeAndHostEnd(const GURL& url,
                                size_t* scheme_end,
                                size_t* host_end) {
  const std::string spec = url.spec();
  *scheme_end = spec.find("//");
  DCHECK(*scheme_end != std::string::npos);

  *host_end = spec.find('/', *scheme_end + 2);
  if (*host_end == std::string::npos)
    *host_end = spec.size();
}

NativeUIContents::NativeUIContents(Profile* profile)
    : TabContents(TAB_CONTENTS_NATIVE_UI),
      is_visible_(false),
      current_ui_(NULL),
      current_view_(NULL),
      state_(new PageState()) {
  if (!g_ui_factories_initialized) {
    InitializeNativeUIFactories();
    g_ui_factories_initialized = true;
  }
}

NativeUIContents::~NativeUIContents() {
  if (current_ui_) {
    views::RootView* root_view = GetRootView();
    current_ui_->WillBecomeInvisible(this);
    root_view->RemoveChildView(current_view_);
    current_ui_ = NULL;
    current_view_ = NULL;
  }

  STLDeleteContainerPairSecondPointers(path_to_native_uis_.begin(),
                                       path_to_native_uis_.end());
}

void NativeUIContents::CreateView() {
  set_delete_on_destroy(false);
  WidgetWin::Init(GetDesktopWindow(), gfx::Rect(), false);
}

LRESULT NativeUIContents::OnCreate(LPCREATESTRUCT create_struct) {
  // Set the view container initial size.
  CRect tmp;
  ::GetWindowRect(GetHWND(), &tmp);
  tmp.right = tmp.Width();
  tmp.bottom = tmp.Height();
  tmp.left = tmp.top = 0;

  // Install the focus manager so we get notified of Tab key events.
  views::FocusManager::InstallFocusSubclass(GetHWND(), NULL);
  GetRootView()->set_background(new NativeUIBackground);
  return 0;
}

void NativeUIContents::OnDestroy() {
  views::FocusManager::UninstallFocusSubclass(GetHWND());
}

void NativeUIContents::OnSize(UINT size_command, const CSize& new_size) {
  Layout();
  ::RedrawWindow(GetHWND(), NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void NativeUIContents::OnWindowPosChanged(WINDOWPOS* position) {
  // NOTE: this may be invoked even when the visbility didn't change, in which
  // case hiding and showing are both false.
  const bool hiding = (position->flags & SWP_HIDEWINDOW) == SWP_HIDEWINDOW;
  const bool showing = (position->flags & SWP_SHOWWINDOW) == SWP_SHOWWINDOW;
  if (hiding || showing) {
    if (is_visible_ != showing) {
      is_visible_ = showing;
      if (current_ui_) {
        if (is_visible_)
          current_ui_->WillBecomeVisible(this);
        else
          current_ui_->WillBecomeInvisible(this);
      }
    }
  }
  ChangeSize(0, CSize(position->cx, position->cy));

  SetMsgHandled(FALSE);
}

void NativeUIContents::GetContainerBounds(gfx::Rect* out) const {
  GetBounds(out, false);
}

void NativeUIContents::SetPageState(PageState* page_state) {
  if (!page_state)
    page_state = new PageState();
  state_.reset(page_state);
  NavigationController* ctrl = controller();
  if (ctrl) {
    int ne_index = ctrl->GetLastCommittedEntryIndex();
    NavigationEntry* ne = ctrl->GetEntryAtIndex(ne_index);
    if (ne) {
      // NavigationEntry is null if we're being restored.
      DCHECK(ne);
      std::string rep;
      state_->GetByteRepresentation(&rep);
      ne->set_content_state(rep);
      ctrl->NotifyEntryChanged(ne, ne_index);
    }
  }
}

bool NativeUIContents::NavigateToPendingEntry(bool reload) {
  views::RootView* root_view = GetRootView();
  DCHECK(root_view);

  if (current_ui_) {
    current_ui_->WillBecomeInvisible(this);
    root_view->RemoveChildView(current_view_);
    current_ui_ = NULL;
    current_view_ = NULL;
  }

  NavigationEntry* pending_entry = controller()->GetPendingEntry();
  NativeUI* new_ui = GetNativeUIForURL(pending_entry->url());
  if (new_ui) {
    current_ui_ = new_ui;
    is_visible_ = true;
    current_ui_->WillBecomeVisible(this);
    current_view_ = new_ui->GetView();
    root_view->AddChildView(current_view_);

    std::string s = pending_entry->content_state();
    if (s.empty())
      state_->InitWithURL(pending_entry->url());
    else
      state_->InitWithBytes(s);

    current_ui_->Navigate(*state_);
    Layout();
  }

  // Commit the new load in the navigation controller. If the ID of the
  // NavigationEntry we were given was -1, that means this is a new load, so
  // we have to generate a new ID.
  controller()->CommitPendingEntry();

  // Populate the committed entry.
  NavigationEntry* committed_entry = controller()->GetLastCommittedEntry();
  committed_entry->set_title(GetDefaultTitle());
  committed_entry->favicon().set_bitmap(GetFavIcon());
  committed_entry->favicon().set_is_valid(true);
  if (new_ui) {
    // Strip out the query params, they should have moved to state.
    // TODO(sky): use GURL methods for replacements once bug is fixed.
    size_t scheme_end, host_end;
    GetSchemeAndHostEnd(committed_entry->url(), &scheme_end, &host_end);
    committed_entry->set_url(
        GURL(committed_entry->url().spec().substr(0, host_end)));
  }
  std::string content_state;
  state_->GetByteRepresentation(&content_state);
  committed_entry->set_content_state(content_state);

  // Broadcast the fact that we just updated all that crap.
  controller()->NotifyEntryChanged(
      committed_entry,
      controller()->GetIndexOfEntry(committed_entry));
  return true;
}

void NativeUIContents::Layout() {
  if (current_view_) {
    views::RootView* root_view = GetRootView();
    current_view_->SetBounds(0, 0, root_view->width(),
                             root_view->height());
    current_view_->Layout();
  }
}

const std::wstring NativeUIContents::GetDefaultTitle() const {
  if (current_ui_)
    return current_ui_->GetTitle();
  else
    return std::wstring();
}

SkBitmap NativeUIContents::GetFavIcon() const {
  int icon_id;

  if (current_ui_)
    icon_id = current_ui_->GetFavIconID();
  else
    icon_id = IDR_DEFAULT_FAVICON;

  return *ResourceBundle::GetSharedInstance().GetBitmapNamed(icon_id);
}

void NativeUIContents::DidBecomeSelected() {
  TabContents::DidBecomeSelected();
  Layout();
}

void NativeUIContents::SetInitialFocus() {
  if (!current_ui_ || !current_ui_->SetInitialFocus()) {
    int tab_index;
    Browser* browser = Browser::GetBrowserForController(
        this->controller(), &tab_index);
    if (browser)
      browser->SetFocusToLocationBar();
    else
      TabContents::SetInitialFocus();  // Will set focus to our HWND.
  }
}

void NativeUIContents::SetIsLoading(bool is_loading,
                                    LoadNotificationDetails* details) {
  TabContents::SetIsLoading(is_loading, details);
}

// FocusTraversable Implementation
views::View* NativeUIContents::FindNextFocusableView(
    views::View* starting_view, bool reverse,
    views::FocusTraversable::Direction direction, bool dont_loop,
    views::FocusTraversable** focus_traversable,
    views::View** focus_traversable_view) {
  return GetRootView()->FindNextFocusableView(
      starting_view, reverse, direction, dont_loop,
      focus_traversable, focus_traversable_view);
}

//static
std::string NativeUIContents::GetScheme() {
  return kNativeUIContentsScheme;
}

//static
void NativeUIContents::InitializeNativeUIFactories() {
  RegisterNativeUIFactory(DownloadTabUI::GetURL(),
                          DownloadTabUI::GetNativeUIFactory());
  RegisterNativeUIFactory(HistoryTabUI::GetURL(),
                          HistoryTabUI::GetNativeUIFactory());
}

// static
std::string NativeUIContents::GetFactoryKey(const GURL& url) {
  size_t scheme_end;
  size_t host_end;
  GetSchemeAndHostEnd(url, &scheme_end, &host_end);
  return url.spec().substr(scheme_end + 2, host_end - scheme_end - 2);
}

typedef std::map<std::string, NativeUIFactory*> PathToFactoryMap;

static PathToFactoryMap* g_path_to_factory = NULL;

//static
void NativeUIContents::RegisterNativeUIFactory(const GURL& url,
                                               NativeUIFactory* factory) {
  const std::string key = GetFactoryKey(url);

  if (!g_path_to_factory)
    g_path_to_factory = new PathToFactoryMap;

  PathToFactoryMap::iterator i = g_path_to_factory->find(key);
  if (i != g_path_to_factory->end()) {
    delete i->second;
    g_path_to_factory->erase(i);
  }
  (*g_path_to_factory)[key] = factory;
}

views::RootView* NativeUIContents::CreateRootView() {
  return new NativeRootView(this);
}

//static
NativeUI* NativeUIContents::InstantiateNativeUIForURL(
    const GURL& url, NativeUIContents* contents) {
  if (!g_path_to_factory)
    return NULL;

  const std::string key = GetFactoryKey(url);

  NativeUIFactory* factory = (*g_path_to_factory)[key];
  if (factory)
    return factory->CreateNativeUIForURL(url, contents);
  else
    return NULL;
}

NativeUI* NativeUIContents::GetNativeUIForURL(const GURL& url) {
  const std::string key = GetFactoryKey(url);

  PathToUI::iterator i = path_to_native_uis_.find(key);
  if (i != path_to_native_uis_.end())
    return i->second;

  NativeUI* ui = InstantiateNativeUIForURL(url, this);
  if (ui)
    path_to_native_uis_[key] = ui;
  return ui;
}


////////////////////////////////////////////////////////////////////////////////
//
// Standard NativeUI background implementation.
//
////////////////////////////////////////////////////////////////////////////////
NativeUIBackground::NativeUIBackground() {
}

NativeUIBackground::~NativeUIBackground() {
}

void NativeUIBackground::Paint(ChromeCanvas* canvas,
                               views::View* view) const {
  static const SkColor kBackground = SkColorSetRGB(255, 255, 255);
  canvas->FillRectInt(kBackground, 0, 0, view->width(), view->height());
}

/////////////////////////////////////////////////////////////////////////////
//
// SearchableUIBackground
// A Background subclass to be used with SearchableUIContainer objects.
// Paint() is overridden to do nothing here; the background of the bar is
// painted in SearchableUIContainer::Paint.  This class is necessary
// only for native controls to be able to get query the background
// brush.

class SearchableUIBackground : public views::Background {
 public:
  explicit SearchableUIBackground(SkColor native_control_color) {
    SetNativeControlColor(native_control_color);
  }
  virtual ~SearchableUIBackground() {};

  // Empty implementation.
  // The actual painting of the bar happens in SearchableUIContainer::Paint.
  virtual void Paint(ChromeCanvas* canvas, views::View* view) const { }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(SearchableUIBackground);
};

/////////////////////////////////////////////////////////////////////////////
//
// SearchableUIContainer implementation.
//
/////////////////////////////////////////////////////////////////////////////

SearchableUIContainer::SearchableUIContainer(
    SearchableUIContainer::Delegate* delegate)
    : delegate_(delegate),
      search_field_(NULL),
      title_link_(NULL),
      title_image_(NULL),
      scroll_view_(NULL) {
  title_link_ = new views::Link;
  ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();
  ChromeFont title_font(resource_bundle
      .GetFont(ResourceBundle::WebFont).DeriveFont(2));
  title_link_->SetFont(title_font);
  title_link_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_link_->SetController(this);

  title_image_ = new views::ImageView();
  title_image_->SetVisible(false);

  // Get the product logo
  if (!kProductLogo) {
    kProductLogo = resource_bundle.GetBitmapNamed(IDR_PRODUCT_LOGO);
  }

  product_logo_ = new views::ImageView();
  product_logo_->SetVisible(true);
  product_logo_->SetImage(*kProductLogo);
  AddChildView(product_logo_);

  search_field_ = new views::TextField;
  search_field_->SetFont(ResourceBundle::GetSharedInstance().GetFont(
                             ResourceBundle::WebFont));
  search_field_->SetController(this);

  scroll_view_ = new views::ScrollView;
  scroll_view_->set_background(
      views::Background::CreateSolidBackground(kBackground));

  // Set background class so that native controls can get a color.
  set_background(new SearchableUIBackground(kBackground));

  throbber_ = new views::SmoothedThrobber(50);

  GridLayout* layout = new GridLayout(this);
  // View owns the LayoutManager and will delete it along with all the columns
  // we create here.
  SetLayoutManager(layout);

  search_button_ =
      new views::NativeButton(std::wstring());
  search_button_->SetFont(resource_bundle.GetFont(ResourceBundle::WebFont));
  search_button_->SetListener(this);

  // Set a background color for the search button.  If SearchableUIContainer
  // provided a background, then the search button could inherit that instead.
  search_button_->set_background(new SearchableUIBackground(kBackground));

  // For the first row (icon, title/text field, search button and throbber).
  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddPaddingColumn(0, kDestinationTitleOffset);

  // Add the icon column.
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF,
                        kDestinationSearchOffset - kDestinationTitleOffset -
                        kDestinationSmallerMargin,
                        kDestinationSearchOffset - kDestinationTitleOffset -
                        kDestinationSmallerMargin);
  column_set->AddPaddingColumn(0, kDestinationSmallerMargin);

  // Add the title/search field column.
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, kDestinationSearchWidth,
                        kDestinationSearchWidth);
  column_set->AddPaddingColumn(0, kDestinationSmallerMargin);

  // Add the search button column.
  column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kDestinationSmallerMargin);

  // Add the throbber column.
  column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  // For the scroll view.
  column_set = layout->AddColumnSet(1);
  column_set->AddPaddingColumn(0, 1);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->AddPaddingRow(0, kDestinationSmallerMargin);
  layout->StartRow(0, 0);
  layout->AddView(title_image_, 1, 2);
  layout->AddView(title_link_);

  layout->StartRow(0, 0);
  layout->SkipColumns(1);
  layout->AddView(search_field_);
  layout->AddView(search_button_);
  layout->AddView(throbber_);

  layout->AddPaddingRow(0, kDestinationSmallerMargin);
  layout->StartRow(1, 1);
  layout->AddView(scroll_view_);
}

SearchableUIContainer::~SearchableUIContainer() {
}

void SearchableUIContainer::SetContents(views::View* contents) {
  // The column view will resize to accomodate long titles.
  title_link_->SetText(delegate_->GetTitle());

  int section_icon_id = delegate_->GetSectionIconID();
  if (section_icon_id != 0) {
    title_image_->SetImage(*ResourceBundle::GetSharedInstance().
        GetBitmapNamed(section_icon_id));
    title_image_->SetVisible(true);
  }

  search_button_->SetLabel(delegate_->GetSearchButtonText());
  scroll_view_->SetContents(contents);
}

views::View* SearchableUIContainer::GetContents() {
  return scroll_view_->GetContents();
}

void SearchableUIContainer::Layout() {
  View::Layout();

  gfx::Size search_button_size = search_button_->GetPreferredSize();
  gfx::Size product_logo_size = product_logo_->GetPreferredSize();

  int field_width = kDestinationSearchOffset + 
    kDestinationSearchWidth +
    kDestinationSmallerMargin +
    static_cast<int>(search_button_size.width()) +
    kDestinationSmallerMargin;

  product_logo_->SetBounds(std::max(width() - kProductLogo->width() - 
                               kProductLogoPadding,
                               field_width), 
                           kProductLogoPadding, 
                           product_logo_size.width(),
                           product_logo_size.height());
}

void SearchableUIContainer::Paint(ChromeCanvas* canvas) {
  SkColor top_color(kBackground);
  canvas->FillRectInt(top_color, 0, 0,
                      width(), scroll_view_->y());

  canvas->FillRectInt(kBottomMarginColor, 0, scroll_view_->y() -
                      kBottomMargin, width(), kBottomMargin);

  canvas->FillRectInt(SkColorSetRGB(196, 196, 196),
                      0, scroll_view_->y() - 1, width(), 1);
}

views::TextField* SearchableUIContainer::GetSearchField() const {
  return search_field_;
}

views::ScrollView* SearchableUIContainer::GetScrollView() const {
  return scroll_view_;
}

void SearchableUIContainer::SetSearchEnabled(bool enabled) {
  search_field_->SetReadOnly(!enabled);
  search_button_->SetEnabled(enabled);
}

void SearchableUIContainer::StartThrobber() {
  throbber_->Start();
}

void SearchableUIContainer::StopThrobber() {
  throbber_->Stop();
}

void SearchableUIContainer::ButtonPressed(views::NativeButton* sender) {
  DoSearch();
}

void SearchableUIContainer::LinkActivated(views::Link *link,
                                          int event_flags) {
  if (link == title_link_) {
    search_field_->SetText(std::wstring());
    DoSearch();
  }
}

void SearchableUIContainer::HandleKeystroke(views::TextField* sender,
                                            UINT message,
                                            TCHAR key,
                                            UINT repeat_count,
                                            UINT flags) {
  if (key == VK_RETURN)
    DoSearch();
}

void SearchableUIContainer::DoSearch() {
  if (delegate_)
    delegate_->DoSearch(search_field_->GetText());

  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(), 0);
}

