// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Modified: zengjian@google.com
// Extracted from the original options.h for sharing between external
// components.

#ifndef BAR_TOOLBAR_OPTIONS_CONSTANTS_H__
#define BAR_TOOLBAR_OPTIONS_CONSTANTS_H__

// Use a struct so that the original code works well without modification.
struct OptionConstants {
  // Make sure that these values match those entered in bar/installer/install.h.
  enum UserOption {
    OPTION_STATUS_NO = 0,  // Note: this is also the T4 state for off.
    OPTION_STATUS_YES = 1,  // Note: this is also the T4 state for on.
    OPTION_STATUS_ASK_OPT_IN = 2,
    OPTION_STATUS_ASK_OPT_OUT = 3,
  };

  // List of all Toolbar options.
  enum Option {
    NONE,
    NEW_TAB,
    BROWSE_BY_NAME,
    LINK_DOCTOR,
    AUTOLINK,
    AUTOLINK_NEW_WINDOW,
    BOX_RIGHT_TO_LEFT,
    BOX_WIDTH,
    CONTEXT_MENU_ENABLED,
    CRASH_REPORTING_ENABLED,
    CURSOR_CHANGE_ON_KILL,
    DICTIONARY_FROM_LANG,
    DICTIONARY_TO_LANG,
    ENABLE_DC,
    ALERT_SERIAL,
    GOOGLE_HOME,
    HIDDEN,
    HOVER_DICTIONARY,
    INSTALLED_VERSION,
    ISBN_PROVIDER,
    KILL_POPUP_COUNT,
    KILL_POPUP_SOUND,
    LAST_CRASH_LOCATION,
    LAST_CRASH_TIME,
    LAST_INTRO_TIME,
    MAP_PROVIDER,
    NEW_WINDOW_OR_TAB,
    NO_ONUNLOAD,
    OPENSEARCH_FETCH_SUCCEEDED,
    POPUP_BLOCKER_WHITELIST,
    POPUP_DISABLE_SCRIPT_ERRORS,
    PREEMPT,
    PREVIOUS_VERSION,
    PRIV_WARN,
    REBAR_BREAK,
    SHOW_DC_FAREWELL,
    SPELL_DEFAULT_LANGUAGE,
    SPELL_IGNORE_CAPS,
    SPELL_IGNORE_DIGITS,
    SPELL_IGNORE_DUPS,
    SPELL_LAST_LANGUAGE,
    SUGGEST_USE_HTML,
    SUGGEST_BOOKMARKS,
    SUGGEST_QUERIES,
    STORE_HISTORY,
    TOOLTIPS,
    USAGE_STATS_ENABLED,
    VERBOSITY,
    VIN_PROVIDER,
    WHATS_NEW_URL,
    BOOKMARK_SORT_ORDER,
    PROTECTOR_OFFER_REPORTED,
    SHOULD_OFFER_REENABLE,
    BEFORE_OFFER_DS,  // The default search value before protector offer.
    TOAST_OFFERED,  // Set if an update toast was shown.
    WELCOME_TOAST_OFFERED,  // Set per user and indicates that the initial
                            // welcome toast was offered.
    AUTOFILL_POSITIVE_UPLOAD_RATE,
    AUTOFILL_NEGATIVE_UPLOAD_RATE,
    WORDFIND_ENABLE,
    WORDFIND_FIND_AS_YOU_TYPE,
    USE_GMAIL_FOR_MAILTO,
    MSXML_FAILED_COUNT,
    MSXML_FAILED_LAST,
    AUTO_FILL_IMPORT_DATA_OFFERED,
    SYNC_BALLOON_SHOWN,
    SYNC_SHOULD_PROMOTE,
    SYNC_PROMOTION_BALLOON_COUNT,
    BOOKMARK_HIGHLIGHTS_ENABLED,
    GAIA_SIGNIN_TIME,  // Tick count of last GAIA signin event.
    ENABLE_X_BUTTON,
    HIDDEN_TOOLBARS,
    SIDEBAR_VISIBLE,
    SIDEBAR_ACTIVE_FEATURE,

    // Installer options
    SETUP_IS_INSTALLING,
    SETUP_TARGET_HOME,
    SETUP_TOAST_DELAY,
    SETUP_EULA_ACCEPTED,
    SETUP_SHOW_PRIVACY_POLICY_UPDATE,
    SETUP_USAGE_STATS_ENABLED,
    SETUP_ALLOW_INTERACTIONS,
    SETUP_ENABLE_SEARCH_PROTECTOR,
    SETUP_DISABLE_BBN,

    // Toolbar state options
    TOOLBAR_DISABLED,

    // Toast options
    TOAST_SET_DEFAULT_SEARCH,
    TOAST_SET_PAGE_RANK,
    TOAST_SET_HOME_PAGE,

    // Button visibility options
    BUTTON_AUTO_FILL,
    BUTTON_AUTO_LINK,
    BUTTON_CO_BRAND_SEARCH,
    BUTTON_COUNTRY_SEARCH,
    BUTTON_CUSTOM_BUTTONS_MENU,
    BUTTON_FROOGLE_SEARCH,
    BUTTON_GROUPS_SEARCH,
    BUTTON_GO,
    BUTTON_HIGHLIGHT,
    BUTTON_IMAGES_SEARCH,
    BUTTON_LOCAL_SEARCH,
    BUTTON_LUCKY,
    BUTTON_NEW_PRODUCT_OFFER,
    BUTTON_NEWS_SEARCH,
    BUTTON_PAGE_RANK,
    BUTTON_POPUP_KILLER,
    BUTTON_SEND_TO_MENU,
    BUTTON_SITE_SEARCH,
    BUTTON_SPELLCHECK,
    BUTTON_STAR_BOOKMARK,
    BUTTON_TRANSLATE_MENU,
    BUTTON_UP,
    BUTTON_VIDEO_SEARCH,
    BUTTON_WHATS_NEW,
    BUTTON_WEB_HISTORY,
    BUTTON_WEB_HISTORY_SEARCH,

    // Debug options
    DEBUG_AUTOLINK_SERVER,
    DEBUG_AUTOLINK_PORT,
    DEBUG_AUTOLINK_PATH,
    DEBUG_AUTOLINK_REDIR_PORT,
    DEBUG_AUTOLINK_REDIR_PATH,
    DEBUG_SPELL_SERVER,
    DEBUG_SPELL_PORT,
    DEBUG_SPELL_PATH,
    DEBUG_DICT_SERVER,
    DEBUG_DICT_PORT,
    DEBUG_DICT_PATH,
    DEBUG_ACCOUNT_SERVER,
    DEBUG_ACCOUNT_PORT,
    DEBUG_ACCOUNT_PATH,
    DEBUG_NOTEBOOK_SERVER,
    DEBUG_NOTEBOOK_PORT,
    DEBUG_NOTEBOOK_URL_HOST,
    DEBUG_NOTEBOOK_URL_PORT,
    DEBUG_BOOKMARK_SERVER,
    DEBUG_BOOKMARK_PORT,
    DEBUG_STAT_SERVER,
    DEBUG_STAT_PORT,
    DEBUG_STAT_PATH,
    DEBUG_STAT_SEND_INTERVAL,
    DEBUG_UMA_ENABLED,
    DEBUG_UMA_SERVER,  // UMA (User Metrics Analysis) server.
    DEBUG_UMA_PORT,
    DEBUG_UMA_PATH,
    DEBUG_UMA_CONTENT_TYPE,
    DEBUG_SUGGEST_SERVER,
    DEBUG_SUGGEST_PORT,
    DEBUG_SUGGEST_PATH,
    DEBUG_SUGGEST_HTML_URL,
    DEBUG_NEWTAB_HTML_URL,
    DEBUG_BLOGTHIS_SERVER,
    DEBUG_BLOGTHIS_PORT,
    DEBUG_BLOGTHIS_PATH,
    DEBUG_GMAILTHIS_SERVER,
    DEBUG_GMAILTHIS_PORT,
    DEBUG_GMAILTHIS_REQ_PATH,
    DEBUG_GMAILTHIS_POST_PATH,
    DEBUG_AUTOUPDATE_SERVER,
    DEBUG_AUTOUPDATE_PORT,
    DEBUG_AUTOUPDATE_PATH,
    DEBUG_PROMOPING_SERVER,
    DEBUG_PROMOPING_PORT,
    DEBUG_PROMOPING_PATH,
    DEBUG_PAGERANK_SERVER,
    DEBUG_PAGERANK_ALTERNATE_SERVER,
    DEBUG_AUTOFILL_QUERY_SERVER,
    DEBUG_AUTOFILL_UPLOAD_SERVER,
    DEBUG_AUTOFILL_QUERY_PORT,
    DEBUG_AUTOFILL_UPLOAD_PORT,
    DEBUG_AUTOFILL_QUERY_PATH,
    DEBUG_AUTOFILL_UPLOAD_PATH,
    DEBUG_AUTOFILL_QUERY_ENABLED,
    DEBUG_AUTOFILL_UPLOAD_ENABLED,
    DEBUG_AUTOFILL_USE_TYPENAMES,
    DEBUG_SYNC_SERVER,
    DEBUG_SYNC_PORT,
    DEBUG_SYNC_DELAY_BETWEEN_SYNC_SEC,  // Delay between periodic syncs in
                                        // seconds.
    DEBUG_SYNC_WAKEUP_CYCLE_SEC,  // Sync download thread wake up cycle in
                                  // seconds.
    DEBUG_SYNC_RESYNC_DELAY_SEC,  // Delay between sync retries in seconds.
    DEBUG_WEB_HISTORY_SERVER,
    DEBUG_WEB_HISTORY_PORT,
    DEBUG_WEB_HISTORY_PATH,
    DEBUG_WEB_HISTORY_URL_HOST,
    DEBUG_WEB_HISTORY_URL_PORT,
    DEBUG_LINKDOCTOR_SERVER,
    DEBUG_LINKDOCTOR_PORT,
    DEBUG_LINKDOCTOR_PATH,
    DEBUG_GADGET_HTML_SERVER,
    DEBUG_GADGET_HTML_PATH,
    DEBUG_GADGET_OPTIONS_SERVER,
    DEBUG_GADGET_OPTIONS_PATH,
    DEBUG_GADGET_RENDER_SERVER,
    DEBUG_GADGET_RENDER_PATH,
    DEBUG_GADGET_AUTO_CLOSE,
    DEBUG_OPENSEARCH_QUERY_SERVER,
    DEBUG_OPENSEARCH_QUERY_PORT,
    DEBUG_OPENSEARCH_QUERY_PATH,
    DEBUG_PREFETCH_SB_TIMEOUT,  // Prefetch timeout for search box.
    DEBUG_PREFETCH_AB_TIMEOUT,  // Prefetch timeout for address bar.
    DEBUG_CONTENT_UPDATE_CHECK_CYCLE_SEC,  // Cycle for periodically pinging the
                                           // server for static content update
                                           // checks in seconds.
    DEBUG_CONTENT_UPDATE_WAKEUP_CYCLE_SEC,  // Wake up cycle for checking
                                            // content updates in seconds.
    DEBUG_CONTENT_UPDATE_SERVER,  // Static content update server.
    DEBUG_CONTENT_UPDATE_PORT,  // Static content update port.
    DEBUG_TRANSLATE_HTTP_SERVER,
    DEBUG_TRANSLATE_HTTPS_SERVER,
    DEBUG_TRANSLATE_GZIP_REQUEST,

    // Last fixed option and the first extended option.
    LAST_INTERNAL_OPTION,
  };

  // Available types of options.
  enum Type {
    TYPE_NONE,
    TYPE_INT,
    TYPE_UINT,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_LAST  // Must be last in the enum.
  };

  // Option location
  enum Location {
    LOCATION_PER_USER,  // Per-user settings.
    LOCATION_PER_MACHINE,  // Per-machine settings.
    LOCATION_DEBUG,  // Debug options. It will return default values in retail.
    LOCATION_LAST  // Must be last in the enum.
  };

  enum ChangeCause {
    CAUSE_LOADING,  // The value is loaded off the registry.
    CAUSE_MODIFY,  // SetX called.
    CAUSE_RESET,  // ResetToDefaults called.
  };

  enum ModifyCompletion {
    MODIFY_COMMIT,  // All modifications will be saved to registry.
    MODIFY_DISCARD,  // Throw away all the modifications.
  };

  enum ResetBehavior {
    RESET,
    KEEP,
  };
};

#endif  // BAR_TOOLBAR_OPTIONS_CONSTANTS_H__
