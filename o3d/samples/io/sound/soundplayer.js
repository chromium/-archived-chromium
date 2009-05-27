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
 * @fileoverview  This file defines a SoundPlayer object that can be used to
 * play .mp3 files via a tiny soundplayer.swf file.
 *
 * Here's some example javascript for how you'd use this thing:
 *   var soundPlayer = new SoundPlayer();
 *   soundPlayer.play('songToPlayOnce.mp3')
 *   soundPlayer.play('soundToLoopForeverAt50PercentVolume.mp3',50,999)
 *   soundPlayer.play('sfxToPlayAsEvent.mp3',100,0,true)
 *   soundPlayer.setVolume('soundToTurnOff.mp3',0);
 *   soundPlayer.setGlobalVolume(20);
 *
 */

// Write a DIV to the screen that we can later use to embed our flash movie.
  var html = '<div style="position:absolute; top:-1000px; left:-1000px; z-index: 0;width: 200px; height: 200px; ">'
  html = '<OBJECT style="position:absolute; top:-1000px; left:-1000px;" classid="clsid:D27CDB6E-AE6D-11cf-96B8-444553540000" '
  html += 'codebase="http://download.macromedia.com/pub/shockwave/cabs/flash'
  html += '/swflash.cab#version=6,0,0,0" WIDTH="200" HEIGHT="200" '
  html += 'id="soundPlayer" ALIGN=""> '
  html += '<PARAM NAME=movie VALUE="sound/soundplayer.swf"> '
  html += '<PARAM NAME=quality VALUE=high> '
  html += '<PARAM NAME=allowScriptAccess VALUE=always> '
  html += '<EMBED src="sound/soundplayer.swf" quality=high WIDTH="200" HEIGHT="200" '
  html += 'NAME="soundPlayer" swLiveConnect=true ALIGN="" '
  html += 'TYPE="application/x-shockwave-flash" allowScriptAccess="always" '
  html += 'PLUGINSPAGE="http://www.macromedia.com/go/getflashplayer">'
  html += '</EMBED></OBJECT></div>';
  document.write(html);

/**
 * Constructor for our SoundPlayer object.
 */
function SoundPlayer() {
  // TODO: Make this more generic so it loads at the author's
  // request rather than at include time.
}

/**
 * This is a helper function to simplify code throughout. Returns a default
 * value if the first is empty.
 * @param {Object} val1  Value to check for emptiness
 * @param {Object} val2  Value to return if va1 is empty
 * @return {Object} One of the above.
 */
SoundPlayer.prototype.defaultIfEmpty = function(val1,val2) {
  if (val1 == undefined) {
    return val2;
  } else {
    return val1;
  }
}

/**
 * This is the way to send action calls to the flash plugin. In this case,
 * a string containing our action script that we want to execute. (Note that
 * flash doesn't have a true "eval" function, so this isn't a security hole.)
 * @param {string} callStr  The command to have flash execute.
 */
SoundPlayer.prototype.makeActionScriptCall = function(callStr) {
  try {
    window.document['soundPlayer'].SetVariable('actionScriptCall', callStr);
  } catch (ex) {
  // Catch the exception so that any problems with the sound player don't
  // derail the whle game.
  }
}

// TODO: Need to clean up these function comments to follow
// jsdoc standard. Also need to clean up for readability.

/**
 * SoundPlayer.play() : Call this guy to play a sound or music file.
 *
 * fileURL     : a url to any mp3 you want to play (required)
 * volumeLevel : a number from 0 (silent) to 100 (optional, defaults to 100)
 * loopCount   : How many times to loop the sound (optional, defaults to 0)
 * isNewTrack  : Boolean. If true, then a new "track" will be created,
 *               allowing you to layer as many sounds as you like
 */
SoundPlayer.prototype.play = function(fileURL,volumeLevel,loopCount,isNewTrack) {
  volumeLevel = this.defaultIfEmpty(volumeLevel,100);
  loopCount = this.defaultIfEmpty(loopCount,0);
  isNewTrack = this.defaultIfEmpty(isNewTrack,false);
  var actionScriptCall = 'play('+fileURL+',' + volumeLevel + ',' +
    loopCount + ',' + isNewTrack + ')';
  this.makeActionScriptCall(actionScriptCall);
}

/**
 * SoundPlayer.setVolume() : Call this guy to change the volume on a sound.
 *
 * fileURL     : a url to any mp3 or wav file you want to change the volume on
 * volumeLevel : a number from 0 (silent) to 100 (loud)
 */
SoundPlayer.prototype.setVolume = function(fileURL,volumeLevel,fadeTime) {
  var actionScriptCall = 'setVolume('+fileURL+',' + volumeLevel + ')';
  this.makeActionScriptCall(actionScriptCall);
}

/**
 * SoundPlayer.setGlobalVolume() : Call this guy to change the global
 * volume on a sound. The global volume "multiplies" each sound's volume,
 * so if your sound is playing at 100% and your global is at 50%, then
 * your sounds will be half as loud.
 *
 * volumeLevel : a number from 0 (silent) to 100 (loud)
 */
SoundPlayer.prototype.setGlobalVolume = function(volumeLevel) {
  var actionScriptCall = 'setGlobalVolume(' + volumeLevel + ')';
  this.makeActionScriptCall(actionScriptCall);
}

// Create a global variable for this application to use.
var soundPlayer = new SoundPlayer();
