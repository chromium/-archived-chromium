// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/old_frames/simple_xp_frame.h"

#include "chrome/app/theme/theme_resources.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/browser/views/tab_contents_container_view.h"
#include "chrome/browser/views/tabs/tab.h"
#include "chrome/browser/web_app.h"
#include "chrome/browser/web_app_icon_manager.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/win_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/focus_manager.h"
#include "chrome/views/label.h"
#include "chrome/views/text_field.h"
#include "net/base/net_util.h"
#include "SkBitmap.h"

#include "chromium_strings.h"
#include "generated_resources.h"

// The title bar text color.
static const SkColor kTitleBarTextColor = SkColorSetRGB(255, 255, 255);

// How thick is the top resize bar.
static const int kTopResizeBarHeight = 3;

// Left margin on the left side of the favicon.
static const int kFavIconMargin = 1;

// Label offset.
static const int kLabelVerticalOffset = -1;

// Padding between the favicon and the text.
static const int kFavIconPadding = 4;

// Background color for the button hot state.
static const SkColor kHotColor = SkColorSetRGB(49, 106, 197);

// distance between contents and drop arrow.
static const int kHorizMargin = 4;

// Border all around the menu.
static const int kHorizBorderSize = 2;
static const int kVertBorderSize = 1;

// How much wider or shorter the location bar is relative to the client area.
static const int kLocationBarOffset = 2;
// Spacing between the location bar and the content area.
static const int kLocationBarSpacing = 1;

////////////////////////////////////////////////////////////////////////////////
//
// TitleBarMenuButton implementation.
//
////////////////////////////////////////////////////////////////////////////////
TitleBarMenuButton::TitleBarMenuButton(SimpleXPFrameTitleBar* title_bar)
    : views::MenuButton(L"", title_bar, false),
      contents_(NULL),
      title_bar_(title_bar) {
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  drop_arrow_ = rb.GetBitmapNamed(IDR_APP_DROPARROW);
}

TitleBarMenuButton::~TitleBarMenuButton() {
}

void TitleBarMenuButton::SetContents(views::View* contents) {
  contents_ = contents;
}

gfx::Size TitleBarMenuButton::GetPreferredSize() {
  gfx::Size prefsize;
  if (contents_)
    prefsize = contents_->GetPreferredSize();

  prefsize.set_height(std::max(drop_arrow_->height(), prefsize.height()));
  prefsize.Enlarge(
      drop_arrow_->width() + kHorizMargin + (2 * kHorizBorderSize),
      2 * kVertBorderSize);
  return prefsize;
}

void TitleBarMenuButton::Paint(ChromeCanvas* canvas) {
  if (GetState() == TextButton::BS_HOT ||
      GetState() == TextButton::BS_PUSHED || menu_visible_) {
    canvas->FillRectInt(kHotColor, 0, 0, width(), height());
  }

  if (contents_) {
    gfx::Size s = contents_->GetPreferredSize();
    // Note: we use a floating view in this case because we never want the
    // contents to process any event.
    PaintFloatingView(canvas,
                      contents_,
                      kVertBorderSize,
                      (height() - s.height()) / 2,
                      width() - kHorizMargin - drop_arrow_->width() -
                      (2 * kHorizBorderSize),
                      s.height());
  }

  // We can not use the mirroring infrastructure in views in order to
  // mirror the drop down arrow because is is drawn directly on the canvas
  // (instead of using a child View). Thus, we should mirror its position
  // manually.
  gfx::Rect arrow_bounds(width() - drop_arrow_->width() - kHorizBorderSize,
                         (height() - drop_arrow_->height()) / 2,
                         drop_arrow_->width(),
                         drop_arrow_->height());
  arrow_bounds.set_x(MirroredLeftPointForRect(arrow_bounds));
  canvas->DrawBitmapInt(*drop_arrow_, arrow_bounds.x(), arrow_bounds.y());
}

bool TitleBarMenuButton::OnMousePressed(const views::MouseEvent& e) {
  if (e.GetFlags() & views::MouseEvent::EF_IS_DOUBLE_CLICK) {
    if (!HitTest(e.location()))
      return true;
    title_bar_->CloseWindow();
    return true;
  } else {
    return MenuButton::OnMousePressed(e);
  }
}


////////////////////////////////////////////////////////////////////////////////
//
// SimpleXPFrameTitleBar implementation.
//
////////////////////////////////////////////////////////////////////////////////

SimpleXPFrameTitleBar::SimpleXPFrameTitleBar(SimpleXPFrame* parent)
    : parent_(parent) {
  DCHECK(parent);
  tab_icon_.reset(new TabIconView(this));
  tab_icon_->set_is_light(true);
  menu_button_ = new TitleBarMenuButton(this);
  menu_button_->SetContents(tab_icon_view());
  AddChildView(menu_button_);

  tab_icon_->Update();

  label_ = new views::Label();
  label_->SetColor(kTitleBarTextColor);
  label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(label_);
}

SimpleXPFrameTitleBar::~SimpleXPFrameTitleBar() {
}

TabContents* SimpleXPFrameTitleBar::GetCurrentTabContents() {
  return parent_->GetCurrentContents();
}

SkBitmap SimpleXPFrameTitleBar::GetFavIcon() {
  // Only use the favicon if we're a web application.
  TabContents* contents = GetCurrentTabContents();
  if (parent_->IsApplication()) {
    TabContents* contents = GetCurrentTabContents();
    WebContents* web_contents = contents->AsWebContents();
    if (web_contents) {
      // If it's a web contents, pull the favicon from the WebApp, and fallback
      // to the icon.
      WebApp* web_app = web_contents->web_app();
      if (web_app && !web_app->GetFavIcon().isNull())
        return web_app->GetFavIcon();
    }
    if (contents) {
      SkBitmap favicon = contents->GetFavIcon();
      if (!favicon.isNull())
        return favicon;
    }
  }

  // Otherwise, use the default icon.
  return SkBitmap();
}

void SimpleXPFrameTitleBar::RunMenu(views::View* source,
                                    const CPoint& pt, HWND hwnd) {
  // Make sure we calculate the menu position based on the display bounds of
  // the menu button. The display bounds are different than the actual bounds
  // when the UI layout is RTL and hence we use the mirroring transformation
  // flag. We also adjust the menu position because RTL menus use a different
  // anchor point.
  gfx::Point p(menu_button_->GetX(APPLY_MIRRORING_TRANSFORMATION),
               menu_button_->y() + menu_button_->height());

  if (UILayoutIsRightToLeft())
    p.set_x(p.x() + menu_button_->width());
  View::ConvertPointToScreen(this, &p);
  parent_->RunMenu(p.ToPOINT(), hwnd);
}

void SimpleXPFrameTitleBar::Layout() {
  gfx::Size s = menu_button_->GetPreferredSize();
  menu_button_->SetBounds(kFavIconMargin, (height() - s.height()) / 2,
                          s.width(), s.height());
  menu_button_->Layout();
  label_->SetBounds(menu_button_->x() + menu_button_->width() +
                    kFavIconPadding, kLabelVerticalOffset,
                    width() - (menu_button_->x() +
                                  menu_button_->width() + kFavIconPadding),
                    height());
}

bool SimpleXPFrameTitleBar::WillHandleMouseEvent(int x, int y) {
  // If the locale is RTL, we must query for the bounds of the menu button in
  // a way that returns the mirrored position and not the position set using
  // SetX()/SetBounds().
  gfx::Point p(x - menu_button_->GetX(APPLY_MIRRORING_TRANSFORMATION),
               y - menu_button_->y());
  return menu_button_->HitTest(p);
}

void SimpleXPFrameTitleBar::SetWindowTitle(std::wstring s) {
  if (parent_->IsApplication()) {
    std::wstring t(s);
    Browser::FormatTitleForDisplay(&t);
    label_->SetText(t);
  } else {
    label_->SetText(Browser::ComputePopupTitle(
                        GetCurrentTabContents()->GetURL(), s));
  }
}

void SimpleXPFrameTitleBar::ValidateThrobber() {
  tab_icon_->Update();
  menu_button_->SchedulePaint();
}

void SimpleXPFrameTitleBar::CloseWindow() {
  parent_->Close();
}

void SimpleXPFrameTitleBar::Update() {
  tab_icon_->Update();
}

////////////////////////////////////////////////////////////////////////////////
//
// SimpleXPFrame implementation.
//
////////////////////////////////////////////////////////////////////////////////

// static
SimpleXPFrame* SimpleXPFrame::CreateFrame(const gfx::Rect& bounds,
                                          Browser* browser) {
  SimpleXPFrame* instance = new SimpleXPFrame(browser);
  instance->Create(NULL, bounds.ToRECT(),
                   l10n_util::GetString(IDS_PRODUCT_NAME).c_str());
  instance->InitAfterHWNDCreated();
  instance->SetIsOffTheRecord(browser->profile()->IsOffTheRecord());
  views::FocusManager::CreateFocusManager(instance->m_hWnd,
                                          instance->GetRootView());
  return instance;
}

SimpleXPFrame::SimpleXPFrame(Browser* browser)
    : XPFrame(browser),
      title_bar_(NULL),
      location_bar_(NULL) {
}

void SimpleXPFrame::InitAfterHWNDCreated() {
  icon_manager_.reset(new WebAppIconManager(*this));
  XPFrame::InitAfterHWNDCreated();
}

SimpleXPFrame::~SimpleXPFrame() {
}

void SimpleXPFrame::Init() {
  XPFrame::Init();
  if (IsTitleBarVisible()) {
    title_bar_ = new SimpleXPFrameTitleBar(this);
    GetFrameView()->AddChildView(title_bar_);
  }

  location_bar_ = new LocationBarView(browser_->profile(),
                                      browser_->controller(),
                                      browser_->toolbar_model(),
                                      this, true);
  GetFrameView()->AddChildView(location_bar_);
  location_bar_->Init();

  // Constrained popups that were unconstrained will need to set up a
  // throbber.
  UpdateTitleBar();
}

TabContents* SimpleXPFrame::GetCurrentContents() {
  if (browser_)
    return browser_->GetSelectedTabContents();
  else
    return NULL;
}

void SimpleXPFrame::Layout() {
  XPFrame::Layout();
  if (IsTitleBarVisible()) {
    TabContentsContainerView* tccv = GetTabContentsContainer();
    DCHECK(tccv);
    title_bar_->SetBounds(tccv->x(), 0,
                          GetButtonXOrigin() - tccv->x(),
                          GetContentsYOrigin());
    title_bar_->Layout();
  }

  if (browser_->ShouldDisplayURLField()) {
    TabContentsContainerView* container = GetTabContentsContainer();
    gfx::Size s = location_bar_->GetPreferredSize();
    location_bar_->SetBounds(container->x() - kLocationBarOffset,
                          container->y(),
                          container->width() + kLocationBarOffset * 2,
                          s.height());
    container->SetBounds(container->x(),
                         location_bar_->y() + location_bar_->height() +
                         kLocationBarSpacing, container->width(),
                         container->height() - location_bar_->height() -
                         1);
    location_bar_->SetVisible(true);
    location_bar_->Layout();
  } else {
    location_bar_->SetVisible(false);
  }
}

LRESULT SimpleXPFrame::OnNCHitTest(const CPoint& pt) {
  if (IsTitleBarVisible()) {
    gfx::Point p(pt);
    views::View::ConvertPointToView(NULL, title_bar_, &p);
    if (!title_bar_->WillHandleMouseEvent(p.x(), p.y()) &&
        p.x() >= 0 && p.y() >= kTopResizeBarHeight &&
        p.x() < title_bar_->width() &&
        p.y() < title_bar_->height()) {
      return HTCAPTION;
    }
  }
  return XPFrame::OnNCHitTest(pt);
}

void SimpleXPFrame::SetWindowTitle(const std::wstring& title) {
  if (IsTitleBarVisible())
    title_bar_->SetWindowTitle(title);
  XPFrame::SetWindowTitle(title);
}

void SimpleXPFrame::UpdateTitleBar() {
  if (IsTitleBarVisible()) {
    title_bar_->Update();
    title_bar_->SchedulePaint();
  }
  UpdateLocationBar();
}

void SimpleXPFrame::ValidateThrobber() {
  if (IsTitleBarVisible())
    title_bar_->ValidateThrobber();
}

void SimpleXPFrame::RunMenu(const CPoint& pt, HWND hwnd) {
  browser_->RunSimpleFrameMenu(gfx::Point(pt.x, pt.y), hwnd);
}

void SimpleXPFrame::ShowTabContents(TabContents* selected_contents) {
  XPFrame::ShowTabContents(selected_contents);

  icon_manager_->SetContents(selected_contents);

  UpdateLocationBar();
}

bool SimpleXPFrame::IsApplication() const {
  return browser_->IsApplication();
}

void SimpleXPFrame::UpdateLocationBar() {
  if (location_bar_ && location_bar_->IsVisible())
    location_bar_->Update(NULL);
}

TabContents* SimpleXPFrame::GetTabContents() {
  return GetCurrentContents();
}

void SimpleXPFrame::OnInputInProgress(bool in_progress) {
}

