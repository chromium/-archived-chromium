REM Copyright 2009, Google Inc.
REM All rights reserved.
REM
REM Redistribution and use in source and binary forms, with or without
REM modification, are permitted provided that the following conditions are
REM met:
REM
REM     * Redistributions of source code must retain the above copyright
REM notice, this list of conditions and the following disclaimer.
REM     * Redistributions in binary form must reproduce the above
REM copyright notice, this list of conditions and the following disclaimer
REM in the documentation and/or other materials provided with the
REM distribution.
REM     * Neither the name of Google Inc. nor the names of its
REM contributors may be used to endorse or promote products derived from
REM this software without specific prior written permission.
REM
REM THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
REM "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
REM LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
REM A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
REM OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
REM SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
REM LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
REM DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
REM THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
REM (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
REM OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

REM This test will invoke all of the O3D unit-tests.  Please execute
REM these tests before submitting to P4.
@echo off
echo Invoking Tests
%~dp0\unit_tests.exe
if exist %~dp0\test_driver.bat call %~dp0\test_driver.bat %*
