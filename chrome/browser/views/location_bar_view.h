// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_LOCATION_BAR_VIEW_H_
#define CHROME_BROWSER_VIEWS_LOCATION_BAR_VIEW_H_

#include <string>
#include <vector>

#include "app/gfx/font.h"
#include "base/gfx/rect.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/browser/views/info_bubble.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/native/native_view_host.h"
#include "views/painter.h"

#if defined(OS_WIN)
#include "chrome/browser/autocomplete/autocomplete_edit_view_win.h"
#else
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#endif

class AutocompletePopupPositioner;
class CommandUpdater;
class GURL;
class PageAction;
class Profile;

/////////////////////////////////////////////////////////////////////////////
//
// LocationBarView class
//
//   The LocationBarView class is a View subclass that paints the background
//   of the URL bar strip and contains its content.
//
/////////////////////////////////////////////////////////////////////////////
class LocationBarView : public LocationBar,
                        public LocationBarTesting,
                        public views::View,
                        public AutocompleteEditController {
 public:
  class Delegate {
   public:
    // Should return the current tab contents.
    virtual TabContents* GetTabContents() = 0;

    // Called by the location bar view when the user starts typing in the edit.
    // This forces our security style to be UNKNOWN for the duration of the
    // editing.
    virtual void OnInputInProgress(bool in_progress) = 0;
  };

  LocationBarView(Profile* profile,
                  CommandUpdater* command_updater,
                  ToolbarModel* model,
                  Delegate* delegate,
                  bool popup_window_mode,
                  AutocompletePopupPositioner* popup_positioner);
  virtual ~LocationBarView();

  void Init();

  // Returns whether this instance has been initialized by callin Init. Init can
  // only be called when the receiving instance is attached to a view container.
  bool IsInitialized() const;

  // Updates the location bar.  We also reset the bar's permanent text and
  // security style, and, if |tab_for_state_restoring| is non-NULL, also restore
  // saved state that the tab holds.
  void Update(const TabContents* tab_for_state_restoring);

  void SetProfile(Profile* profile);
  Profile* profile() { return profile_; }

  // Sizing functions
  virtual gfx::Size GetPreferredSize();

  // Layout and Painting functions
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);

  // No focus border for the location bar, the caret is enough.
  virtual void PaintFocusBorder(gfx::Canvas* canvas) { }

  // Called when any ancestor changes its size, asks the AutocompleteEditModel
  // to close its popup.
  virtual void VisibleBoundsInRootChanged();


#if defined(OS_WIN)
  // Event Handlers
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual bool OnMouseDragged(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
#endif

  // AutocompleteEditController
  virtual void OnAutocompleteAccept(const GURL& url,
                                    WindowOpenDisposition disposition,
                                    PageTransition::Type transition,
                                    const GURL& alternate_nav_url);
  virtual void OnChanged();
  virtual void OnInputInProgress(bool in_progress) {
    delegate_->OnInputInProgress(in_progress);
  }
  virtual SkBitmap GetFavIcon() const;
  virtual std::wstring GetTitle() const;

  // Returns the accessibility role.
  bool GetAccessibleRole(AccessibilityTypes::Role* role);

  // Overridden from views::View:
  virtual bool SkipDefaultKeyEventProcessing(const views::KeyEvent& e);

  // Overridden from LocationBar:
  virtual void ShowFirstRunBubble(bool use_OEM_bubble);
  virtual std::wstring GetInputString() const;
  virtual WindowOpenDisposition GetWindowOpenDisposition() const;
  virtual PageTransition::Type GetPageTransition() const;
  virtual void AcceptInput();
  virtual void AcceptInputWithDisposition(WindowOpenDisposition);
  virtual void FocusLocation();
  virtual void FocusSearch();
  virtual void UpdatePageActions();
  virtual void SaveStateToContents(TabContents* contents);
  virtual void Revert();
  virtual AutocompleteEditView* location_entry() {
    return location_entry_.get();
  }
  virtual LocationBarTesting* GetLocationBarForTesting() { return this; }

  // Overridden from LocationBarTesting:
  virtual int PageActionVisibleCount();

  static const int kVertMargin;
  static const SkColor kBackgroundColorByLevel[];

 protected:
  void Focus();

 private:
  // View used when the user has selected a keyword.
  //
  // SelectedKeywordView maintains two labels. One label contains the
  // complete description of the keyword, the second contains a truncated
  // version of the description. The second is used if there is not enough room
  // to display the complete description.
  class SelectedKeywordView : public views::View {
   public:
    explicit SelectedKeywordView(Profile* profile);
    virtual ~SelectedKeywordView();

    void SetFont(const gfx::Font& font);

    virtual void Paint(gfx::Canvas* canvas);

    virtual gfx::Size GetPreferredSize();
    virtual gfx::Size GetMinimumSize();
    virtual void Layout();

    // The current keyword, or an empty string if no keyword is displayed.
    void SetKeyword(const std::wstring& keyword);
    std::wstring keyword() const { return keyword_; }

    void set_profile(Profile* profile) { profile_ = profile; }

   private:
    // Returns the truncated version of description to use.
    std::wstring CalculateMinString(const std::wstring& description);

    // The keyword we're showing. If empty, no keyword is selected.
    // NOTE: we don't cache the TemplateURL as it is possible for it to get
    // deleted out from under us.
    std::wstring keyword_;

    // For painting the background.
    views::HorizontalPainter background_painter_;

    // Label containing the complete description.
    views::Label full_label_;

    // Label containing the partial description.
    views::Label partial_label_;

    Profile* profile_;

    DISALLOW_COPY_AND_ASSIGN(SelectedKeywordView);
  };

  // KeywordHintView is used to display a hint to the user when the selected
  // url has a corresponding keyword.
  //
  // Internally KeywordHintView uses two labels to render the text, and draws
  // the tab image itself.
  //
  // NOTE: This should really be called LocationBarKeywordHintView, but I
  // couldn't bring myself to use such a long name.
  class KeywordHintView : public views::View {
   public:
    explicit KeywordHintView(Profile* profile);
    virtual ~KeywordHintView();

    void SetFont(const gfx::Font& font);

    void SetColor(const SkColor& color);

    void SetKeyword(const std::wstring& keyword);
    std::wstring keyword() const { return keyword_; }

    virtual void Paint(gfx::Canvas* canvas);
    virtual gfx::Size GetPreferredSize();
    // The minimum size is just big enough to show the tab.
    virtual gfx::Size GetMinimumSize();
    virtual void Layout();

    void set_profile(Profile* profile) { profile_ = profile; }

   private:
    views::Label leading_label_;
    views::Label trailing_label_;

    // The keyword.
    std::wstring keyword_;

    Profile* profile_;

    DISALLOW_COPY_AND_ASSIGN(KeywordHintView);
  };


  class ShowInfoBubbleTask;
  class ShowFirstRunBubbleTask;

  class LocationBarImageView : public views::ImageView,
                               public InfoBubbleDelegate  {
  public:
    LocationBarImageView();
    virtual ~LocationBarImageView();

    // Overridden from view for the mouse hovering.
    virtual void OnMouseMoved(const views::MouseEvent& event);
    virtual void OnMouseExited(const views::MouseEvent& event);
    virtual bool OnMousePressed(const views::MouseEvent& event) = 0;

    // InfoBubbleDelegate
    void InfoBubbleClosing(InfoBubble* info_bubble, bool closed_by_escape);
    bool CloseOnEscape() { return true; }

    virtual void ShowInfoBubble() = 0;

  protected:
    void ShowInfoBubbleImpl(const std::wstring& text, SkColor text_color);

  private:
    friend class ShowInfoBubbleTask;

    // The currently shown info bubble if any.
    InfoBubble* info_bubble_;

    // A task used to display the info bubble when the mouse hovers on the
    // image.
    ShowInfoBubbleTask* show_info_bubble_task_;

    DISALLOW_COPY_AND_ASSIGN(LocationBarImageView);
  };

  // SecurityImageView is used to display the lock or warning icon when the
  // current URL's scheme is https.
  //
  // If a message has been set with SetInfoBubbleText, it displays an info
  // bubble when the mouse hovers on the image.
  class SecurityImageView : public LocationBarImageView {
   public:
    enum Image {
      LOCK = 0,
      WARNING
    };

    SecurityImageView(Profile* profile, ToolbarModel* model_);
    virtual ~SecurityImageView();

    // Sets the image that should be displayed.
    void SetImageShown(Image image);

    // Overridden from view for the mouse hovering.
    virtual bool OnMousePressed(const views::MouseEvent& event);

    void set_profile(Profile* profile) { profile_ = profile; }

    virtual void ShowInfoBubble();

   private:
    // The lock icon shown when using HTTPS.
    static SkBitmap* lock_icon_;

    // The warning icon shown when HTTPS is broken.
    static SkBitmap* warning_icon_;

    // The currently shown info bubble if any.
    InfoBubble* info_bubble_;

    // A task used to display the info bubble when the mouse hovers on the
    // image.
    ShowInfoBubbleTask* show_info_bubble_task_;

    Profile* profile_;

    ToolbarModel* model_;

    DISALLOW_COPY_AND_ASSIGN(SecurityImageView);
  };

  // PageActionImageView is used to display the icon for a given PageAction
  // and notify the extension when the icon is clicked.
  class PageActionImageView : public LocationBarImageView {
  public:
    PageActionImageView(
        LocationBarView* owner, Profile* profile,
        const PageAction* page_action);
    virtual ~PageActionImageView();

    // Overridden from view for the mouse hovering.
    virtual bool OnMousePressed(const views::MouseEvent& event);

    // Overridden from LocationBarImageView.
    virtual void ShowInfoBubble();

    // Called to notify the PageAction that it should determine whether to be
    // visible or hidden. |contents| is the TabContents that is active, |url|
    // is the current page URL.
    void UpdateVisibility(TabContents* contents, GURL url);

    // A callback for when the image has loaded.
    void OnImageLoaded(SkBitmap* image, size_t index);

  private:
    // We load the images for the PageActions on the file thread. These tasks
    // help with that.
    class LoadImageTask;
    class ImageLoadingTracker;

    // The location bar view that owns us.
    LocationBarView* owner_;

    // The current profile (not owned by us).
    Profile* profile_;

    // The PageAction that this view represents. The PageAction is not owned by
    // us, it resides in the extension of this particular profile.
    const PageAction* page_action_;

    // The icons representing different states for the page action.
    std::vector<SkBitmap> page_action_icons_;

    // The object that is waiting for the image loading to complete
    // asynchronously.
    ImageLoadingTracker* tracker_;

    // The tab id we are currently showing the icon for.
    int current_tab_id_;

    // The URL we are currently showing the icon for.
    GURL current_url_;

    // The string to show for a tooltip;
    std::string tooltip_;

    DISALLOW_COPY_AND_ASSIGN(PageActionImageView);
  };

  // Both Layout and OnChanged call into this. This updates the contents
  // of the 3 views: selected_keyword, keyword_hint and type_search_view. If
  // force_layout is true, or one of these views has changed in such a way as
  // to necessitate a layout, layout occurs as well.
  void DoLayout(bool force_layout);

  // Returns the height in pixels of the margin at the top of the bar.
  int TopMargin() const;

  // Returns the width in pixels of the contents of the edit.
  int TextDisplayWidth();

  // Returns true if the preferred size should be used for a view whose width
  // is pref_width, the width of the text in the edit is text_width, and
  // max_width is the maximum width of the edit. If this returns false, the
  // minimum size of the view should be used.
  bool UsePref(int pref_width, int text_width, int max_width);

  // Returns true if the view needs to be resized. This determines whether the
  // min or pref should be used, and returns true if the view is not at that
  // size.
  bool NeedsResize(View* view, int text_width, int max_width);

  // Adjusts the keyword hint, selected keyword and type to search views
  // based on the contents of the edit. Returns true if something changed that
  // necessitates a layout.
  bool AdjustHints(int text_width, int max_width);

  // If View fits in the specified region, it is made visible and the
  // bounds are adjusted appropriately. If the View does not fit, it is
  // made invisible.
  void LayoutView(bool leading, views::View* view, int text_width,
                  int max_width, gfx::Rect* bounds);

  // Sets the security icon to display.  Note that no repaint is done.
  void SetSecurityIcon(ToolbarModel::Icon icon);

  // Delete all page action views that we have created.
  void DeletePageActionViews();

  // Retrieves a vector of all page actions, irrespective of which
  // extension they belong to.
  std::vector<PageAction*> GetPageActions();

  // Update the views for the Page Actions, to reflect state changes for
  // PageActions.
  void RefreshPageActionViews();

  // Sets the text that should be displayed in the info label and its associated
  // tooltip text.  Call with an empty string if the info label should be
  // hidden.
  void SetInfoText(const std::wstring& text,
                   ToolbarModel::InfoTextType text_type,
                   const std::wstring& tooltip_text);

  // Sets the visibility of view to new_vis. Returns whether the visibility
  // changed.
  bool ToggleVisibility(bool new_vis, views::View* view);

#if defined(OS_WIN)
  // Helper for the Mouse event handlers that does all the real work.
  void OnMouseEvent(const views::MouseEvent& event, UINT msg);
#endif

  // Helper to show the first run info bubble.
  void ShowFirstRunBubbleInternal(bool use_OEM_bubble);

  // Current profile. Not owned by us.
  Profile* profile_;

  // The Autocomplete Edit field.
#if defined(OS_WIN)
  scoped_ptr<AutocompleteEditViewWin> location_entry_;
#else
  scoped_ptr<AutocompleteEditViewGtk> location_entry_;
#endif

  // The CommandUpdater for the Browser object that corresponds to this View.
  CommandUpdater* command_updater_;

  // The model.
  ToolbarModel* model_;

  // Our delegate.
  Delegate* delegate_;

  // This is the string of text from the autocompletion session that the user
  // entered or selected.
  std::wstring location_input_;

  // The user's desired disposition for how their input should be opened
  WindowOpenDisposition disposition_;

  // The transition type to use for the navigation
  PageTransition::Type transition_;

  // Font used by edit and some of the hints.
  gfx::Font font_;

  // Location_entry view wrapper
  views::NativeViewHost* location_entry_view_;

  // The following views are used to provide hints and remind the user as to
  // what is going in the edit. They are all added a children of the
  // LocationBarView. At most one is visible at a time. Preference is
  // given to the keyword_view_, then hint_view_, then type_to_search_view_.

  // Shown if the user has selected a keyword.
  SelectedKeywordView selected_keyword_view_;

  // Shown if the selected url has a corresponding keyword.
  KeywordHintView keyword_hint_view_;

  // Shown if the text is not a keyword or url.
  views::Label type_to_search_view_;

  // The view that shows the lock/warning when in HTTPS mode.
  SecurityImageView security_image_view_;

  // The page action icon views.
  std::vector<PageActionImageView*> page_action_image_views_;

  // A label displayed after the lock icon to show some extra information.
  views::Label info_label_;

  // When true, the location bar view is read only and also is has a slightly
  // different presentation (font size / color). This is used for popups.
  bool popup_window_mode_;

  // Used schedule a task for the first run info bubble.
  ScopedRunnableMethodFactory<LocationBarView> first_run_bubble_;

  // The positioner that places the autocomplete popup.
  AutocompletePopupPositioner* popup_positioner_;
};

#endif  // CHROME_BROWSER_VIEWS_LOCATION_BAR_VIEW_H_
