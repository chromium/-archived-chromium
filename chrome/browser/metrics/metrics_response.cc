// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <libxml/parser.h>

#include "chrome/browser/metrics/metrics_response.h"

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
