// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extensions_ui.h"

#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/url_constants.h"

#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

// ExtensionsUI is accessible from chrome-ui://extensions.
static const char kExtensionsHost[] = "extensions";

////////////////////////////////////////////////////////////////////////////////
//
// ExtensionsHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

ExtensionsUIHTMLSource::ExtensionsUIHTMLSource()
    : DataSource(kExtensionsHost, MessageLoop::current()) {
}

void ExtensionsUIHTMLSource::StartDataRequest(const std::string& path,
                                              int request_id) {
  DictionaryValue localized_strings;
  localized_strings.SetString(
      ASCIIToUTF16("title"),
      WideToUTF16Hack(l10n_util::GetString(IDS_EXTENSIONS_TITLE)));

  static const StringPiece extensions_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_EXTENSIONS_HTML));
  const std::string full_html = jstemplate_builder::GetTemplateHtml(
      extensions_html, &localized_strings, "root");

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

///////////////////////////////////////////////////////////////////////////////
//
// ExtensionsDOMHandler
//
///////////////////////////////////////////////////////////////////////////////

ExtensionsDOMHandler::ExtensionsDOMHandler(DOMUI* dom_ui)
    : DOMMessageHandler(dom_ui) {
}

ExtensionsDOMHandler::~ExtensionsDOMHandler() {
}

// ExtensionsDOMHandler, public: -----------------------------------------------

void ExtensionsDOMHandler::Init() {
}

ExtensionsUI::ExtensionsUI(DOMUIContents* contents) : DOMUI(contents) {
}

void ExtensionsUI::Init() {
  ExtensionsDOMHandler* handler = new ExtensionsDOMHandler(this);
  AddMessageHandler(handler);
  handler->Init();

  ExtensionsUIHTMLSource* html_source = new ExtensionsUIHTMLSource();

  // Set up the chrome-ui://extensions/ source.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(&chrome_url_data_manager,
          &ChromeURLDataManager::AddDataSource, html_source));
}

// static
GURL ExtensionsUI::GetBaseURL() {
  std::string url = DOMUIContents::GetScheme();
  url += chrome::kStandardSchemeSeparator;
  url += kExtensionsHost;
  return GURL(url);
}
