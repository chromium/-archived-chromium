// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThemeData_h
#define ThemeData_h

struct ThemeData {
    ThemeData() : m_part(0), m_state(0), m_classicState(0) {}

    unsigned m_part;
    unsigned m_state;
    unsigned m_classicState;
};

#endif  // ThemeData_h

