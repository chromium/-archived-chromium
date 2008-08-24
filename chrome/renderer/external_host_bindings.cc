// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/external_host_bindings.h"

#include "base/values.h"
#include "chrome/common/render_messages.h"

void ExternalHostBindings::BindMethods() {
  BindMethod("ForwardMessageToExternalHost",
             &ExternalHostBindings::ForwardMessageToExternalHost);
}

void ExternalHostBindings::ForwardMessageToExternalHost(
    const CppArgumentList& args, CppVariant* result) {
  // We expect at least a string message identifier, and optionally take
  // an object parameter.  If we get anything else we bail.
  if (args.size() < 2)
    return;

  // Args should be strings.
  if (!args[0].isString() && !args[1].isString())
    return;

  const std::string& receiver = args[0].ToString();
  const std::string& message = args[1].ToString();

  sender()->Send(new ViewHostMsg_ForwardMessageToExternalHost(
      routing_id(), receiver, message));
}

