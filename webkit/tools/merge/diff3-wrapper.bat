@ECHO off

:: Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

:: Wrapper around diff3-wrapper.py so it can be called from svn.

python %~dp0diff3-wrapper.py %*
exit /B %ERRORLEVEL%
