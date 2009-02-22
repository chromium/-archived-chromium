// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_view.h"

#include "base/string_util.h"
#include "base/time_format.h"
#include "base/word_iterator.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/drag_utils.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/tab_contents/native_ui_contents.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/browser/views/bookmark_bubble_view.h"
#include "chrome/browser/views/event_utils.h"
#include "chrome/browser/views/star_toggle.h"
#include "chrome/common/drag_drop_types.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/favicon_size.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/time_format.h"
#include "chrome/common/win_util.h"
#include "chrome/views/link.h"
#include "chrome/views/widget.h"
#include "grit/generated_resources.h"

using base::Time;
using base::TimeDelta;

// The extra-wide space between groups of entries for each new day.
static const int kDayHeadingHeight = 50;

// The space between groups of entries within a day.
static const int kSessionBreakHeight = 24;

// Amount of time between page-views that triggers a break (in microseconds).
static const int64 kSessionBreakTime = 1800 * 1000000;  // 30 minutes

// Horizontal space between the left edge of the entries and the
// left edge of the view.
static const int kLeftMargin = 38;

// x-position of the page title (massage this so it visually matches
// kDestinationSearchOffset in native_ui_contents.cc
static const int kPageTitleOffset = 102;

// x-position of the Time
static const int kTimeOffset = 24;

// Vertical offset for the delete control (distance from the top of a day
// break segment).
static const int kDeleteControlOffset = 30;

// x-position of the session gap filler (currently a thin vertical line
// joining the times on either side of a session gap).
static const int kSessionGapOffset = 16;

// Horizontal space between the right edge of the item
// and the right edge of the view.
static const int kRightMargin = 20;

// The ideal height of an entry. This may change depending on font line-height.
static const int kSearchResultsHeight = 72;
static const int kBrowseResultsHeight = 24;

// How much room to leave above the first result.
static const int kResultsMargin = 24;

// Height of the results text area.
static const int kResultTextHeight = 24;

// Height of the area when there are no results to display.
static const int kNoResultTextHeight = 48;
static const int kNoResultMinWidth = 512;

// Extra vertical space between the different lines of text.
// (Note that the height() variables are baseline-to-baseline already.)
static const int kLeading = 2;

// The amount of space from the edges of an entry to the edges of its contents.
static const int kEntryPadding = 8;

// Padding between the icons (star, favicon) and other elements.
static const int kIconPadding = 4;

// SnippetRenderer is a View that can displayed text with bolding and wrapping.
// It's used to display search result snippets.
class SnippetRenderer : public views::View {
 public:
  SnippetRenderer();

  // Set the text snippet.
  void SetSnippet(const Snippet& snippet);

  int GetLineHeight();

  virtual void Paint(ChromeCanvas* canvas);

 private:
  // The snippet that we're drawing.
  Snippet snippet_;

  // Font for plain text.
  ChromeFont text_font_;
  // Font for match text.  (TODO(evanm): use red for Chinese (bug 844518).)
  ChromeFont match_font_;

  // Layout/draw a substring of the snippet from [start,end) at (x, y).
  // ProcessRun is strictly for text in a single line: it doesn't do any
  // word-wrapping, and is used as a helper for laying out multiple lines
  // of output in Pain().
  // match_iter is an iterator in match_runs_ that covers a region
  // before or at start.
  // When canvas is NULL, does no drawing and only computes the size.
  // Returns the pixel width of the run.
  // TODO(evanm): this could be optimizing by only measuring the text once
  // and returning the layout, but it's worth profiling first.
  int ProcessRun(ChromeCanvas* canvas,
                 int x,
                 int y,
                 Snippet::MatchPositions::const_iterator match_iter,
                 size_t start,
                 size_t end);

  DISALLOW_EVIL_CONSTRUCTORS(SnippetRenderer);
};

SnippetRenderer::SnippetRenderer() {
  ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();

  text_font_ = resource_bundle.GetFont(ResourceBundle::WebFont);
  match_font_ = text_font_.DeriveFont(0, ChromeFont::BOLD);
}

void SnippetRenderer::SetSnippet(const Snippet& snippet) {
  snippet_ = snippet;
}

int SnippetRenderer::GetLineHeight() {
  return std::max(text_font_.height(), match_font_.height()) + kLeading;
}

void SnippetRenderer::Paint(ChromeCanvas* canvas) {
  const int line_height = GetLineHeight();

  WordIterator iter(snippet_.text(), WordIterator::BREAK_LINE);
  if (!iter.Init())
    return;
  Snippet::MatchPositions::const_iterator match_iter =
      snippet_.matches().begin();

  int x = 0;
  int y = 0;
  while (iter.Advance()) {
    // Advance match_iter to a run that potentially covers this region.
    while (match_iter != snippet_.matches().end() &&
           match_iter->second <= iter.prev()) {
      ++match_iter;
    }

    // The region from iter.prev() to iter.pos() should be on one line.
    // It can be a mixture of bold and non-bold, so first lay it out to
    // compute its width.
    const int width = ProcessRun(NULL, 0, 0,
                                 match_iter, iter.prev(), iter.pos());
    // Advance to the next line if necessary.
    if (x + width > View::width()) {
      x = 0;
      y += line_height;
      if (y >= height())
        return;  // Out of vertical space.
    }
    ProcessRun(canvas, x, y, match_iter, iter.prev(), iter.pos());
    x += width;
  }
}

int SnippetRenderer::ProcessRun(
    ChromeCanvas* canvas,
    int x,
    int y,
    Snippet::MatchPositions::const_iterator match_iter,
    size_t start,
    size_t end) {
  int total_width = 0;

  while (start < end) {
    // Advance match_iter to the next match that can cover the current
    // position.
    while (match_iter != snippet_.matches().end() &&
           match_iter->second <= start) {
      ++match_iter;
    }

    // Determine the next substring to process by examining whether
    // we're before a match or within a match.
    ChromeFont* font = &text_font_;
    size_t next = end;
    if (match_iter != snippet_.matches().end()) {
      if (match_iter->first > start) {
        // We're in a plain region.
        next = std::min(match_iter->first, end);
      } else if (match_iter->first <= start &&
                 match_iter->second > start) {
        // We're in a match region.
        font = &match_font_;
        next = std::min(match_iter->second, end);
      }
    }

    // Draw/layout the text.
    const std::wstring run = snippet_.text().substr(start, next - start);
    const int width = font->GetStringWidth(run);
    if (canvas) {
      canvas->DrawStringInt(run, *font, SkColorSetRGB(0, 0, 0),
                            x + total_width, y,
                            width, height(),
                            ChromeCanvas::TEXT_VALIGN_BOTTOM);
    }

    // Advance.
    total_width += width;
    start = next;
  }

  return total_width;
}

// A View for an individual history result.
class HistoryItemRenderer : public views::View,
                            public views::LinkController,
                            public StarToggle::Delegate {
 public:
  HistoryItemRenderer(HistoryView* parent, bool show_full);
  ~HistoryItemRenderer();

  // Set the BaseHistoryModel that this renderer displays.
  // model_index is the index of this entry, and is passed to all of the
  // model functions.
  void SetModel(BaseHistoryModel* model, int model_index);

  // Set whether we should display full size or partial-sized items.
  void SetDisplayStyle(bool show_full);

  // Layout the contents of this view.
  void Layout();

 protected:
  // Overridden to do a drag if over the favicon or thumbnail.
  virtual int GetDragOperations(int press_x, int press_y);
  virtual void WriteDragData(int press_x, int press_y, OSExchangeData* data);

 private:
  // Regions drags may originate from.
  enum DragRegion {
    FAV_ICON,
    THUMBNAIL,
    NONE
  };

  // The thickness of the border drawn around thumbnails.
  static const int kThumbnailBorderWidth = 1;

  // The height of the thumbnail images.
  static const int kThumbnailHeight = kSearchResultsHeight - kEntryPadding * 2;

  // The width of the thumbnail images.
  static const int kThumbnailWidth = static_cast<int>(1.44 * kThumbnailHeight);

  // The maximum width of a snippet - we want to constrain this to make
  // snippets easier to read (like Google search results).
  static const int kMaxSnippetWidth = 500;

  // Returns the bounds of the thumbnail.
  void GetThumbnailBounds(CRect* rect);

  // Convert a GURL into a displayable string.
  std::wstring DisplayURL(const GURL& url);

  virtual void Paint(ChromeCanvas* canvas);

  // Notification that the star was changed.
  virtual void StarStateChanged(bool state);

  // Notification that the link was clicked.
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Returns the region the mouse is over.
  DragRegion GetDragRegion(int x, int y);

  // The HistoryView containing this view.
  HistoryView* parent_;

  // Whether we're showing a fullsize item, or a single-line item.
  bool show_full_;

  // The model and index of this entry within the model.
  BaseHistoryModel* model_;
  int model_index_;

  // Widgets.
  StarToggle* star_toggle_;
  views::Link* title_link_;
  views::Label* time_label_;
  SnippetRenderer* snippet_label_;

  DISALLOW_EVIL_CONSTRUCTORS(HistoryItemRenderer);
};

HistoryItemRenderer::HistoryItemRenderer(HistoryView* parent,
                                         bool show_full)
    : parent_(parent),
      show_full_(show_full) {
  ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();

  ChromeFont text_font(resource_bundle.GetFont(ResourceBundle::WebFont));

  star_toggle_ = new StarToggle(this);
  star_toggle_->set_change_state_immediately(false);
  AddChildView(star_toggle_);

  title_link_ = new views::Link();
  title_link_->SetFont(text_font);
  title_link_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_link_->SetController(this);
  AddChildView(title_link_);

  const SkColor kTimeColor = SkColorSetRGB(136, 136, 136);  // Gray.

  time_label_ = new views::Label();
  ChromeFont time_font(text_font);
  time_label_->SetFont(time_font);
  time_label_->SetColor(kTimeColor);
  time_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(time_label_);

  snippet_label_ = new SnippetRenderer();
  AddChildView(snippet_label_);
}

HistoryItemRenderer::~HistoryItemRenderer() {
}

void HistoryItemRenderer::GetThumbnailBounds(CRect* rect) {
  DCHECK(rect);
  rect->right = width() - kEntryPadding;
  rect->left = rect->right - kThumbnailWidth;
  rect->top = kEntryPadding;
  rect->bottom = rect->top + kThumbnailHeight;
}

std::wstring HistoryItemRenderer::DisplayURL(const GURL& url) {
  std::string url_str = url.spec();
  // Hide the "http://" prefix like web search does.
  if (url_str.find("http://") == 0)
    url_str.erase(0, strlen("http://"));
  return UTF8ToWide(url_str);
}

void HistoryItemRenderer::Paint(ChromeCanvas* canvas) {
  views::View::Paint(canvas);

  // Draw thumbnail or placeholder.
  if (show_full_) {
    SkBitmap* thumbnail = model_->GetThumbnail(model_index_);
    CRect thumbnail_rect;
    GetThumbnailBounds(&thumbnail_rect);  // Includes border

    // If the UI layout is right-to-left, we must mirror the bounds so that we
    // render the bitmap in the correct position.
    gfx::Rect mirrored_rect(thumbnail_rect);
    thumbnail_rect.MoveToX(MirroredLeftPointForRect(mirrored_rect));

    if (thumbnail) {
      // This will create a MipMap for the bitmap if one doesn't exist already
      // (it's a NOP if a MipMap already exists). This will give much smoother
      // results for the scaled-down thumbnails.
      thumbnail->buildMipMap(false);

      canvas->DrawBitmapInt(
          *thumbnail,
          0, 0, thumbnail->width(), thumbnail->height(),
          thumbnail_rect.left, thumbnail_rect.top,
          thumbnail_rect.Width(), thumbnail_rect.Height(),
          true);
    } else {
      canvas->FillRectInt(SK_ColorWHITE,
                          thumbnail_rect.left, thumbnail_rect.top,
                          thumbnail_rect.Width(), thumbnail_rect.Height());
    }
    canvas->DrawRectInt(SkColorSetRGB(153, 153, 191),
                        thumbnail_rect.left, thumbnail_rect.top,
                        thumbnail_rect.Width(), thumbnail_rect.Height());
  }

  // Draw the favicon.
  SkBitmap* favicon = model_->GetFavicon(model_index_);
  if (favicon) {
    // WARNING: if you change these values, update the code that determines
    // whether we should allow a drag (GetDragRegion).

    // We need to tweak the favicon position if the UI layout is RTL.
    gfx::Rect favicon_bounds;
    favicon_bounds.set_x(title_link_->x() - kIconPadding - kFavIconSize);
    favicon_bounds.set_y(kEntryPadding);
    favicon_bounds.set_width(favicon->width());
    favicon_bounds.set_height(favicon->height());
    favicon_bounds.set_x(MirroredLeftPointForRect(favicon_bounds));

    // Drawing the bitmap using the possibly adjusted bounds.
    canvas->DrawBitmapInt(*favicon, favicon_bounds.x(), favicon_bounds.y());
  }

  // The remainder of painting is handled by drawing our children, which
  // is managed by the View class for us.
}

void HistoryItemRenderer::Layout() {
  // Figure out the maximum x-position of any text.
  CRect thumbnail_rect;
  int max_x;
  if (show_full_) {
    GetThumbnailBounds(&thumbnail_rect);
    max_x = thumbnail_rect.left - kEntryPadding;
  } else {
    max_x = width() - kEntryPadding;
  }

  // Calculate the ideal positions of some items. If possible, we
  // want the title to line up with kPageTitleOffset (and we would lay
  // out the star and the favicon to the left of that), but in cases
  // where font or language choices cause the time label to be
  // horizontally large, we need to push everything to the right.
  //
  // If you fiddle with the calculations below, you may need to adjust
  // the favicon painting in Paint() (and in GetDragRegion by extension).

  // First we calculate the ideal position of the title.
  int title_x = kPageTitleOffset;

  // We calculate the size of the star.
  gfx::Size star_size = star_toggle_->GetPreferredSize();

  // Measure and lay out the time label, and potentially move
  // our title to suit.
  Time visit_time = model_->GetVisitTime(model_index_);
  int time_x = kTimeOffset;
  if (visit_time.is_null()) {
    // We will get null times if the page has never been visited, for example,
    // bookmarks after you clear history.
    time_label_->SetText(std::wstring());
  } else if (show_full_) {
    time_x = 0;
    time_label_->SetText(base::TimeFormatShortDate(visit_time));
  } else {
    time_label_->SetText(base::TimeFormatTimeOfDay(visit_time));
  }
  gfx::Size time_size = time_label_->GetPreferredSize();

  time_label_->SetBounds(time_x, kEntryPadding,
                         time_size.width(), time_size.height());

  // Calculate the position of the favicon.
  int favicon_x = title_x - kFavIconSize - kIconPadding;

  // Now we look to see if the favicon overlaps the time label,
  // and if so, we push the title to the right. If we're not
  // showing the time label, then ignore this step.
  int overlap = favicon_x - (time_x + time_size.width() + kIconPadding);
  if (overlap < 0) {
    title_x -= overlap;
  }

  // Populate and measure the title label.
  const std::wstring& title = model_->GetTitle(model_index_);
  if (!title.empty())
    title_link_->SetText(title);
  else
    title_link_->SetText(l10n_util::GetString(IDS_HISTORY_UNTITLED_TITLE));
  gfx::Size title_size = title_link_->GetPreferredSize();

  // Lay out the title label.
  int max_title_x;

  max_title_x = std::max(0, max_x - title_x);

  if (title_size.width() + kEntryPadding > max_title_x) {
    // We need to shrink the title to make everything fit.
    title_size.set_width(max_title_x - kEntryPadding);
  }
  title_link_->SetBounds(title_x, kEntryPadding,
                         title_size.width(), title_size.height());

  // Lay out the star.
  if (model_->IsStarred(model_index_)) {
    star_toggle_->SetBounds(title_x + title_size.width() + kIconPadding,
                            kEntryPadding, star_size.width(),
                            star_size.height());
    star_toggle_->SetState(true);
    star_toggle_->SetVisible(true);
  } else {
    star_toggle_->SetVisible(false);
  }

  // Lay out the snippet label.
  snippet_label_->SetVisible(show_full_);
  if (show_full_) {
    const Snippet& snippet = model_->GetSnippet(model_index_);
    if (snippet.text().empty()) {
      snippet_label_->SetSnippet(Snippet());  // Bug 843469 will fix this.
    } else {
      snippet_label_->SetSnippet(snippet);
    }
    snippet_label_->SetBounds(title_x,
                              kEntryPadding + snippet_label_->GetLineHeight(),
                              std::min(
                                  static_cast<int>(thumbnail_rect.left -
                                                   title_x),
                                  kMaxSnippetWidth) -
                              kEntryPadding * 2,
                              snippet_label_->GetLineHeight() * 2);
  }
}

int HistoryItemRenderer::GetDragOperations(int x, int y) {
  if (GetDragRegion(x, y) != NONE)
    return DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_LINK;
  return DragDropTypes::DRAG_NONE;
}

void HistoryItemRenderer::WriteDragData(int press_x,
                                        int press_y,
                                        OSExchangeData* data) {
  DCHECK(GetDragOperations(press_x, press_y) != DragDropTypes::DRAG_NONE);

  if (GetDragRegion(press_x, press_y) == FAV_ICON)
    UserMetrics::RecordAction(L"History_DragIcon", model_->profile());
  else
    UserMetrics::RecordAction(L"History_DragThumbnail", model_->profile());

  SkBitmap icon;
  if (model_->GetFavicon(model_index_))
    icon = *model_->GetFavicon(model_index_);

  drag_utils::SetURLAndDragImage(model_->GetURL(model_index_),
                                 model_->GetTitle(model_index_),
                                 icon, data);
}

void HistoryItemRenderer::SetModel(BaseHistoryModel* model, int model_index) {
  DCHECK(model_index < model->GetItemCount());
  model_ = model;
  model_index_ = model_index;
}

void HistoryItemRenderer::SetDisplayStyle(bool show_full) {
  show_full_ = show_full;
}

void HistoryItemRenderer::StarStateChanged(bool state) {
  // Show the user a tip that can be used to edit the bookmark/star.
  gfx::Point star_location;
  views::View::ConvertPointToScreen(star_toggle_, &star_location);
  // Shift the location to make the bubble appear at a visually pleasing
  // location.
  gfx::Rect star_bounds(star_location.x(), star_location.y() + 4,
                        star_toggle_->width(),
                        star_toggle_->height());
  HWND parent = GetWidget()->GetHWND();
  Profile* profile = model_->profile();
  GURL url = model_->GetURL(model_index_);

  if (state) {
    // Only change the star state if the page is not starred. The user can
    // unstar by way of the bubble.
    star_toggle_->SetState(true);
    model_->SetPageStarred(model_index_, true);
  }
  // WARNING: if state is true, we've been deleted.
  BookmarkBubbleView::Show(parent, star_bounds, NULL, profile, url, state);
}

void HistoryItemRenderer::LinkActivated(views::Link* link,
                                        int event_flags) {
  if (link == title_link_) {
    const GURL& url = model_->GetURL(model_index_);
    PageNavigator* navigator = parent_->navigator();
    if (navigator && !url.is_empty()) {
      UserMetrics::RecordAction(L"Destination_History_OpenURL",
                                model_->profile());
      navigator->OpenURL(url, GURL(),
                         event_utils::DispositionFromEventFlags(event_flags),
                         PageTransition::AUTO_BOOKMARK);
      // WARNING: call to OpenURL likely deleted us.
      return;
    }
  }
}

HistoryItemRenderer::DragRegion HistoryItemRenderer::GetDragRegion(int x,
                                                                   int y) {
  // Is the location over the favicon?
  SkBitmap* favicon = model_->GetFavicon(model_index_);
  if (favicon) {
    // If the UI layout is right-to-left, we must make sure we mirror the
    // favicon position before doing any hit testing.
    gfx::Rect favicon_bounds;
    favicon_bounds.set_x(title_link_->x() - kIconPadding - kFavIconSize);
    favicon_bounds.set_y(kEntryPadding);
    favicon_bounds.set_width(favicon->width());
    favicon_bounds.set_height(favicon->height());
    favicon_bounds.set_x(MirroredLeftPointForRect(favicon_bounds));
    if (favicon_bounds.Contains(x, y)) {
      return FAV_ICON;
    }
  }

  // Is it over the thumbnail?
  if (show_full_ && model_->GetThumbnail(model_index_)) {
    CRect thumbnail_loc;
    GetThumbnailBounds(&thumbnail_loc);

    // If the UI layout is right-to-left, we mirror the thumbnail bounds before
    // we check whether or not it contains the point in question.
    gfx::Rect mirrored_loc(thumbnail_loc);
    thumbnail_loc.MoveToX(MirroredLeftPointForRect(mirrored_loc));
    if (gfx::Rect(thumbnail_loc).Contains(x, y))
      return THUMBNAIL;
  }

  return NONE;
}

HistoryView::HistoryView(SearchableUIContainer* container,
                         BaseHistoryModel* model,
                         PageNavigator* navigator)
    : container_(container),
      renderer_(NULL),
      model_(model),
      navigator_(navigator),
      scroll_helper_(this),
      line_height_(-1),
      show_results_(false),
      show_delete_controls_(false),
      delete_control_width_(0),
      loading_(true) {
  DCHECK(model_.get());
  DCHECK(navigator_);
  model_->SetObserver(this);

  ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();
  day_break_font_ = resource_bundle.GetFont(ResourceBundle::WebFont);

  // Ensure break_offsets_ is never empty.
  BreakValue s = {0, 0};
  break_offsets_.insert(std::make_pair(0, s));
}

HistoryView::~HistoryView() {
  if (renderer_)
    delete renderer_;
}

void HistoryView::EnsureRenderer() {
  if (!renderer_)
    renderer_ = new HistoryItemRenderer(this, show_results_);
  if (show_delete_controls_ && !delete_renderer_.get()) {
    delete_renderer_.reset(
        new views::Link(
            l10n_util::GetString(IDS_HISTORY_DELETE_PRIOR_VISITS_LINK)));
    delete_renderer_->SetFont(day_break_font_);
  }
}

int HistoryView::GetLastEntryMaxY() {
  if (break_offsets_.empty())
    return 0;
  BreakOffsets::iterator last_entry_i = break_offsets_.end();
  last_entry_i--;
  return last_entry_i->first;
}

int HistoryView::GetEntryHeight() {
  if (line_height_ == -1) {
    ChromeFont font = ResourceBundle::GetSharedInstance()
        .GetFont(ResourceBundle::WebFont);
    line_height_ = font.height() + font.height() - font.baseline();
  }
  if (show_results_) {
    return std::max(line_height_ * 3 + kEntryPadding, kSearchResultsHeight);
  } else {
    return std::max(line_height_ + kEntryPadding, kBrowseResultsHeight);
  }
}

void HistoryView::ModelChanged(bool result_set_changed) {
  DetachAllFloatingViews();

  if (!result_set_changed) {
    // Only item metadata changed.  We don't need to do a full re-layout,
    // but we may need to redraw the affected items.
    SchedulePaint();
    return;
  }

  // TODO(evanm): this could be optimized by computing break_offsets_ lazily.
  // It'd be especially nice because of our incremental search; right now
  // we recompute the entire layout with each key you press.
  break_offsets_.clear();

  const int count = model_->GetItemCount();

  // If we're not viewing bookmarks and we are looking at search results, then
  // show the items in a results (larger) style.
  show_results_ = model_->IsSearchResults();
  if (renderer_)
    renderer_->SetDisplayStyle(show_results_);

  // If we're viewing bookmarks or we're viewing the larger results, we don't
  // need to insert break offsets between items.
  if (show_results_) {
    BreakValue s = {0, true};
    break_offsets_.insert(std::make_pair(kResultsMargin, s));
    if (count > 0) {
      BreakValue s = {count, true};
      break_offsets_.insert(
          std::make_pair(GetEntryHeight() * count + kResultsMargin, s));
    }
  } else {
    int y = 0;
    Time last_time;
    Time last_day;

    // Loop through our list of items and find places to insert breaks.
    for (int i = 0; i < count; ++i) {
      // NOTE: if you change how we calculate breaks you'll need to update
      // the deletion code as well (DeleteDayAtModelIndex).
      Time time = model_->GetVisitTime(i);
      Time day = time.LocalMidnight();
      if (i == 0 ||
          (last_time - time).ToInternalValue() > kSessionBreakTime ||
          day != last_day) {
        // We've detected something that needs a break.

        bool day_separation = true;

        // If it's not the first item, figure out if it's a day
        // break or session break.
        if (i != 0)
          day_separation = (day != last_day);

        BreakValue s = {i, day_separation};

        break_offsets_.insert(std::make_pair(y, s));
        y += GetBreakOffsetHeight(s);
      }
      last_time = time;
      last_day = day;
      y += GetEntryHeight();
    }

    // Insert ending day.
    BreakValue s = {count, true};
    break_offsets_.insert(std::make_pair(y, s));
  }

  // Find our ScrollView and layout.
  if (GetParent() && GetParent()->GetParent())
    GetParent()->GetParent()->Layout();
}

void HistoryView::ModelBeginWork() {
  loading_ = true;
  if (container_)
    container_->StartThrobber();
}

void HistoryView::ModelEndWork() {
  loading_ = false;
  if (container_)
    container_->StopThrobber();
  if (model_->GetItemCount() == 0)
    SchedulePaint();
}

void HistoryView::SetShowDeleteControls(bool show_delete_controls) {
  if (show_delete_controls == show_delete_controls_)
    return;

  show_delete_controls_ = show_delete_controls;

  delete_renderer_.reset(NULL);

  // Be sure and rebuild the display, otherwise the floating view indices are
  // off.
  ModelChanged(true);
}

int HistoryView::GetPageScrollIncrement(
    views::ScrollView* scroll_view, bool is_horizontal,
    bool is_positive) {
  return scroll_helper_.GetPageScrollIncrement(scroll_view, is_horizontal,
                                               is_positive);
}

int HistoryView::GetLineScrollIncrement(
    views::ScrollView* scroll_view, bool is_horizontal,
    bool is_positive) {
  return scroll_helper_.GetLineScrollIncrement(scroll_view, is_horizontal,
                                               is_positive);
}

views::VariableRowHeightScrollHelper::RowInfo
    HistoryView::GetRowInfo(int y) {
  // Get the time separator header for a given Y click.
  BreakOffsets::iterator i = GetBreakOffsetIteratorForY(y);
  int index = i->second.index;
  int current_y = i->first;

  // Check if the click is on the separator header.
  if (y < current_y + GetBreakOffsetHeight(i->second)) {
    return views::VariableRowHeightScrollHelper::RowInfo(
        current_y, GetBreakOffsetHeight(i->second));
  }

  // Otherwise increment current_y by the item height until it goes past y.
  current_y += GetBreakOffsetHeight(i->second);

  while (index < model_->GetItemCount()) {
    int next_y = current_y + GetEntryHeight();
    if (y < next_y)
      break;
    current_y = next_y;
  }

  // Find the item that corresponds to this new current_y value.
  return views::VariableRowHeightScrollHelper::RowInfo(
      current_y, GetEntryHeight());
}

bool HistoryView::IsVisible() {
  views::Widget* widget = GetWidget();
  return widget && widget->IsVisible();
}

void HistoryView::DidChangeBounds(const gfx::Rect& previous,
                                  const gfx::Rect& current) {
  SchedulePaint();
}

void HistoryView::Layout() {
  DetachAllFloatingViews();

  View* parent = GetParent();
  if (!parent)
    return;

  gfx::Rect bounds = parent->GetLocalBounds(true);

  // If not visible, have zero size so we don't compute anything.
  int width = 0;
  int height = 0;
  if (IsVisible()) {
    width = bounds.width();
    height = std::max(GetLastEntryMaxY(),
                      kEntryPadding + kNoResultTextHeight);
  }

  SetBounds(x(), y(), width, height);
}

HistoryView::BreakOffsets::iterator HistoryView::GetBreakOffsetIteratorForY(
    int y) {
  BreakOffsets::iterator iter = break_offsets_.upper_bound(y);
  DCHECK(iter != break_offsets_.end());
  // Move to the first offset smaller than y.
  if (iter != break_offsets_.begin())
    --iter;
  return iter;
}

int HistoryView::GetBreakOffsetHeight(HistoryView::BreakValue value) {
  if (show_results_)
    return 0;

  if (value.day) {
    return kDayHeadingHeight;
  } else {
    return kSessionBreakHeight;
  }
}

void HistoryView::Paint(ChromeCanvas* canvas) {
  views::View::Paint(canvas);

  EnsureRenderer();

  SkRect clip;
  if (!canvas->getClipBounds(&clip))
    return;

  const int content_width = width() - kLeftMargin - kRightMargin;

  const int x1 = kLeftMargin;
  int clip_y = SkScalarRound(clip.fTop);
  int clip_max_y = SkScalarRound(clip.fBottom);

  if (model_->GetItemCount() == 0) {
    // Display text indicating that no results were found.
    int result_id;

    if (loading_)
      result_id = IDS_HISTORY_LOADING;
    else if (show_results_)
      result_id = IDS_HISTORY_NO_RESULTS;
    else
      result_id = IDS_HISTORY_NO_ITEMS;

    canvas->DrawStringInt(l10n_util::GetString(result_id),
                          day_break_font_,
                          SkColorSetRGB(0, 0, 0),
                          x1, kEntryPadding,
                          std::max(content_width, kNoResultMinWidth),
                          kNoResultTextHeight,
                          ChromeCanvas::MULTI_LINE);
  }

  if (clip_y >= GetLastEntryMaxY())
    return;

  BreakOffsets::iterator break_offsets_iter =
      GetBreakOffsetIteratorForY(clip_y);
  int item_index = break_offsets_iter->second.index;
  int y = break_offsets_iter->first;

  // Display the "Search results for 'xxxx'" text.
  if (show_results_ && model_->GetItemCount() > 0) {
    canvas->DrawStringInt(l10n_util::GetStringF(IDS_HISTORY_SEARCH_STRING,
                              model_->GetSearchText()),
                          day_break_font_,
                          SkColorSetRGB(0, 0, 0),
                          x1, kEntryPadding,
                          content_width, kResultTextHeight,
                          ChromeCanvas::TEXT_VALIGN_BOTTOM);
  }

  Time midnight_today = Time::Now().LocalMidnight();
  while (y < clip_max_y && item_index < model_->GetItemCount()) {
    if (!show_results_ && y == break_offsets_iter->first) {
      if (y + kDayHeadingHeight > clip_y) {
        if (break_offsets_iter->second.day) {
          // We're at a day break, draw the day break appropriately.
          Time visit_time = model_->GetVisitTime(item_index);
          DCHECK(visit_time.ToInternalValue() > 0);

          // If it's the first day, then it has a special presentation.
          std::wstring date_str = TimeFormat::RelativeDate(visit_time,
                                                           &midnight_today);
          if (date_str.empty()) {
            date_str = base::TimeFormatFriendlyDate(visit_time);
          } else {
            date_str = l10n_util::GetStringF(
                IDS_HISTORY_DATE_WITH_RELATIVE_TIME,
                date_str, base::TimeFormatFriendlyDate(visit_time));
          }

          // Draw date
          canvas->DrawStringInt(date_str,
                                day_break_font_,
                                SkColorSetRGB(0, 0, 0),
                                x1, y + kDayHeadingHeight -
                                kBrowseResultsHeight + kEntryPadding,
                                content_width, kBrowseResultsHeight,
                                ChromeCanvas::TEXT_VALIGN_BOTTOM);

          // Draw delete controls.
          if (show_delete_controls_) {
            gfx::Rect delete_bounds = CalculateDeleteControlBounds(y);
            if (!HasFloatingViewForPoint(delete_bounds.x(),
                                         delete_bounds.y())) {
              PaintFloatingView(canvas, delete_renderer_.get(),
                                delete_bounds.x(), delete_bounds.y(),
                                delete_bounds.width(), delete_bounds.height());
            }
          }
        } else {
          // Draw session separator. Note that we must mirror the position of
          // the separator if we run in an RTL locale because we draw the
          // separator directly on the canvas.
          gfx::Rect separator_bounds(x1 + kSessionGapOffset + kTimeOffset,
                                     y,
                                     1,
                                     kBrowseResultsHeight);
          separator_bounds.set_x(MirroredLeftPointForRect(separator_bounds));
          canvas->FillRectInt(SkColorSetRGB(178, 178, 178),
                              separator_bounds.x(), separator_bounds.y(),
                              separator_bounds.width(),
                              separator_bounds.height());
        }
      }

      y += GetBreakOffsetHeight(break_offsets_iter->second);
    }

    if (y + GetEntryHeight() > clip_y && !HasFloatingViewForPoint(x1, y)) {
      renderer_->SetModel(model_.get(), item_index);
      PaintFloatingView(canvas, renderer_, x1, y, content_width,
                        GetEntryHeight());
    }

    y += GetEntryHeight();

    BreakOffsets::iterator next_break_offsets = break_offsets_iter;
    ++next_break_offsets;
    if (next_break_offsets != break_offsets_.end() &&
        y >= next_break_offsets->first) {
      break_offsets_iter = next_break_offsets;
    }

    ++item_index;
  }
}

int HistoryView::GetYCoordinateForViewID(int id,
                                         int* model_index,
                                         bool* is_delete_control) {
  DCHECK(id < GetMaxViewID());

  // Loop through our views and figure out model ids and y coordinates
  // of the various items as we go until we find the item that matches.
  // the supplied id. This should closely match the code in Paint().
  //
  // Watch out, this will be is_null when there is no visit.
  Time last_time = model_->GetVisitTime(0);

  int current_model_index = 0;
  int y = show_results_ ? kResultsMargin : 0;

  bool show_breaks = !show_results_;

  for (int i = 0; i <= id; i++) {
    // Consider day and session breaks also between when moving between groups
    // of unvisited (visit_time().is_null()) and visited URLs.
    Time time = model_->GetVisitTime(current_model_index);
    bool at_day_break = last_time.is_null() != time.is_null() ||
        (i == 0 || last_time.LocalMidnight() != time.LocalMidnight());
    bool at_session_break = last_time.is_null() != time.is_null() ||
        (!at_day_break &&
         (last_time - time).ToInternalValue() > kSessionBreakTime);
    bool at_result = (i == id);

    // If we're showing breaks, are a day break and are showing delete
    // controls, this view id must be a delete control.
    if (show_breaks && at_day_break && show_delete_controls_) {
      if (at_result) {
        // We've found what we're looking for.
        *is_delete_control = true;
        *model_index = current_model_index;
        return y;
      } else {
        // This isn't what we're looking for, but it is a valid view, so carry
        // on through the loop, but don't increment our current_model_index,
        // as the next view will have the same model index.
        y += kDayHeadingHeight;
        last_time = time;
      }
    } else {
      if (show_breaks) {
        if (at_day_break) {
          y += kDayHeadingHeight;
        } else if (at_session_break) {
          y += kSessionBreakHeight;
        }
      }

      // We're on a result item.
      if (at_result) {
        *is_delete_control = false;
        *model_index = current_model_index;
        return y;
      }

      // It wasn't the one we're looking for, so increment our y coordinate and
      // model index and move on to the next view.
      current_model_index++;
      last_time = time;
      y += GetEntryHeight();
    }
  }

  return y;
}

bool HistoryView::GetFloatingViewIDForPoint(int x, int y, int* id) {
  // Here's a picture of the various offsets used here.
  // Let the (*) on entry #5 below represent the mouse position.
  //  +--------------
  //  | entry #2
  //  +--------------
  //                   <- base_y is the y coordinate of the break.
  //  +--------------  <- break_offsets->second.index points at this entry
  //  | entry #3          base_index is this entry index (3).
  //  +--------------
  //  +--------------
  //  | entry #4
  //  +--------------
  //  +--------------
  //  | entry #5 (*)   <- y is this y coordinate
  //  +--------------

  // First, verify the x coordinate is within the correct region.
  if (x < kLeftMargin || x > width() - kRightMargin ||
      y >= GetLastEntryMaxY()) {
    return false;
  }

  // Find the closest break to this y-coordinate.
  BreakOffsets::const_iterator break_offsets_iter =
      GetBreakOffsetIteratorForY(y);

  // Get the model index of the first item after that break.
  int base_index = break_offsets_iter->second.index;

  // Get the view id of that item by adding the number of deletes prior to
  // this item. (See comments for break_offsets_);
  if (show_delete_controls_) {
    base_index += CalculateDeleteOffset(break_offsets_iter);

    // The current break contains a delete, we need to account for that.
    if (break_offsets_iter->second.day)
      base_index++;
  }

  // base_y is the top of the break block.
  int base_y = break_offsets_iter->first;

  // Add the height of the break.
  if (!show_results_)
    base_y += GetBreakOffsetHeight(break_offsets_iter->second);

  // If y is less than base_y, then it must be over the break and so the
  // only view the mouse could be over would be the delete link.
  if (y < base_y) {
    if (show_delete_controls_ &&
        break_offsets_iter->second.day) {
      gfx::Rect delete_bounds =
          CalculateDeleteControlBounds(base_y - kDayHeadingHeight);

      // The delete link bounds must be mirrored if the locale is RTL since the
      // point we check against is in LTR coordinates.
      delete_bounds.set_x(MirroredLeftPointForRect(delete_bounds));
      if (x >= delete_bounds.x() && x < delete_bounds.right()) {
        *id = base_index - 1;
        return true;
      }
    }
    return false;  // Point is over the day heading.
  }

  // y_delta is the distance from the top of the first item in
  // this block to the target y point.
  const int y_delta = y - base_y;

  int view_id = base_index + (y_delta / GetEntryHeight());
  *id = view_id;
  return true;
}

bool HistoryView::EnumerateFloatingViews(
    views::View::FloatingViewPosition position,
    int starting_id,
    int* id) {
  DCHECK(id);
  return View::EnumerateFloatingViewsForInterval(0, GetMaxViewID(),
                                                 true,
                                                 position, starting_id, id);
}

views::View* HistoryView::ValidateFloatingViewForID(int id) {
  if (id >= GetMaxViewID())
    return NULL;

  bool is_delete_control;
  int model_index;
  View* floating_view;

  int y = GetYCoordinateForViewID(id, &model_index, &is_delete_control);
  if (is_delete_control) {
    views::Link* delete_link = new views::Link(
        l10n_util::GetString(IDS_HISTORY_DELETE_PRIOR_VISITS_LINK));
    delete_link->SetID(model_index);
    delete_link->SetFont(day_break_font_);
    delete_link->SetController(this);

    gfx::Rect delete_bounds = CalculateDeleteControlBounds(y);
    delete_link->SetBounds(delete_bounds.x(), delete_bounds.y(),
                           delete_bounds.width(), delete_bounds.height());
    floating_view = delete_link;
  } else {
    HistoryItemRenderer* renderer =
        new HistoryItemRenderer(this,
                                show_results_);
    renderer->SetModel(model_.get(), model_index);
    renderer->SetBounds(kLeftMargin, y,
                        width() - kLeftMargin - kRightMargin,
                        GetEntryHeight());
    floating_view = renderer;
  }
  floating_view->Layout();
  AttachFloatingView(floating_view, id);

#ifdef DEBUG_FLOATING_VIEWS
  floating_view->SetBackground(views::Background::CreateSolidBackground(
                               SkColorSetRGB(255, 0, 0)));
  floating_view->SchedulePaint();
#endif
  return floating_view;
}

int HistoryView::GetMaxViewID() {
  if (!show_delete_controls_)
    return model_->GetItemCount();

  // Figure out how many delete controls we are displaying.
  int deletes = 0;
  for (BreakOffsets::iterator i = break_offsets_.begin();
       i != break_offsets_.end(); ++i) {
    if (i->second.day)
      deletes++;
  }

  // Subtract one because we don't display a delete control at the end.
  deletes--;

  return std::max(0, deletes + model_->GetItemCount());
}

void HistoryView::LinkActivated(views::Link* source, int event_flags) {
  DeleteDayAtModelIndex(source->GetID());
}

void HistoryView::DeleteDayAtModelIndex(int index) {
  std::wstring text = l10n_util::GetString(
      IDS_HISTORY_DELETE_PRIOR_VISITS_WARNING);
  std::wstring caption = l10n_util::GetString(
      IDS_HISTORY_DELETE_PRIOR_VISITS_WARNING_TITLE);
  UINT flags = MB_OKCANCEL | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND;

  if (win_util::MessageBox(GetWidget()->GetHWND(),
                           text, caption, flags) !=  IDOK) {
    return;
  }

  if (index < 0 || index >= model_->GetItemCount()) {
    // Bogus index.
    NOTREACHED();
    return;
  }

  UserMetrics::RecordAction(L"History_DeleteHistory", model_->profile());

  // BrowsingDataRemover deletes itself when done.
  // index refers to the last page at the very end of the day, so we delete
  // everything after the start of the day and before the end of the day.
  Time delete_begin = model_->GetVisitTime(index).LocalMidnight();
  Time delete_end =
      (model_->GetVisitTime(index) + TimeDelta::FromDays(1)).LocalMidnight();

  BrowsingDataRemover* remover =
      new BrowsingDataRemover(model_->profile(),
                              delete_begin,
                              delete_end);
  remover->Remove(BrowsingDataRemover::REMOVE_HISTORY |
                  BrowsingDataRemover::REMOVE_COOKIES |
                  BrowsingDataRemover::REMOVE_CACHE);

  model_->Refresh();

  // Scroll to the origin, otherwise the scroll position isn't changed and the
  // user is left looking at a region they originally weren't viewing.
  ScrollRectToVisible(0, 0, 0, 0);
}

int HistoryView::CalculateDeleteOffset(
    const BreakOffsets::const_iterator& it) {
  DCHECK(show_delete_controls_);
  int offset = 0;
  for (BreakOffsets::iterator i = break_offsets_.begin(); i != it; ++i) {
    if (i->second.day)
      offset++;
  }
  return offset;
}

int HistoryView::GetDeleteControlWidth() {
  if (delete_control_width_)
    return delete_control_width_;
  EnsureRenderer();
  gfx::Size pref = delete_renderer_->GetPreferredSize();
  delete_control_width_ = pref.width();
  return delete_control_width_;
}

gfx::Rect HistoryView::CalculateDeleteControlBounds(int base_y) {
  // NOTE: the height here is too big, it should be just big enough to show
  // the link. Additionally this should be baseline aligned with the date. I'm
  // not doing that now as a redesign of HistoryView is in the works.
  const int delete_width = GetDeleteControlWidth();
  const int delete_x = width() - kRightMargin - delete_width;
  return gfx::Rect(delete_x,
                   base_y + kDeleteControlOffset,
                   delete_width,
                   kBrowseResultsHeight);
}
