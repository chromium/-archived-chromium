/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/**
 * @fileoverview This file contains functions used for creating UI components
 * on the web page. It also has functions used for altering the parameters
 * that those UI components control.
 *
 */

/*****************************************************************************
 * Helper functions for the html
 *****************************************************************************/

/**
 * Creates the html for the select pop down menu to select the time.
 */
function buildTimeSelect() {
  document.write('Time: <select id="time" name="time"');
  document.write(' onChange="changeColor()">');
  for (var t = 0; t < 24; t++) {
    document.write('<option value="' + t * 60 + '">');
    if (t % 12 == 0) {
      document.write('12');
    } else {
      document.write(t % 12);
    }
    if (t < 12) {
      document.write(' AM</option>');
    } else {
      document.write(' PM</option>');
    }
  }
  document.write('</select>');
}


/**
 * Creates the html for the select pop down menu to select the speed of the sun.
 */
function buildTimeSpeedSelect() {
  document.write('Speed: <select id="speed" name="speed" ');
  document.write('onChange="changeSpeed()">');
  document.write('<option value="1">1X(real time)</option>');
  for (var t = 10; t <= 100; t += 10) {
    document.write('<option value="' + t + '">' + t + 'X</option>');
  }
  document.write('</select>');
}


/*****************************************************************************
 * Functions for using the html input.
 *****************************************************************************/

/**
 * Caluclates the current position of the sun based on time.
 * TODO: This is currently not being used--a direction vector is.
 * However, this function shoudl change that direction vector.
 * @param {number} opt_inputMinutes Optional paramter of the time in minutes
 *     to return the sun position for.
 * @return {!Array.<number>} A vector indicating the position of the sun.
 */
function getCurrentSunPosition(opt_inputMinutes) {
  userDate = new Date();
  var totalMinutes = opt_inputMinutes;
  if (!opt_inputMinutes) {
    totalMinutes = userDate.getHours() * 60 + userDate.getMinutes();
    g_currentTime = totalMinutes;
  }
  var minuteRadians = ((totalMinutes / 720 * Math.PI) + 1.5 * Math.PI) %
                      (2 * Math.PI);
  return [150 * Math.cos(minuteRadians), 150 * Math.sin(minuteRadians), 0];
}


/**
 * Changes the speed of the sun.
 */
function changeSpeed() {
  g_sunMultiplier = document.getElementById('speed').value;
  o3djs.dump.dump('-------------speed: ' + g_sunMltiplier + '\n');
}
