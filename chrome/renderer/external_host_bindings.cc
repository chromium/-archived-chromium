// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/external_host_bindings.h"

#include "base/values.h"
#include "chrome/common/render_messages.h"
#include "webkit/glue/webframe.h"

ExternalHostBindings::ExternalHostBindings() : frame_(NULL) {
  BindMethod("postMessage", &ExternalHostBindings::postMessage);
  BindProperty("onmessage", &on_message_handler_);
}

void ExternalHostBindings::postMessage(
    const CppArgumentList& args, CppVariant* result) {
  DCHECK(result);

  // We need at least one argument (message) and at most 2 arguments.
  // Also, the first argument must be a string
  if (args.size() < 1 || args.size() > 2 || !args[0].isString()) {
    result->Set(false);
    return;
  }

  const std::string& message = args[0].ToString();
  std::string target;
  if (args.size() >= 2 && args[1].isString()) {
    target = args[1].ToString();
  } else {
    target = "*";
  }

  std::string origin;
  GURL origin_url(frame_->GetURL().GetOrigin());
  if (origin_url.is_empty()) {
    // If the origin is not a scheme/host/port tuple, then return the literal
    // string "null".
    origin = "null";
  } else {
    origin = origin_url.spec();
  }

  result->Set(sender()->Send(new ViewHostMsg_ForwardMessageToExternalHost(
      routing_id(), message, origin, target)));
}

bool ExternalHostBindings::ForwardMessageFromExternalHost(
    const std::string& message, const std::string& origin,
    const std::string& target) {
  if (!on_message_handler_.isObject())
    return false;

  bool status = false;

  // TODO(tommi): Do the appropriate target check and drop the event if
  //  the target doesn't match the url of the current document.
  //  See: http://dev.w3.org/html5/spec/Overview.html#posting-messages
  if (target.compare("*") != 0) {
    DLOG(WARNING) << "Dropping posted message since the target wasn't '*' "
                     "and we haven't implemented parsing of the target param";
    return false;
  }

  // Construct an event object, assign the origin to the origin member and
  // assign message parameter to the 'data' member of the event.
  NPObject* event_obj = NULL;
  CreateMessageEvent(&event_obj);
  if (!event_obj) {
    NOTREACHED() << "CreateMessageEvent failed";
  } else {
    NPIdentifier init_message_event =
        NPN_GetStringIdentifier("initMessageEvent");
    NPVariant init_args[8];
    STRINGN_TO_NPVARIANT("message", sizeof("message") - 1,
                         init_args[0]);  // type
    BOOLEAN_TO_NPVARIANT(false, init_args[1]);  // canBubble
    BOOLEAN_TO_NPVARIANT(true, init_args[2]);  // cancelable
    STRINGN_TO_NPVARIANT(message.c_str(), message.length(), \
                         init_args[3]);  // data
    STRINGN_TO_NPVARIANT(origin.c_str(), origin.length(), \
                         init_args[4]);  // origin
    STRINGN_TO_NPVARIANT("", 0, init_args[5]);  // lastEventId
    NULL_TO_NPVARIANT(init_args[6]);  // source
    NULL_TO_NPVARIANT(init_args[7]);  // messagePort

    NPVariant result;
    NULL_TO_NPVARIANT(result);
    status = NPN_Invoke(NULL, event_obj, init_message_event, init_args,
                        arraysize(init_args), &result);
    DCHECK(status) << "Failed to initialize MessageEvent";
    NPN_ReleaseVariantValue(&result);

    if (status) {
      NPVariant event_arg;
      OBJECT_TO_NPVARIANT(event_obj, event_arg);
      status = NPN_InvokeDefault(NULL, on_message_handler_.value.objectValue,
                                 &event_arg, 1, &result);
      // Don't DCHECK here in case the reason for the failure is a script error.
      DLOG_IF(ERROR, !status) << "NPN_InvokeDefault failed";
      NPN_ReleaseVariantValue(&result);
    }

    NPN_ReleaseObject(event_obj);
  }

  return status;
}

bool ExternalHostBindings::CreateMessageEvent(NPObject** message_event) {
  DCHECK(message_event != NULL);
  DCHECK(frame_ != NULL);

  NPObject* window = frame_->GetWindowNPObject();
  if (!window) {
    NOTREACHED() << "frame_->GetWindowNPObject";
    return false;
  }

  const char* identifier_names[] = {
    "document",
    "createEvent",
  };

  NPIdentifier identifiers[arraysize(identifier_names)] = {0};
  NPN_GetStringIdentifiers(identifier_names, arraysize(identifier_names),
                           identifiers);

  CppVariant document;
  bool ok = NPN_GetProperty(NULL, window, identifiers[0], &document);
  DCHECK(document.isObject());

  bool success = false;
  if (ok && document.isObject()) {
    NPVariant result, event_type;
    STRINGN_TO_NPVARIANT("MessageEvent", sizeof("MessageEvent") - 1, \
                         event_type);
    success = NPN_Invoke(NULL, document.value.objectValue, identifiers[1],
                         &event_type, 1, &result);
    DCHECK(!success || result.type == NPVariantType_Object);
    if (result.type != NPVariantType_Object) {
      DCHECK(success == false);
    } else {
      DCHECK(success != false);
      // Pass the ownership to the caller (don't call ReleaseVariantValue).
      *message_event = result.value.objectValue;
    }
  }

  return success;
}
