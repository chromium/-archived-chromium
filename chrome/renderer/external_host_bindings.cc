// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/external_host_bindings.h"

#include "base/values.h"
#include "chrome/common/render_messages.h"

ExternalHostBindings::ExternalHostBindings() {
  BindMethod("ForwardMessageToExternalHost",
             &ExternalHostBindings::ForwardMessageToExternalHost);
}

void ExternalHostBindings::ForwardMessageToExternalHost(
    const CppArgumentList& args, CppVariant* result) {
  // We only accept a string message identifier.
  if (args.size() != 1)
    return;

  // Args should be strings.
  if (!args[0].isString())
    return;

  const std::string& message = args[0].ToString();

  sender()->Send(new ViewHostMsg_ForwardMessageToExternalHost(
      routing_id(), message));
}

