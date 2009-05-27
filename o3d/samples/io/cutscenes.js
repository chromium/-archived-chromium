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
 * @fileoverview  This file isn't important to the actual game. It just runs
 * pretty animations outside the o3d window.
 */

/**
 * "Opens" the cover. This thing calls itself again and again until it's done.
 * @param {number} lastPercent  Number from 0 to 100 representing how "done"
 */
function animateCover(lastPercent) {
  // Move 10% more toward a 100% animation
  lastPercent = ifEmpty(lastPercent,0);
  lastPercent = lastPercent + 10;

  // Play a nice sound.
  if (lastPercent == 10) {
    soundPlayer.play('sound/page.mp3', 100);
    $('innercover-div').style.width = $('book-left').offsetWidth;
    $('book-left').style.width = $('book-left').offsetWidth;
  }

  // Transform the cover graphic from "closed" to "open." If we're more than
  // 50% along the way, then this cover gets hidden.
  var cover = $('cover')
  var innerCover = $('innercover')

  var factor = Math.abs(Math.cos(lastPercent/50*Math.PI/2));
  if (lastPercent < 50){
    cover.style.width = factor * 1000;
    cover.style.height = 600;
  } else {
    cover.visibility = 'hidden';
    cover.style.width = 1;
    innerCover.style.width = factor * 1000;
    innerCover.style.height = 600;
  }

  var shadow = $('cover-shadow');
  if (lastPercent <= 70) {
    factor = Math.abs(Math.cos(lastPercent/70*Math.PI/2));
    shadow.style.width = factor * 1000;
    shadow.style.height = 600;
    shadow.style.opacity = (70 - lastPercent)/70;
  }

  if (lastPercent < 100) {
    setTimeout('animateCover(' + lastPercent + ')',100);
  } else {
    shadow.style.visibility = 'hidden';
  }
}


/**
 * "Opens" a page. This thing calls itself again and again until it's done.
 * @param {string} id  HTML element ID of the page image that we're animating.
 * @param {number} lastPercent  Number from 0 to 100 representing how "done"
 */
function animatePage(id, lastPercent) {
  // Transform the page graphic from "closed" to "open." If we're more than
  // 50% along the way, then this cover gets hidden.
  var page = $(id)
  page.style.zIndex = 200;
  lastPercent = ifEmpty(lastPercent,0);

  // if we've already animated this page... don't do it again.
  if (parseInt(page.style.width) < 900 && lastPercent == 0) {
    return;
  }

  // Move 10% more toward a 100% animation
  lastPercent = lastPercent + 10;

  // Play a nice sound.
  if (lastPercent == 10) {
    soundPlayer.play('sound/page.mp3', 100);
  }

  var factor = Math.abs(Math.cos(lastPercent/50*Math.PI/2));
  if (lastPercent < 50) {
    page.style.width = factor * 951;
    page.style.height = 549;
  } else {
    page.visibility = 'hidden';
    page.style.width = 1;
  }

  var shadow = $('cover-shadow')
  var coverDiv = $('cover-div')

  if (lastPercent <= 70){
    coverDiv.style.zIndex = 99;
    factor = Math.abs(Math.cos(lastPercent/70*Math.PI/2));
    shadow.style.width = factor * 1000;
    shadow.style.height = 600;
    shadow.style.opacity = (70 - lastPercent)/70;
    shadow.style.visibility = 'visible';
  }

  // Fade in a bit for the start of the animated shadow.
  if (lastPercent <= 20) {
    shadow.style.opacity = (lastPercent/30);
  }

  if (lastPercent < 100) {
    setTimeout('animatePage("' + id + '",' + lastPercent + ')',100);
  } else {
    shadow.style.visibility = 'hidden';
    if (id == "page2") {
      // We'll fire the animation of page 3 after 2 seconds.
      setTimeout('animatePage("page3")',2000);
    } else if (id == "page3") {
      loadLevel(0);
    }
  }
}
