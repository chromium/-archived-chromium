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

#include "config.h"

#include "webkit/glue/npruntime_util.h"

// Import the definition of PrivateIdentifier
#if USE(V8_BINDING)
#include "webkit/port/bindings/v8/np_v8object.h"
#elif USE(JAVASCRIPTCORE_BINDINGS)
#include "c_utility.h"
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
    DCHECK_EQ(data_len, strlen(data) + 1);
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
