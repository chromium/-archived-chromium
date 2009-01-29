// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file describes various types used to describe and filter notifications
// that pass through the NotificationService.

#ifndef CHROME_COMMON_NOTIFICATION_TYPES_H__
#define CHROME_COMMON_NOTIFICATION_TYPES_H__

enum NotificationType {
  // General -------------------------------------------------------------------

  // Special signal value to represent an interest in all notifications.
  // Not valid when posting a notification.
  NOTIFY_ALL = 0,

  // The app is done processing user actions, now is a good time to do
  // some background work.
  NOTIFY_IDLE,

  // Means that the app has just started doing something in response
  // to a user action, and that background processes shouldn't run if avoidable.
  NOTIFY_BUSY,

  // This is sent when the user does a gesture resulting in a noteworthy
  // action taking place. This is typically used for logging. The
  // source is the profile, and the details is a wstring identifying the action.
  NOTIFY_USER_ACTION,

  // NavigationController ------------------------------------------------------

  // A new pending navigation has been created. Pending entries are created when
  // the user requests the navigation. We don't know if it will actually happen
  // until it does (at this point, it will be "committed." Note that renderer-
  // initiated navigations such as link clicks will never be pending.
  //
  // This notification is called after the pending entry is created, but before
  // we actually try to navigate. The source will be the NavigationController
  // that owns the pending entry, and there are no details.
  NOTIFY_NAV_ENTRY_PENDING,

  // A new non-pending navigation entry has been created. This will correspond
  // to one NavigationController entry being created (in the case of new
  // navigations) or renavigated to (for back/forward navigations).
  //
  // The source will be the navigation controller doing the commit. The details
  // will be NavigationController::LoadCommittedDetails.
  NOTIFY_NAV_ENTRY_COMMITTED,

  // Indicates that the NavigationController given in the Source has decreased
  // its back/forward list count by removing entries from either the front or
  // back of its list. This is usually the result of going back and then doing a
  // new navigation, meaning all the "forward" items are deleted.
  //
  // This normally happens as a result of a new navigation. It will be followed
  // by a NOTIFY_NAV_ENTRY_COMMITTED message for the new page that caused the
  // pruning. It could also be a result of removing an item from the list to fix
  // up after interstitials.
  //
  // The details are NavigationController::PrunedDetails.
  NOTIFY_NAV_LIST_PRUNED,

  // Indicates that a NavigationEntry has changed. The source will be the
  // NavigationController that owns the NavigationEntry. The details will be
  // a NavigationController::EntryChangedDetails struct.
  //
  // This will NOT be sent on navigation, interested parties should also listen
  // for NOTIFY_NAV_ENTRY_COMMITTED to handle that case. This will be sent when
  // the entry is updated outside of navigation (like when a new title comes).
  NOTIFY_NAV_ENTRY_CHANGED,

  // Other load-related (not from NavigationController) ------------------------

  // A content load is starting.  The source will be a
  // Source<NavigationController> corresponding to the tab
  // in which the load is occurring.  No details are
  // expected for this notification.
  NOTIFY_LOAD_START,

  // A content load has stopped. The source will be a
  // Source<NavigationController> corresponding to the tab
  // in which the load is occurring.  Details in the form of a
  // LoadNotificationDetails object are optional.
  NOTIFY_LOAD_STOP,

  // A frame is staring a provisional load.  The source is a
  // Source<NavigationController> corresponding to the tab in which the load
  // occurs.  Details is a bool specifying if the load occurs in the main
  // frame (or a sub-frame if false).
  NOTIFY_FRAME_PROVISIONAL_LOAD_START,

  // Content was loaded from an in-memory cache.  The source will be a
  // Source<NavigationController> corresponding to the tab
  // in which the load occurred.  Details in the form of a
  // LoadFromMemoryCacheDetails object are provided.
  NOTIFY_LOAD_FROM_MEMORY_CACHE,

  // A provisional content load has failed with an error.  The source will be a
  // Source<NavigationController> corresponding to the tab
  // in which the load occurred.  Details in the form of a
  // ProvisionalLoadDetails object are provided.
  NOTIFY_FAIL_PROVISIONAL_LOAD_WITH_ERROR,

  // A response has been received for a resource request.  The source will be a
  // Source<NavigationController> corresponding to the tab in which the request
  // was issued.  Details in the form of a ResourceRequestDetails object are
  // provided.
  NOTIFY_RESOURCE_RESPONSE_STARTED,

  // The response to a resource request has completed.  The source will be a
  // Source<NavigationController> corresponding to the tab in which the request
  // was issued.  Details in the form of a ResourceRequestDetails object are
  // provided.
  NOTIFY_RESOURCE_RESPONSE_COMPLETED,

  // A redirect was received while requesting a resource.  The source will be a
  // Source<NavigationController> corresponding to the tab in which the request
  // was issued.  Details in the form of a ResourceRedirectDetails are provided.
  NOTIFY_RESOURCE_RECEIVED_REDIRECT,

  // The SSL state of a page has changed somehow. For example, if an insecure
  // resource is loaded on a secure page. Note that a toplevel load commit
  // will also update the SSL state (since the NavigationEntry is new) and this
  // message won't always be sent in that case.
  //
  // The source will be the navigation controller associated with the load.
  // There are no details. The entry changed will be the active entry of the
  // controller.
  NOTIFY_SSL_STATE_CHANGED,

  // Download start and stop notifications. Stop notifications can occur on both
  // normal completion or via a cancel operation.
  NOTIFY_DOWNLOAD_START,
  NOTIFY_DOWNLOAD_STOP,

  // Views ---------------------------------------------------------------------

  // Notification that a view was removed from a view hierarchy.  The source is
  // the view, the details is the parent view.
  NOTIFY_VIEW_REMOVED,

  // Browser-window ------------------------------------------------------------

  // This message is sent after a window has been opened.  The source is
  // a Source<Browser> with a pointer to the new window.
  // No details are expected.
  NOTIFY_BROWSER_OPENED,

  // This message is sent after a window has been closed.  The source is
  // a Source<Browser> with a pointer to the closed window.
  // Details is a boolean that if true indicates that the application will be
  // closed as a result of this browser window closure (i.e. this was the last
  // opened browser window).  Note that the boolean pointed to by Details is
  // only valid for the duration of this call.
  NOTIFY_BROWSER_CLOSED,

  // This message is sent when the last window considered to be an "application
  // window" has been closed. Dependent/dialog/utility windows can use this as
  // a way to know that they should also close. No source or details are passed.
  NOTIFY_ALL_APPWINDOWS_CLOSED,

  // Indicates that a top window has been closed.  The source is the HWND that
  // was closed, no details are expected.
  NOTIFY_WINDOW_CLOSED,

  // Tabs ----------------------------------------------------------------------

  // This notification is sent after a tab has been appended to the tab_strip.
  // The source is a Source<NavigationController> with a pointer to
  // controller for the added tab. There are no details.
  NOTIFY_TAB_PARENTED,

  // This message is sent before a tab has been closed.  The source is
  // a Source<NavigationController> with a pointer to the controller for the
  // closed tab.
  // No details are expected.
  //
  // See also NOTIFY_TAB_CLOSED.
  NOTIFY_TAB_CLOSING,

  // Notification that a tab has been closed. The source is the
  // NavigationController with no details.
  NOTIFY_TAB_CLOSED,

  // This notification is sent when a render view host has connected to a
  // renderer process. The source is a Source<WebContents> with a pointer to
  // the WebContents.  A NOTIFY_WEB_CONTENTS_DISCONNECTED notification is
  // guaranteed before the source pointer becomes junk.
  // No details are expected.
  NOTIFY_WEB_CONTENTS_CONNECTED,

  // This notification is sent when a WebContents swaps its render view host
  // with another one, possibly changing processes. The source is a
  // Source<WebContents> with a pointer to the WebContents.  A
  // NOTIFY_WEB_CONTENTS_DISCONNECTED notification is guaranteed before the
  // source pointer becomes junk.
  // No details are expected.
  NOTIFY_WEB_CONTENTS_SWAPPED,

  // This message is sent after a WebContents is disconnected from the
  // renderer process.
  // The source is a Source<WebContents> with a pointer to the WebContents
  // (the pointer is usable).
  // No details are expected.
  NOTIFY_WEB_CONTENTS_DISCONNECTED,

  // This message is sent when a new InfoBar has been added to a TabContents.
  // The source is a Source<TabContents> with a pointer to the TabContents the
  // InfoBar was added to. The details is a Details<InfoBarDelegate> with a
  // pointer to an object implementing the InfoBarDelegate interface for the
  // InfoBar that was added.
  NOTIFY_TAB_CONTENTS_INFOBAR_ADDED,

  // This message is sent when an InfoBar is about to be removed from a
  // TabContents. The source is a Source<TabContents> with a pointer to the
  // TabContents the InfoBar was removed from. The details is a
  // Details<InfoBarDelegate> with a pointer to an object implementing the
  // InfoBarDelegate interface for the InfoBar that was removed.
  NOTIFY_TAB_CONTENTS_INFOBAR_REMOVED,

  // This is sent when an externally hosted tab is created. The details contain
  // the ExternalTabContainer that contains the tab
  NOTIFY_EXTERNAL_TAB_CREATED,

  // This is sent when an externally hosted tab is closed.
  // No details are expected.
  NOTIFY_EXTERNAL_TAB_CLOSED,

  // Indicates that the new page tab has finished loading. This is
  // used for performance testing to see how fast we can load it after startup,
  // and is only called once for the lifetime of the browser. The source is
  // unused.  Details is an integer: the number of milliseconds elapsed between
  // starting and finishing all painting.
  NOTIFY_INITIAL_NEW_TAB_UI_LOAD,

  // This notification is sent when a TabContents is being hidden, e.g. due to
  // switching away from this tab.  The source is a Source<TabContents>.
  NOTIFY_TAB_CONTENTS_HIDDEN,

  // This notification is sent when a TabContents is being destroyed. Any object
  // holding a reference to a TabContents can listen to that notification to
  // properly reset the reference. The source is a Source<TabContents>.
  NOTIFY_TAB_CONTENTS_DESTROYED,

  // Stuff inside the tabs -----------------------------------------------------

  // This message is sent after a constrained window has been closed.  The
  // source is a Source<ConstrainedWindow> with a pointer to the closed child
  // window.  (The pointer isn't usable, except for identification.) No details
  // are expected.
  NOTIFY_CWINDOW_CLOSED,

  // Indicates that a render process has terminated. The source will be the
  // RenderProcessHost that corresponds to the process, and the details is a
  // bool specifying whether the termination was expected, i.e. if false it
  // means the process crashed.
  NOTIFY_RENDERER_PROCESS_TERMINATED,

  // Indicates that a render process has become unresponsive for a period of
  // time. The source will be the RenderWidgetHost that corresponds to the hung
  // view, and no details are expected.
  NOTIFY_RENDERER_PROCESS_HANG,

  // Indicates that a render process is created in the sandbox. The source
  // will be the RenderProcessHost that corresponds to the created process
  // and the detail is a bool telling us if the process got created on the
  // sandbox desktop or not.
  NOTIFY_RENDERER_PROCESS_IN_SBOX,

  // This is sent to notify that the RenderViewHost displayed in a WebContents
  // has changed.  Source is the WebContents for which the change happened,
  // details is the previous RenderViewHost (can be NULL when the first
  // RenderViewHost is set).
  NOTIFY_RENDER_VIEW_HOST_CHANGED,

  // This is sent when a RenderWidgetHost is being destroyed. The source
  // is the RenderWidgetHost, the details are not used.
  NOTIFY_RENDER_WIDGET_HOST_DESTROYED,

  // Notification from WebContents that we have received a response from
  // the renderer after using the dom inspector.
  NOTIFY_DOM_INSPECT_ELEMENT_RESPONSE,

  // Notification from WebContents that we have received a response from
  // the renderer in response to a dom automation controller action.
  NOTIFY_DOM_OPERATION_RESPONSE,

  // Sent when the bookmark bubble hides. The source is the profile, the
  // details unused.
  NOTIFY_BOOKMARK_BUBBLE_HIDDEN,

  // This notification is sent when the result of a find-in-page search is
  // available with the browser process. The source is a Source<TabContents>
  // with a pointer to the WebContents. Details encompass a
  // FindNotificationDetail object that tells whether the match was
  // found or not found.
  NOTIFY_FIND_RESULT_AVAILABLE,

  // This is sent when the users preference for when the bookmark bar should
  // be shown changes. The source is the profile, and the details are
  // NoDetails.
  NOTIFY_BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,

  // Used to monitor web cache usage by notifying whenever the CacheManagerHost
  // observes new UsageStats. The source will be the RenderProcessHost that
  // corresponds to the new statistics. Details are a UsageStats object sent
  // by the renderer, and should be copied - ptr not guaranteed to be valid
  // after the notification.
  NOTIFY_WEB_CACHE_STATS_OBSERVED,

  // Plugins -------------------------------------------------------------------

  // This notification is sent when a plugin process host has connected to a
  // plugin process.  There is no usable source, since it is sent from an
  // ephemeral task; register for AllSources() to receive this notification.
  // The details are in a Details<PluginProcessInfo> with a pointer to
  // a plug-in process info for the plugin, that is only valid for the time of
  // the notification (don't keep this pointer around, make a copy of the object
  // if you need to keep it).
  NOTIFY_PLUGIN_PROCESS_HOST_CONNECTED,

  // This message is sent after a PluginProcessHost is disconnected from the
  // plugin process.  There is no usable source, since it is sent from an
  // ephemeral task; register for AllSources() to receive this notification.
  // The details are in a Details<PluginProcessInfo> with a pointer to
  // a plug-in process info for the plugin, that is only valid for the time of
  // the notification (don't keep this pointer around, make a copy of the object
  // if you need to keep it).
  NOTIFY_PLUGIN_PROCESS_HOST_DISCONNECTED,

  // This message is sent when a plugin process disappears unexpectedly.
  // There is no usable source, since it is sent from an
  // ephemeral task; register for AllSources() to receive this notification.
  // The details are in a Details<PluginProcessInfo> with a pointer to
  // a plug-in process info for the plugin, that is only valid for the time of
  // the notification (don't keep this pointer around, make a copy of the object
  // if you need to keep it).
  NOTIFY_PLUGIN_PROCESS_CRASHED,

  // This message indicates that an instance of a particular plugin was
  // created in a page.  (If one page contains several regions rendered
  // by the same plugin, this notification will occur once for each region
  // during the page load.)
  // There is no usable source, since it is sent from an
  // ephemeral task; register for AllSources() to receive this notification.
  // The details are in a Details<PluginProcessInfo> with a pointer to
  // a plug-in process info for the plugin, that is only valid for the time of
  // the notification (don't keep this pointer around, make a copy of the object
  // if you need to keep it).
  NOTIFY_PLUGIN_INSTANCE_CREATED,

  // This is sent when network interception is disabled for a plugin, or the
  // plugin is unloaded.  This should only be sent/received on the browser IO
  // thread or the plugin thread. The source is the plugin that is disabling
  // interception.  No details are expected.
  NOTIFY_CHROME_PLUGIN_UNLOADED,

  // This is sent when a login prompt is shown.  The source is the
  // Source<NavigationController> for the tab in which the prompt is shown.
  // Details are a LoginNotificationDetails which provide the LoginHandler
  // that should be given authentication.
  NOTIFY_AUTH_NEEDED,

  // This is sent when authentication credentials have been supplied (either
  // by the user or by an automation service), but before we've actually
  // received another response from the server.  The source is the
  // Source<NavigationController> for the tab in which the prompt was shown.
  // No details are expected.
  NOTIFY_AUTH_SUPPLIED,

  // History -------------------------------------------------------------------

  // Sent when a history service is created on the main thread. This is sent
  // after history is created, but before it has finished loading. Use
  // NOTIFY_HISTORY_LOADED is you need to know when loading has completed. The
  // source is the profile that the history service belongs to, and the details
  // is the pointer to the newly created HistoryService object.
  NOTIFY_HISTORY_CREATED,

  // Sent when a history service has finished loading. The source is the profile
  // that the history service belongs to, and the details is the HistoryService.
  NOTIFY_HISTORY_LOADED,

  // Sent when a URL that has been typed has been added or modified. This is
  // used by the in-memory URL database (used by autocomplete) to track changes
  // to the main history system.
  //
  // The source is the profile owning the history service that changed, and
  // the details is history::URLsModifiedDetails that lists the modified or
  // added URLs.
  NOTIFY_HISTORY_TYPED_URLS_MODIFIED,

  // Sent when the user visits a URL.
  //
  // The source is the profile owning the history service that changed, and
  // the details is history::URLVisitedDetails.
  NOTIFY_HISTORY_URL_VISITED,

  // Sent when one or more URLs are deleted.
  //
  // The source is the profile owning the history service that changed, and
  // the details is history::URLsDeletedDetails that lists the deleted URLs.
  NOTIFY_HISTORY_URLS_DELETED,

  // Sent by history when the favicon of a URL changes.
  // The source is the profile, and the details is
  // history::FavIconChangeDetails (see history_notifications.h).
  NOTIFY_FAVICON_CHANGED,

  // Bookmarks -----------------------------------------------------------------

  // Sent when the starred state of a URL changes. A URL is starred if there is
  // at least one bookmark for it. The source is a Profile and the details is
  // history::URLsStarredDetails that contains the list of URLs and whether
  // they were starred or unstarred.
  NOTIFY_URLS_STARRED,

  // Sent when the bookmark bar model finishes loading. This source is the
  // Profile, and the details aren't used.
  NOTIFY_BOOKMARK_MODEL_LOADED,

  // Sent when the spellchecker object changes. Note that this is not sent the
  // first time the spellchecker gets initialized. The source is the profile, 
  // the details is SpellcheckerReinitializedDetails defined in profile.
  NOTIFY_SPELLCHECKER_REINITIALIZED,

  // Sent when the bookmark bubble is shown for a particular URL. The source
  // is the profile, the details the URL.
  NOTIFY_BOOKMARK_BUBBLE_SHOWN,

  // Non-history storage services ----------------------------------------------

  // Notification that the TemplateURLModel has finished loading from the
  // database. The source is the TemplateURLModel, and the details are
  // NoDetails.
  TEMPLATE_URL_MODEL_LOADED,

  // Notification triggered when a web application has been installed or
  // uninstalled. Any application view should reload its data.
  // The source is the profile. No details are provided.
  NOTIFY_WEB_APP_INSTALL_CHANGED,

  // This is sent to a pref observer when a pref is changed.
  NOTIFY_PREF_CHANGED,

  // Sent when a default request context has been created, so calling
  // Profile::GetDefaultRequestContext() will not return NULL.  This is sent on
  // the thread where Profile::GetRequestContext() is first called, which should
  // be the UI thread.
  NOTIFY_DEFAULT_REQUEST_CONTEXT_AVAILABLE,

  // Autocomplete --------------------------------------------------------------

  // Sent by the autocomplete controller at least once per query, each time new
  // matches are available, subject to rate-limiting/coalescing to reduce the
  // number of updates.  There are no details.
  NOTIFY_AUTOCOMPLETE_CONTROLLER_RESULT_UPDATED,

  // Sent by the autocomplete controller once per query, immediately after
  // synchronous matches become available.  There are no details.
  NOTIFY_AUTOCOMPLETE_CONTROLLER_SYNCHRONOUS_MATCHES_AVAILABLE,

  // This is sent when an item of the Omnibox popup is selected. The source is
  // the profile.
  NOTIFY_OMNIBOX_OPENED_URL,

  // Sent by the autocomplete edit when it is destroyed.
  NOTIFY_AUTOCOMPLETE_EDIT_DESTROYED,

  // Sent when the main Google URL has been updated.  Some services cache this
  // value and need to update themselves when it changes.  See
  // google_util::GetGoogleURLAndUpdateIfNecessary().
  NOTIFY_GOOGLE_URL_UPDATED,

  // Printing ------------------------------------------------------------------

  // Notification from a PrintedDocument that it has been updated. It may be
  // that a printed page has just been generated or that the document's number
  // of pages has been calculated. Details is the new page or NULL if only the
  // number of pages in the document has been updated.
  NOTIFY_PRINTED_DOCUMENT_UPDATED,

  // Notification from PrintJob that an event occured. It can be that a page
  // finished printing or that the print job failed. Details is
  // PrintJob::EventDetails.
  NOTIFY_PRINT_JOB_EVENT,

  // Shutdown ------------------------------------------------------------------

  // Sent on the browser IO thread when an URLRequestContext is released by its
  // owning Profile.  The source is a pointer to the URLRequestContext.
  NOTIFY_URL_REQUEST_CONTEXT_RELEASED,

  // Sent when WM_ENDSESSION has been received, after the browsers have been
  // closed but before browser process has been shutdown. The source/details
  // are all source and no details.
  NOTIFY_SESSION_END,

  // Personalization -----------------------------------------------------------
  NOTIFY_PERSONALIZATION,

  // User Scripts --------------------------------------------------------------

  // Sent when there are new user scripts available.
  // The details are a pointer to SharedMemory containing the new scripts.
  NOTIFY_USER_SCRIPTS_LOADED,

  // Extensions ----------------------------------------------------------------

  // Sent when new extensions are loaded. The details are an ExtensionList*.
  NOTIFY_EXTENSIONS_LOADED,

  // Sent when new extensions are installed. The details are a FilePath.
  NOTIFY_EXTENSION_INSTALLED,

  // Count (must be last) ------------------------------------------------------
  // Used to determine the number of notification types.  Not valid as
  // a type parameter when registering for or posting notifications.
  NOTIFICATION_TYPE_COUNT
};

#endif  // CHROME_COMMON_NOTIFICATION_TYPES_H__

