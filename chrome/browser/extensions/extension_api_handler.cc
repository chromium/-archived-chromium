// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_api_handler.h"

#include "base/json_reader.h"
#include "base/json_writer.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/renderer_host/render_view_host.h"

ExtensionAPIHandler::ExtensionAPIHandler(RenderViewHost* render_view_host)
  : render_view_host_(render_view_host) {}

void ExtensionAPIHandler::HandleRequest(const std::string& name,
                                        const std::string& args,
                                        int callback_id) {
  scoped_ptr<Value> value;
  if (!args.empty()) {
    value.reset(JSONReader::Read(args, false));
    DCHECK(value.get());
  }

  // TODO(aa): This will probably dispatch to per-module specialized classes.
  // Consider refactoring similar work in dom_ui to reuse.
  if (name == "CreateTab") {
    Browser* browser = BrowserList::GetLastActive();
    if (browser) {
      DCHECK(value->IsType(Value::TYPE_DICTIONARY));
      std::string url;
      static_cast<DictionaryValue*>(value.get())->GetString(L"url", &url);
      browser->AddTabWithURL(GURL(url), GURL(), PageTransition::TYPED, true,
                             NULL);

      static int response_count = 0;
      scoped_ptr<Value> response(Value::CreateIntegerValue(response_count++));
      std::string json;
      JSONWriter::Write(response.get(), false, &json);

      render_view_host_->SendExtensionResponse(callback_id, json);
    }
  }
}
