// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_AUTOMATION_MESSAGES_H__
#define CHROME_TEST_AUTOMATION_AUTOMATION_MESSAGES_H__

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/security_style.h"
#include "chrome/common/ipc_message_utils.h"
#include "chrome/test/automation/automation_constants.h"

struct AutomationMsg_Find_Params {
  // Unused value, which exists only for backwards compat.
  int unused;

  // The word(s) to find on the page.
  string16 search_string;

  // Whether to search forward or backward within the page.
  bool forward;

  // Whether search should be Case sensitive.
  bool match_case;

  // Whether this operation is first request (Find) or a follow-up (FindNext).
  bool find_next;
};

namespace IPC {

template <>
struct ParamTraits<AutomationMsg_Find_Params> {
  typedef AutomationMsg_Find_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.unused);
    WriteParam(m, p.search_string);
    WriteParam(m, p.forward);
    WriteParam(m, p.match_case);
    WriteParam(m, p.find_next);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->unused) &&
      ReadParam(m, iter, &p->search_string) &&
      ReadParam(m, iter, &p->forward) &&
      ReadParam(m, iter, &p->match_case) &&
      ReadParam(m, iter, &p->find_next);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<AutomationMsg_Find_Params>");
  }
};

template <>
struct ParamTraits<AutomationMsg_NavigationResponseValues> {
  typedef AutomationMsg_NavigationResponseValues param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<AutomationMsg_NavigationResponseValues>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring control;
    switch (p) {
     case AUTOMATION_MSG_NAVIGATION_ERROR:
      control = L"AUTOMATION_MSG_NAVIGATION_ERROR";
      break;
     case AUTOMATION_MSG_NAVIGATION_SUCCESS:
      control = L"AUTOMATION_MSG_NAVIGATION_SUCCESS";
      break;
     case AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED:
      control = L"AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED";
      break;
     default:
      control = L"UNKNOWN";
      break;
    }

    LogParam(control, l);
  }
};

template <>
struct ParamTraits<SecurityStyle> {
  typedef SecurityStyle param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<SecurityStyle>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring control;
    switch (p) {
     case SECURITY_STYLE_UNKNOWN:
      control = L"SECURITY_STYLE_UNKNOWN";
      break;
     case SECURITY_STYLE_UNAUTHENTICATED:
      control = L"SECURITY_STYLE_UNAUTHENTICATED";
      break;
     case SECURITY_STYLE_AUTHENTICATION_BROKEN:
      control = L"SECURITY_STYLE_AUTHENTICATION_BROKEN";
      break;
     case SECURITY_STYLE_AUTHENTICATED:
      control = L"SECURITY_STYLE_AUTHENTICATED";
      break;
     default:
      control = L"UNKNOWN";
      break;
    }

    LogParam(control, l);
  }
};

template <>
struct ParamTraits<NavigationEntry::PageType> {
  typedef NavigationEntry::PageType param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<NavigationEntry::PageType>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring control;
    switch (p) {
     case NavigationEntry::NORMAL_PAGE:
      control = L"NORMAL_PAGE";
      break;
     case NavigationEntry::ERROR_PAGE:
      control = L"ERROR_PAGE";
      break;
     case NavigationEntry::INTERSTITIAL_PAGE:
      control = L"INTERSTITIAL_PAGE";
      break;
     default:
      control = L"UNKNOWN";
      break;
    }

    LogParam(control, l);
  }
};

#if defined(OS_WIN)
struct Reposition_Params {
  HWND window;
  HWND window_insert_after;
  int left;
  int top;
  int width;
  int height;
  int flags;
};

// Traits for SetWindowPos_Params structure to pack/unpack.
template <>
struct ParamTraits<Reposition_Params> {
  typedef Reposition_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.window);
    WriteParam(m, p.window_insert_after);
    WriteParam(m, p.left);
    WriteParam(m, p.top);
    WriteParam(m, p.width);
    WriteParam(m, p.height);
    WriteParam(m, p.flags);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->window) &&
           ReadParam(m, iter, &p->window_insert_after) &&
           ReadParam(m, iter, &p->left) &&
           ReadParam(m, iter, &p->top) &&
           ReadParam(m, iter, &p->width) &&
           ReadParam(m, iter, &p->height) &&
           ReadParam(m, iter, &p->flags);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.window, l);
    l->append(L", ");
    LogParam(p.window_insert_after, l);
    l->append(L", ");
    LogParam(p.left, l);
    l->append(L", ");
    LogParam(p.top, l);
    l->append(L", ");
    LogParam(p.width, l);
    l->append(L", ");
    LogParam(p.height, l);
    l->append(L", ");
    LogParam(p.flags, l);
    l->append(L")");
  }
};
#endif  // defined(OS_WIN)

}  // namespace IPC

#define MESSAGES_INTERNAL_FILE \
    "chrome/test/automation/automation_messages_internal.h"
#include "chrome/common/ipc_message_macros.h"

#endif  // CHROME_TEST_AUTOMATION_AUTOMATION_MESSAGES_H__
