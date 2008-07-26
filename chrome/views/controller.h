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

#ifndef CHROME_VIEWS_CONTROLLER_H_
#define CHROME_VIEWS_CONTROLLER_H_

///////////////////////////////////////////////////////////////////////////////
//
// Controller class
//
//   This is the Controller portion of a MVC pattern. It handles dispatching
//   commands, maintaining enabled state, and updating the UI as that state
//   changes.
//
///////////////////////////////////////////////////////////////////////////////
class Controller {
 public:
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
