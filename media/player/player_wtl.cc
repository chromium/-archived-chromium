// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stand alone media player application used for testing the media library.

// ATL compatibility with Chrome build settings.
#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN

#undef min
#undef max
#define NOMINMAX

// Note this header must come before other ATL headers.
#include "media/player/stdafx.h"
#include <atlcrack.h>   // NOLINT
#include <atlctrls.h>   // NOLINT
#include <atlctrlw.h>   // NOLINT
#include <atldlgs.h>    // NOLINT
#include <atlframe.h>   // NOLINT
#include <atlmisc.h>    // NOLINT
#include <atlprint.h>   // NOLINT
#include <atlscrl.h>    // NOLINT

// Note these headers are order sensitive.
#include "base/at_exit.h"
#include "media/base/factory.h"
#include "media/base/pipeline_impl.h"
#include "media/player/movie.h"
#include "media/player/resource.h"
#include "media/player/wtl_renderer.h"
#include "media/player/view.h"
#include "media/player/props.h"
#include "media/player/seek.h"
#include "media/player/list.h"
#include "media/player/mainfrm.h"

// Note these headers are NOT order sensitive.
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/file_data_source.h"

CAppModule g_module;

int Run(wchar_t* cmd_line, int cmd_show) {
  base::AtExitManager exit_manager;

  CMessageLoop the_loop;
  g_module.AddMessageLoop(&the_loop);

  CMainFrame wnd_main;
  if (wnd_main.CreateEx() == NULL) {
    DCHECK(false) << "Main window creation failed!";
    return 0;
  }

  wnd_main.ShowWindow(cmd_show);

  wchar_t* url = NULL;
  if (cmd_line && *cmd_line) {
    url = cmd_line;
  }

  if (url) {
    wnd_main.MovieOpenFile(url);
  }

  int result = the_loop.Run();

  media::Movie::get()->Close();

  g_module.RemoveMessageLoop();
  return result;
}

int WINAPI _tWinMain(HINSTANCE instance, HINSTANCE /*previous_instance*/,
                     wchar_t* cmd_line, int cmd_show) {
  INITCOMMONCONTROLSEX iccx;
  iccx.dwSize = sizeof(iccx);
  iccx.dwICC = ICC_COOL_CLASSES | ICC_BAR_CLASSES;
  if (!::InitCommonControlsEx(&iccx)) {
    DCHECK(false) << "Failed to initialize common controls";
    return 1;
  }
  if (FAILED(g_module.Init(NULL, instance))) {
    DCHECK(false) << "Failed to initialize application module";
    return 1;
  }
  int result = Run(cmd_line, cmd_show);

  g_module.Term();
  return result;
}

