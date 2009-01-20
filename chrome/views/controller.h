// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_CONTROLLER_H_
#define CHROME_VIEWS_CONTROLLER_H_

#include <string>

// TODO(beng): remove this interface and fold it into MenuDelegate.

class Controller {
 public:
  virtual ~Controller() { }

  // Whether or not a command is supported by this controller.
  virtual bool SupportsCommand(int id) const = 0;

  // Whether or not a command is enabled.
  virtual bool IsCommandEnabled(int id) const = 0;

  // Assign the provided string with a contextual label. Returns true if a
  // contextual label exists and false otherwise. This method can be used when
  // implementing a menu or button that needs to have a different label
  // depending on the context. If this method returns false, the default
  // label used when creating the button or menu is used.
  virtual bool GetContextualLabel(int id, std::wstring* out) const = 0;

  // Executes a command.
  virtual void ExecuteCommand(int id) = 0;
};

#endif // CHROME_VIEWS_CONTROLLER_H_

