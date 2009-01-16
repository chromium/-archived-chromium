// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "app_controller_mac.h"

#import "base/message_loop.h"

@implementation AppController

// We can't use the standard terminate: method because it will abrubptly exit
// the app and leave things on the stack in an unfinalized state. We need to
// post a quit message to our run loop so the stack can gracefully unwind.
- (IBAction)quit:(id)sender {
  // TODO(pinkerton):
  // since we have to roll it ourselves, ask the delegate (ourselves, really)
  // if we should terminate. For example, we might not want to if the user
  // has ongoing downloads or multiple windows/tabs open. However, this would
  // require posting UI and may require spinning up another run loop to 
  // handle it. If it says to continue, post the quit message, otherwise
  // go back to normal.
  
  MessageLoopForUI::current()->Quit();
}


@end
