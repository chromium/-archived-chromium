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

#include <libxml/parser.h>

#include "chrome/browser/metrics_response.h"

// State to pass around during SAX parsing.
struct SAXState {
  int collectors;
  int events;
  int interval;
};

// libxml uses xmlChar*, which is unsigned char*
inline const char* Char(const xmlChar* input) {
  return reinterpret_cast<const char*>(input);
}

static void SAXStartElement(void* user_data,
                            const xmlChar* name,
                            const xmlChar** attrs) {
  if (!name || !attrs)
    return;

  SAXState* state = static_cast<SAXState*>(user_data);

  if (strcmp(Char(name), "upload") == 0) {
    for (int i = 0; attrs[i] && attrs[i + i]; i += 2) {
      if (strcmp(Char(attrs[i]), "interval") == 0) {
        state->interval = atoi(Char(attrs[i + 1]));
        return;
      }
    }
  } else if (strcmp(Char(name), "limit") == 0) {
    for (int i = 0; attrs[i] && attrs[i + 1]; i += 2) {
      if (strcmp(Char(attrs[i]), "events") == 0) {
        state->events = atoi(Char(attrs[i + 1]));
        return;
      }
    }
  } else if (strcmp(Char(name), "collector") == 0) {
    for (int i = 0; attrs[i] && attrs[i + 1]; i += 2) {
      if (strcmp(Char(attrs[i]), "type") == 0) {
        const char* type = Char(attrs[i + 1]);
        if (strcmp(type, "document") == 0) {
          state->collectors |= MetricsResponse::COLLECTOR_DOCUMENT;
        } else if (strcmp(type, "profile") == 0) {
          state->collectors |= MetricsResponse::COLLECTOR_PROFILE;
        } else if (strcmp(type, "window") == 0) {
          state->collectors |= MetricsResponse::COLLECTOR_WINDOW;
        } else if (strcmp(type, "ui") == 0) {
          state->collectors |= MetricsResponse::COLLECTOR_UI;
        }
        return;
      }
    }
  }
}

MetricsResponse::MetricsResponse(const std::string& response_xml)
    : valid_(false),
      collectors_(COLLECTOR_NONE),
      events_(0),
      interval_(0) {
  if (response_xml.empty())
    return;

  xmlSAXHandler handler = {0};
  handler.startElement = SAXStartElement;
  SAXState state = {0};

  valid_ = !xmlSAXUserParseMemory(&handler, &state,
                                  response_xml.data(),
                                  static_cast<int>(response_xml.size()));

  collectors_ = state.collectors;
  events_ = state.events;
  interval_ = state.interval;
}

