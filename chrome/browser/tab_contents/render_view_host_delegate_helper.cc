// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "render_view_host_delegate_helper.h"

#include "base/command_line.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

void RenderViewHostDelegateViewHelper::CreateNewWindow(int route_id,
    base::WaitableEvent* modal_dialog_event, Profile* profile,
    SiteInstance* site) {
  // Create the new web contents. This will automatically create the new
  // TabContentsView. In the future, we may want to create the view separately.
  TabContents* new_contents =
      new TabContents(profile,
                      site,
                      route_id,
                      modal_dialog_event);
  TabContentsView* new_view = new_contents->view();

  // TODO(brettw) it seems bogus that we have to call this function on the
  // newly created object and give it one of its own member variables.
  new_view->CreateViewForWidget(new_contents->render_view_host());

  // Save the created window associated with the route so we can show it later.
  pending_contents_[route_id] = new_contents;
}

RenderWidgetHostView* RenderViewHostDelegateViewHelper::CreateNewWidget(
    int route_id, bool activatable, RenderProcessHost* process) {
  RenderWidgetHost* widget_host =
      new RenderWidgetHost(process, route_id);
  RenderWidgetHostView* widget_view =
      RenderWidgetHostView::CreateViewForWidget(widget_host);
  widget_view->set_activatable(activatable);
  // Save the created widget associated with the route so we can show it later.
  pending_widget_views_[route_id] = widget_view;
  return widget_view;
}

TabContents* RenderViewHostDelegateViewHelper::GetCreatedWindow(int route_id) {
  PendingContents::iterator iter = pending_contents_.find(route_id);
  if (iter == pending_contents_.end()) {
    DCHECK(false);
    return NULL;
  }

  TabContents* new_tab_contents = iter->second;
  pending_contents_.erase(route_id);

  if (!new_tab_contents->render_view_host()->view() ||
      !new_tab_contents->process()->channel()) {
    // The view has gone away or the renderer crashed. Nothing to do.
    return NULL;
  }

  // TODO(brettw) this seems bogus to reach into here and initialize the host.
  new_tab_contents->render_view_host()->Init();

  return new_tab_contents;
}

RenderWidgetHostView* RenderViewHostDelegateViewHelper::GetCreatedWidget(
    int route_id) {
  PendingWidgetViews::iterator iter = pending_widget_views_.find(route_id);
  if (iter == pending_widget_views_.end()) {
    DCHECK(false);
    return NULL;
  }

  RenderWidgetHostView* widget_host_view = iter->second;
  pending_widget_views_.erase(route_id);

  RenderWidgetHost* widget_host = widget_host_view->GetRenderWidgetHost();
  if (!widget_host->process()->channel()) {
    // The view has gone away or the renderer crashed. Nothing to do.
    return NULL;
  }

  return widget_host_view;
}

void RenderViewHostDelegateViewHelper::RenderWidgetHostDestroyed(
    RenderWidgetHost* host) {
  for (PendingWidgetViews::iterator i = pending_widget_views_.begin();
       i != pending_widget_views_.end(); ++i) {
    if (host->view() == i->second) {
      pending_widget_views_.erase(i);
      return;
    }
  }
}

// static
WebPreferences RenderViewHostDelegateHelper::GetWebkitPrefs(
    PrefService* prefs, bool is_dom_ui) {

  WebPreferences web_prefs;

  web_prefs.fixed_font_family =
      prefs->GetString(prefs::kWebKitFixedFontFamily);
  web_prefs.serif_font_family =
      prefs->GetString(prefs::kWebKitSerifFontFamily);
  web_prefs.sans_serif_font_family =
      prefs->GetString(prefs::kWebKitSansSerifFontFamily);
  if (prefs->GetBoolean(prefs::kWebKitStandardFontIsSerif))
    web_prefs.standard_font_family = web_prefs.serif_font_family;
  else
    web_prefs.standard_font_family = web_prefs.sans_serif_font_family;
  web_prefs.cursive_font_family =
      prefs->GetString(prefs::kWebKitCursiveFontFamily);
  web_prefs.fantasy_font_family =
      prefs->GetString(prefs::kWebKitFantasyFontFamily);

  web_prefs.default_font_size =
      prefs->GetInteger(prefs::kWebKitDefaultFontSize);
  web_prefs.default_fixed_font_size =
      prefs->GetInteger(prefs::kWebKitDefaultFixedFontSize);
  web_prefs.minimum_font_size =
      prefs->GetInteger(prefs::kWebKitMinimumFontSize);
  web_prefs.minimum_logical_font_size =
      prefs->GetInteger(prefs::kWebKitMinimumLogicalFontSize);

  web_prefs.default_encoding = prefs->GetString(prefs::kDefaultCharset);

  web_prefs.javascript_can_open_windows_automatically =
      prefs->GetBoolean(prefs::kWebKitJavascriptCanOpenWindowsAutomatically);
  web_prefs.dom_paste_enabled =
      prefs->GetBoolean(prefs::kWebKitDomPasteEnabled);
  web_prefs.shrinks_standalone_images_to_fit =
      prefs->GetBoolean(prefs::kWebKitShrinksStandaloneImagesToFit);
  web_prefs.inspector_settings =
      prefs->GetString(prefs::kWebKitInspectorSettings);

  {  // Command line switches are used for preferences with no user interface.
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    web_prefs.developer_extras_enabled =
        !command_line.HasSwitch(switches::kDisableDevTools);
    web_prefs.javascript_enabled =
        !command_line.HasSwitch(switches::kDisableJavaScript) &&
        prefs->GetBoolean(prefs::kWebKitJavascriptEnabled);
    web_prefs.web_security_enabled =
        !command_line.HasSwitch(switches::kDisableWebSecurity) &&
        prefs->GetBoolean(prefs::kWebKitWebSecurityEnabled);
    web_prefs.plugins_enabled =
        !command_line.HasSwitch(switches::kDisablePlugins) &&
        prefs->GetBoolean(prefs::kWebKitPluginsEnabled);
    web_prefs.java_enabled =
        !command_line.HasSwitch(switches::kDisableJava) &&
        prefs->GetBoolean(prefs::kWebKitJavaEnabled);
    web_prefs.loads_images_automatically =
        !command_line.HasSwitch(switches::kDisableImages) &&
        prefs->GetBoolean(prefs::kWebKitLoadsImagesAutomatically);
    web_prefs.uses_page_cache =
        command_line.HasSwitch(switches::kEnableFastback);
    web_prefs.remote_fonts_enabled =
        command_line.HasSwitch(switches::kEnableRemoteFonts);
    web_prefs.xss_auditor_enabled =
        command_line.HasSwitch(switches::kEnableXSSAuditor);
  }

  web_prefs.uses_universal_detector =
      prefs->GetBoolean(prefs::kWebKitUsesUniversalDetector);
  web_prefs.text_areas_are_resizable =
      prefs->GetBoolean(prefs::kWebKitTextAreasAreResizable);

  // User CSS is currently disabled because it crashes chrome.  See
  // webkit/glue/webpreferences.h for more details.

  // Make sure we will set the default_encoding with canonical encoding name.
  web_prefs.default_encoding =
      CharacterEncoding::GetCanonicalEncodingNameByAliasName(
          web_prefs.default_encoding);
  if (web_prefs.default_encoding.empty()) {
    prefs->ClearPref(prefs::kDefaultCharset);
    web_prefs.default_encoding = prefs->GetString(
        prefs::kDefaultCharset);
  }
  DCHECK(!web_prefs.default_encoding.empty());

  if (is_dom_ui) {
    web_prefs.loads_images_automatically = true;
    web_prefs.javascript_enabled = true;
  }

  return web_prefs;
}
