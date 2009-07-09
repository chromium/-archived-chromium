// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/npruntime_util.h"

#if USE(V8_BINDING)
#include "ChromiumDataObject.h"
#include "ClipboardChromium.h"
#include "EventNames.h"
#include "MouseEvent.h"
#include "NPV8Object.h"  // for PrivateIdentifier
#include "V8Helpers.h"
#include "V8Proxy.h"
#elif USE(JAVASCRIPTCORE_BINDINGS)
#include "bridge/c/c_utility.h"
#endif

#undef LOG

#include "base/pickle.h"
#if USE(V8_BINDING)
#include "webkit/api/public/WebDragData.h"
#include "webkit/glue/glue_util.h"
#endif

using WebKit::WebDragData;
#if USE(JAVASCRIPTCORE_BINDINGS)
using JSC::Bindings::PrivateIdentifier;
#endif


namespace webkit_glue {

bool SerializeNPIdentifier(NPIdentifier identifier, Pickle* pickle) {
  PrivateIdentifier* priv = static_cast<PrivateIdentifier*>(identifier);

  // If the identifier was null, then we just send a numeric 0.  This is to
  // support cases where the other end doesn't care about the NPIdentifier
  // being serialized, so the bogus value of 0 is really inconsequential.
  PrivateIdentifier null_id;
  if (!priv) {
    priv = &null_id;
    priv->isString = false;
    priv->value.number = 0;
  }

  if (!pickle->WriteBool(priv->isString))
    return false;
  if (priv->isString) {
    // Write the null byte for efficiency on the other end.
    return pickle->WriteData(
        priv->value.string, strlen(priv->value.string) + 1);
  }
  return pickle->WriteInt(priv->value.number);
}

bool DeserializeNPIdentifier(const Pickle& pickle, void** pickle_iter,
                             NPIdentifier* identifier) {
  bool is_string;
  if (!pickle.ReadBool(pickle_iter, &is_string))
    return false;

  if (is_string) {
    const char* data;
    int data_len;
    if (!pickle.ReadData(pickle_iter, &data, &data_len))
      return false;
    DCHECK_EQ((static_cast<size_t>(data_len)), strlen(data) + 1);
    *identifier = NPN_GetStringIdentifier(data);
  } else {
    int number;
    if (!pickle.ReadInt(pickle_iter, &number))
      return false;
    *identifier = NPN_GetIntIdentifier(number);
  }
  return true;
}

#if USE(V8)

inline v8::Local<v8::Value> GetEvent(const v8::Handle<v8::Context>& context) {
  static v8::Persistent<v8::String> event(
      v8::Persistent<v8::String>::New(v8::String::NewSymbol("event")));
  return context->Global()->GetHiddenValue(event);
}

static bool DragEventData(NPObject* npobj, int* event_id, WebDragData* data) {
  using WebCore::V8DOMWrapper;
  using WebCore::V8Proxy;

  if (npobj == NULL)
    return false;
  if (npobj->_class != npScriptObjectClass)
    return false;

  v8::HandleScope handle_scope;
  v8::Handle<v8::Context> context = v8::Context::GetEntered();
  if (context.IsEmpty())
    return false;

  // Get the current WebCore event.
  v8::Handle<v8::Value> current_event(GetEvent(context));
  WebCore::Event* event = V8DOMWrapper::convertToNativeEvent(current_event);
  if (event == NULL)
    return false;

  // Check that the given npobj is that event.
  V8NPObject* object = reinterpret_cast<V8NPObject*>(npobj);
  WebCore::Event* given = V8DOMWrapper::convertToNativeEvent(object->v8Object);
  if (given != event)
    return false;

  // Check the execution frames are same origin.
  V8Proxy* current = V8Proxy::retrieve(V8Proxy::retrieveFrame());
  WebCore::Frame* frame = V8Proxy::retrieveFrame(context);
  if (!current || !current->canAccessFrame(frame, false))
    return false;

  const WebCore::EventNames& event_names(WebCore::eventNames());
  const WebCore::AtomicString& event_type(event->type());

  enum DragTargetMouseEventId {
    DragEnterId = 1, DragOverId = 2, DragLeaveId = 3, DropId = 4
  };

  // The event type should be a drag event.
  if (event_type == event_names.dragenterEvent) {
    *event_id = DragEnterId;
  } else if (event_type == event_names.dragoverEvent) {
    *event_id = DragOverId;
  } else if (event_type == event_names.dragleaveEvent) {
    *event_id = DragLeaveId;
  } else if (event_type == event_names.dropEvent) {
    *event_id = DropId;
  } else {
    return false;
  }

  // Drag events are mouse events and should have a clipboard.
  WebCore::MouseEvent* me = reinterpret_cast<WebCore::MouseEvent*>(event);
  WebCore::Clipboard* clipboard = me->clipboard();
  if (!clipboard)
    return false;

  // And that clipboard should be accessible by WebKit policy.
  WebCore::ClipboardChromium* chrome =
      reinterpret_cast<WebCore::ClipboardChromium*>(clipboard);
  HashSet<WebCore::String> accessible(chrome->types());
  if (accessible.isEmpty())
    return false;

  RefPtr<WebCore::ChromiumDataObject> data_object(chrome->dataObject());
  if (data_object && data)
    *data = ChromiumDataObjectToWebDragData(data_object);

  return data_object != NULL;
}

#endif

bool GetDragData(NPObject* event, int* event_id, WebDragData* data) {
#if USE(V8)
  return DragEventData(event, event_id, data);
#else
  // Not supported on other ports (JSC, etc).
  return false;
#endif
}

bool IsDragEvent(NPObject* event) {
#if USE(V8)
  int event_id;
  return DragEventData(event, &event_id, NULL);  // Check the event only.
#else
  // Not supported on other ports (JSC, etc).
  return false;
#endif
}

}  // namespace webkit_glue
