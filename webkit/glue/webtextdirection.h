// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBTEXTDIRECTION_H_
#define WEBKIT_GLUE_WEBTEXTDIRECTION_H_

// Represents text directions (or writing directions) of a DOM node.
// This type is used as the input parameter of WebWidget::SetTextDirection().
// This function converts these values to WebCore::WritingDirection values and
// call the Editor::setBaseWritingDirection() function.
// TODO(hbono): Add WEB_TEXT_DIRECTION_ORIGINAL that represents "revert the
// previous changes and set back to the original one" and implement it.
// TODO(hbono): Add WEB_TEXT_DIRECTION_TOGGLE that represents "toggle the text
// direction" and implement it.
enum WebTextDirection {
  WEB_TEXT_DIRECTION_DEFAULT,  // WebCore::NaturalWritingDirection
  WEB_TEXT_DIRECTION_LTR,      // WebCore::LeftToRightWritingDirection
  WEB_TEXT_DIRECTION_RTL,      // WebCore::RightToLeftWritingDirection
};

#endif  // WEBKIT_GLUE_WEBTEXTDIRECTION_H_
