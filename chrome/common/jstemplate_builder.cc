// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// A helper function for using JsTemplate. See jstemplate_builder.h for more
// info.

#include "chrome/common/jstemplate_builder.h"

#include "chrome/common/common_resources.h"
#include "chrome/common/json_value_serializer.h"
#include "base/logging.h"
#include "chrome/common/resource_bundle.h"
#include "base/string_util.h"

namespace jstemplate_builder {

std::string GetTemplateHtml(const StringPiece& html_template,
                            const DictionaryValue* json,
                            const StringPiece& template_id) {
  // fetch and cache the pointer of the jstemplate resource source text.
  static const StringPiece jstemplate_src(
    ResourceBundle::GetSharedInstance().GetRawDataResource(IDR_JSTEMPLATE_JS));

  if (jstemplate_src.empty()) {
    NOTREACHED() << "Unable to get jstemplate src";
    return std::string();
  }

  // Convert the template data to a json string.
  DCHECK(json) << "must include json data structure";

  std::string jstext;
  JSONStringValueSerializer serializer(&jstext);
  serializer.Serialize(*json);
  // </ confuses the HTML parser because it could be a </script> tag.  So we
  // replace </ with <\/.  The extra \ will be ignored by the JS engine.
  ReplaceSubstringsAfterOffset(&jstext, 0, "</", "<\\/");

  std::string output(html_template.data(), html_template.size());
  output.append("<script>");
  output.append(jstemplate_src.data(), jstemplate_src.size());
  output.append("var tp = document.getElementById('");
  output.append(template_id.data(), template_id.size());
  output.append("'); var cx = new JsExprContext(");
  output.append(jstext);
  output.append("); jstProcess(cx, tp);</script>");

  return output;
}

}  // namespace jstemplate_builder
