// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/npruntime_util.h"

// Import the definition of PrivateIdentifier
#if USE(V8_BINDING)
#include "NPV8Object.h"
#elif USE(JAVASCRIPTCORE_BINDINGS)
#include "bridge/c/c_utility.h"
#undef LOG
using JSC::Bindings::PrivateIdentifier;
#endif

#include "base/pickle.h"

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

}  // namespace webkit_glue

