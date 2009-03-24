// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extensions_ui.h"

#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/profile.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/url_constants.h"
#include "net/base/net_util.h"

#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

////////////////////////////////////////////////////////////////////////////////
//
// ExtensionsHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

ExtensionsUIHTMLSource::ExtensionsUIHTMLSource()
    : DataSource(chrome::kChromeUIExtensionsHost, MessageLoop::current()) {
}

void ExtensionsUIHTMLSource::StartDataRequest(const std::string& path,
                                              int request_id) {
  DictionaryValue localized_strings;
  localized_strings.SetString(L"title",
      l10n_util::GetString(IDS_EXTENSIONS_TITLE));

  static const StringPiece extensions_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_EXTENSIONS_UI_HTML));
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

ExtensionsDOMHandler::ExtensionsDOMHandler(DOMUI* dom_ui,
                                           ExtensionsService* extension_service)
    : DOMMessageHandler(dom_ui), extensions_service_(extension_service) {
  dom_ui_->RegisterMessageCallback("requestExtensionsData",
      NewCallback(this, &ExtensionsDOMHandler::HandleRequestExtensionsData));
}

void ExtensionsDOMHandler::HandleRequestExtensionsData(const Value* value) {
  DictionaryValue results;

  // Add the extensions to the results structure.
  ListValue *extensions_list = new ListValue();
  const ExtensionList* extensions = extensions_service_->extensions();
  for (ExtensionList::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
     extensions_list->Append(CreateExtensionDetailValue(*extension));
  }
  results.Set(L"extensions", extensions_list);

  // Add any error log lines to the result structure.
  ListValue *errors_list = new ListValue();
  const std::vector<std::string>* errors =
      ExtensionErrorReporter::GetInstance()->GetErrors();
  for (std::vector<std::string>::const_iterator error = errors->begin();
       error != errors->end(); ++error) {
    errors_list->Append(Value::CreateStringValue(*error));
  }
  results.Set(L"errors", errors_list);

  dom_ui_->CallJavascriptFunction(L"returnExtensionsData", results);
}

static void CreateScriptFileDetailValue(
    const FilePath& extension_path, const UserScript::FileList& scripts,
    const wchar_t* key, DictionaryValue* script_data) {
  if (scripts.empty())
    return;

  ListValue *list = new ListValue();
  for (size_t i = 0; i < scripts.size(); ++i) {
    const UserScript::File &file = scripts[i];
    // We are passing through GURLs to canonicalize the output to a valid
    // URL path fragment.
    GURL script_url = net::FilePathToFileURL(file.path());
    GURL extension_url = net::FilePathToFileURL(extension_path);
    std::string relative_path =
        script_url.spec().substr(extension_url.spec().length() + 1);

    list->Append(new StringValue(relative_path));
  }
  script_data->Set(key, list);
}

// Static
DictionaryValue* ExtensionsDOMHandler::CreateContentScriptDetailValue(
  const UserScript& script, const FilePath& extension_path) {
  DictionaryValue* script_data = new DictionaryValue();
  CreateScriptFileDetailValue(extension_path, script.js_scripts(), L"js",
    script_data);
  CreateScriptFileDetailValue(extension_path, script.css_scripts(), L"css",
    script_data);

  // Get list of glob "matches" strings
  ListValue *url_pattern_list = new ListValue();
  const std::vector<URLPattern>& url_patterns = script.url_patterns();
  for (std::vector<URLPattern>::const_iterator url_pattern =
      url_patterns.begin();
    url_pattern != url_patterns.end(); ++url_pattern) {
    url_pattern_list->Append(new StringValue(url_pattern->GetAsString()));
  }

  script_data->Set(L"matches", url_pattern_list);

  return script_data;
}

// Static
DictionaryValue* ExtensionsDOMHandler::CreateExtensionDetailValue(
    const Extension *extension) {
  DictionaryValue* extension_data = new DictionaryValue();

  extension_data->SetString(L"name", extension->name());
  extension_data->SetString(L"description", extension->description());
  extension_data->SetString(L"version", extension->version()->GetString());

  // Add list of content_script detail DictionaryValues
  ListValue *content_script_list = new ListValue();
  UserScriptList content_scripts = extension->content_scripts();
  for (UserScriptList::const_iterator script = content_scripts.begin();
    script != content_scripts.end(); ++script) {
      content_script_list->Append(CreateContentScriptDetailValue(*script,
          extension->path()));
  }
  extension_data->Set(L"content_scripts", content_script_list);

  // Add permissions
  ListValue *permission_list = new ListValue;
  std::vector<URLPattern> permissions = extension->permissions();
  for (std::vector<URLPattern>::iterator permission = permissions.begin();
       permission != permissions.end(); ++permission) {
    permission_list->Append(Value::CreateStringValue(
        permission->GetAsString()));
  }
  extension_data->Set(L"permissions", permission_list);

  return extension_data;
}

ExtensionsDOMHandler::~ExtensionsDOMHandler() {
}

// ExtensionsDOMHandler, public: -----------------------------------------------

void ExtensionsDOMHandler::Init() {
}

ExtensionsUI::ExtensionsUI(WebContents* contents) : DOMUI(contents) {
  ExtensionsService *exstension_service = GetProfile()->GetExtensionsService();

  ExtensionsDOMHandler* handler = new ExtensionsDOMHandler(this,
      exstension_service);
  AddMessageHandler(handler);
  handler->Init();

  ExtensionsUIHTMLSource* html_source = new ExtensionsUIHTMLSource();

  // Set up the chrome-ui://extensions/ source.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(&chrome_url_data_manager,
          &ChromeURLDataManager::AddDataSource, html_source));
}
