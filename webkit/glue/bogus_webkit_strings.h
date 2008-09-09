// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_BOGUS_WEBKIT_STRINGS_H_
#define WEBKIT_GLUE_BOGUS_WEBKIT_STRINGS_H_

// TODO(pinkerton): This file is temporary for bootstrapping mac&linux.
//
// The real webkit_strings.h is generated from a .grd file to create a .h file
// and the associated resource file. We don't have the GRIT machinery on the
// Mac (yet) so we cheat here by defining a whole bunch of constants. The calls
// to webkit_glue::GetLocalizedString will actually try to use NSBundle to load
// them. It'll fail at runtime, harshly.


#define IDS_SEARCHABLE_INDEX_INTRO              1
#define IDS_FORM_SUBMIT_LABEL                   2
#define IDS_FORM_INPUT_ALT                      3
#define IDS_FORM_RESET_LABEL                    4
#define IDS_FORM_FILE_BUTTON_LABEL              5
#define IDS_FORM_FILE_NO_FILE_LABEL             6
#define IDS_RECENT_SEARCHES_NONE                7
#define IDS_RECENT_SEARCHES                     8
#define IDS_RECENT_SEARCHES_CLEAR               9
#define IDS_AX_ROLE_WEB_AREA                    10
#define IDS_AX_ROLE_LINK                        11
#define IDS_AX_ROLE_LIST_MARKER                 12
#define IDS_AX_ROLE_IMAGE_MAP                   13
#define IDS_AX_ROLE_HEADING                     14
#define IDS_AX_BUTTON_ACTION_VERB               15
#define IDS_AX_RADIO_BUTTON_ACTION_VERB         16
#define IDS_AX_TEXT_FIELD_ACTION_VERB           17
#define IDS_AX_CHECKED_CHECK_BOX_ACTION_VERB    18
#define IDS_AX_UNCHECKED_CHECK_BOX_ACTION_VERB  19
#define IDS_AX_LINK_ACTION_VERB                 20
#define IDS_KEYGEN_HIGH_GRADE_KEY               21
#define IDS_KEYGEN_MED_GRADE_KEY                22

#endif  // WEBKIT_GLUE_BOGUS_WEBKIT_STRINGS_H_
