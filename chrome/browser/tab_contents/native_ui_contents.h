// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_NATIVE_UI_CONTENTS_H_
#define CHROME_BROWSER_TAB_CONTENTS_NATIVE_UI_CONTENTS_H_

#include "chrome/browser/page_state.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/views/background.h"
#include "chrome/views/link.h"
#include "chrome/views/native_button.h"
#include "chrome/views/text_field.h"
#include "chrome/views/widget_win.h"

namespace views {
class CheckBox;
class FocusTraversable;
class ImageView;
class ScrollView;
class Throbber;
}

class NativeUIFactory;
class NativeUI;

////////////////////////////////////////////////////////////////////////////////
//
// NativeUIContents
//
// NativeUIContents is a TabContents that is used to show some pages made with
// some native user interface elements. NativeUIContents maintains a list of URL
// path mapping to specific NativeUI implementations.
//
////////////////////////////////////////////////////////////////////////////////
class NativeUIContents : public TabContents,
                         public views::WidgetWin {
 public:
  explicit NativeUIContents(Profile* profile);

  virtual void CreateView();
  virtual gfx::NativeView GetNativeView() const { return GetHWND(); }
  virtual void GetContainerBounds(gfx::Rect* out) const;

  // Sets the page state. NativeUIContents takes ownership of the supplied
  // PageState. Use a value of NULL to set the state to empty.
  void SetPageState(PageState* page_state);

  // Returns the page state. This is intended for UIs that want to store page
  // state.
  const PageState& page_state() const { return *state_; }

  //
  // TabContents implementation
  //
  virtual bool NavigateToPendingEntry(bool reload);
  virtual const std::wstring GetDefaultTitle() const;
  virtual SkBitmap GetFavIcon() const;
  virtual bool ShouldDisplayURL() { return false; }
  virtual bool ShouldDisplayFavIcon() { return true; }
  virtual void DidBecomeSelected();
  virtual void SetInitialFocus();

  // Sets the current loading state. This is public for NativeUIs to update.
  void SetIsLoading(bool is_loading, LoadNotificationDetails* details);

  // FocusTraversable Implementation
  virtual views::View* FindNextFocusableView(
      views::View* starting_view,
      bool reverse,
      views::FocusTraversable::Direction direction,
      bool dont_loop,
      views::FocusTraversable** focus_traversable,
      views::View** focus_traversable_view);
  virtual views::RootView* GetContentsRootView() { return GetRootView(); }

  // Return the scheme used. We currently use chrome-nativeui:
  static std::string GetScheme();

  // Register a NativeUIFactory for a given path.
  static void RegisterNativeUIFactory(const GURL& url,
                                      NativeUIFactory* factory);

 protected:
  // Should be deleted via CloseContents.
  virtual ~NativeUIContents();

  // Overridden to create a view that that handles drag and drop.
  virtual views::RootView* CreateRootView();

 private:
  // Initialize the factories. This is called the first time a NativeUIContents
  // object is created. If you add a new factory, you need to add a line in this
  // method.
  static void InitializeNativeUIFactories();

  // Instantiates a native UI for the provided URL. This is done by using the
  // native factories which have been registered.
  static NativeUI* InstantiateNativeUIForURL(const GURL& url,
                                             NativeUIContents* contents);

  // Returns the key to use based on the TabUI's url.
  static std::string GetFactoryKey(const GURL& url);

  // Size the current UI if any.
  void Layout();

  // Return the Native UI for the provided URL. The NativeUIs are returned from
  // a cache. Returns NULL if no such UI exists.
  NativeUI* GetNativeUIForURL(const GURL& url);

  // Windows message handlers.
  virtual LRESULT OnCreate(LPCREATESTRUCT create_struct);
  virtual void OnDestroy();
  virtual void OnSize(UINT size_command, const CSize& new_size);
  virtual void OnWindowPosChanged(WINDOWPOS* position);

  // Whether this contents is visible.
  bool is_visible_;

  // Path to NativeUI map. We keep reusing the same UIs.
  typedef std::map<std::string, NativeUI*> PathToUI;
  PathToUI path_to_native_uis_;

  // The current UI.
  NativeUI* current_ui_;

  // The current view for the current UI. We don't ask again just in case the
  // UI implementation keeps allocating new uis.
  views::View* current_view_;

  // The current page state for the native contents.
  scoped_ptr<PageState> state_;

  // Whether factories have been initialized.
  static bool g_ui_factories_initialized;

  DISALLOW_EVIL_CONSTRUCTORS(NativeUIContents);
};

/////////////////////////////////////////////////////////////////////////////
//
// A native UI needs to implement the following interface to work with the
// NativeUIContents.
//
/////////////////////////////////////////////////////////////////////////////
class NativeUI {
 public:
  virtual ~NativeUI() {}

  // Return the title for this user interface. The title is used as a tab title.
  virtual const std::wstring GetTitle() const = 0;

  // Return the favicon id for this user interface.
  virtual const int GetFavIconID () const = 0;

  // Return the view that should be used to render this user interface.
  virtual views::View* GetView() = 0;

  // Inform the view that it is about to become visible.
  virtual void WillBecomeVisible(NativeUIContents* parent) = 0;

  // Inform the view that it is about to become invisible.
  virtual void WillBecomeInvisible(NativeUIContents* parent) = 0;

  // Inform the view that it should recreate the provided state. The state
  // should be updated as needed by using the current navigation entry of
  // the provided tab contents.
  virtual void Navigate(const PageState& state) = 0;

  // Requests the contents set the initial focus. A return value of true
  // indicates the contents wants focus and requested focus. A return value of
  // false indicates the contents does not want focus, and that focus should
  // go to the location bar.
  virtual bool SetInitialFocus() = 0;
};

/////////////////////////////////////////////////////////////////////////////
//
// NativeUIFactory defines the method necessary to instantiate a NativeUI
// object. Typically, each NativeUI implementation registers an object that
// can instantiate NativeUI objects given the necessary path.
//
/////////////////////////////////////////////////////////////////////////////
class NativeUIFactory {
 public:
  virtual ~NativeUIFactory() {}

  // Request the factory to instantiate a NativeUI object given the provided
  // url. The url is a nativeui: URL which contains the path for which this
  // factory was registered.
  //
  // See NativeUIContents::RegisterNativeUI().
  virtual NativeUI* CreateNativeUIForURL(const GURL& url,
                                         NativeUIContents* contents) = 0;
};


////////////////////////////////////////////////////////////////////////////////
//
// A standard background for native UIs.
//
////////////////////////////////////////////////////////////////////////////////
class NativeUIBackground : public views::Background {
 public:
  NativeUIBackground();
  virtual ~NativeUIBackground();

  virtual void Paint(ChromeCanvas* canvas, views::View* view) const;

 private:

  DISALLOW_EVIL_CONSTRUCTORS(NativeUIBackground);
};

////////////////////////////////////////////////////////////////////////////////
//
// A view subclass used to implement native uis that feature a search field.
// This view contains a search field and a ScrollView for the contents. It
// implements a consistent look for these UIs.
//
////////////////////////////////////////////////////////////////////////////////
class SearchableUIContainer : public views::View,
                              public views::NativeButton::Listener,
                              public views::LinkController,
                              public views::TextField::Controller {
 public:
  // The Delegate is notified when the user clicks the search button.
  class Delegate {
   public:
    virtual void DoSearch(const std::wstring& text) = 0;
    virtual const std::wstring GetTitle() const = 0;
    virtual const int GetSectionIconID() const = 0;
    virtual const std::wstring GetSearchButtonText() const = 0;
  };

  // Create a new SearchableUIContainer given a delegate.
  explicit SearchableUIContainer(Delegate* delegate);

  virtual ~SearchableUIContainer();

  // Add the view as the contents of the container.
  void SetContents(views::View* contents);
  views::View* GetContents();

  virtual void Layout();

  // Overriden to paint the container.
  virtual void Paint(ChromeCanvas* canvas);

  // Provide the mode access to various UI elements.
  views::TextField* GetSearchField() const;
  views::ScrollView* GetScrollView() const;

  // Enable/disable the search text-field/button.
  void SetSearchEnabled(bool enabled);

  // Start and stop the throbber.
  void StartThrobber();
  void StopThrobber();

 private:
  // Invoked when the user presses the search button.
  virtual void ButtonPressed(views::NativeButton* sender);

  // TextField method, does nothing.
  virtual void ContentsChanged(views::TextField* sender,
                               const std::wstring& new_contents)  {}

  // Textfield method, if key is the return key the search is updated.
  virtual void HandleKeystroke(views::TextField* sender,
                               UINT message,
                               TCHAR key,
                               UINT repeat_count,
                               UINT flags);

  // Notifies the delegate to update the search.
  void DoSearch();

  void LinkActivated(views::Link* link, int event_flags);

  Delegate* delegate_;
  views::Link* title_link_;
  views::ImageView* title_image_;
  views::ImageView* product_logo_;
  views::TextField* search_field_;
  views::NativeButton* search_button_;
  views::ScrollView* scroll_view_;
  views::Throbber* throbber_;

  DISALLOW_EVIL_CONSTRUCTORS(SearchableUIContainer);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_NATIVE_UI_CONTENTS_H_

