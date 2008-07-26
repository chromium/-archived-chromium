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

// This provides some helper methods for building and rendering an
// internal html page.  The flow is as follows:
// - instantiate a builder given a webframe that we're going to render content
//   into
// - load the template html and load the jstemplate javascript into the frame
// - given a json data object, run the jstemplate javascript which fills in
//   template values

#ifndef CHROME_RENDERER_JSTEMPLATE_BUILDER_H__
#define CHROME_RENDERER_JSTEMPLATE_BUILDER_H__

#include <string>

#include "base/values.h"
#include "base/string_piece.h"

namespace jstemplate_builder {
  // A helper function that generates a string of HTML to be loaded.  The
  // string includes the HTML and the javascript code necessary to generate the
  // full page.
  std::string GetTemplateHtml(const StringPiece& html_template,
                              const DictionaryValue* json,
                              const StringPiece& template_id);
}  // namespace jstemplate_builder
#endif // CHROME_RENDERER_JSTEMPLATE_BUILDER_H__
