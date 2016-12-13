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
 * @fileoverview  This file defines the Arrow class.
 */

/**
 * An arrow that Io can shoot.
 */
function Arrow(initObj) {
  this.absorbNamedValues(initObj);

  // Hide myself by moving way off the screen.
  this.z = -10000;
  this.mapX = -10000;
  this.width = 40;
  this.height = 1;
  this.velX = 0;

  // Figure out which arrow number I am.
  this.arrowNumber = parseInt(initObj.name.substr(initObj.name.length-1,1));

}
Arrow.prototype = new Actor;

Arrow.prototype.onTick = function(timeElapsed) {
  if (this.mapX < avatar.mapX-500 || this.mapX > avatar.mapX+500 &&
      this.z > -10000) {
    // then hide ourselves way off screen.
    this.mapX = -10000;
    this.z = -10000;
    updateActor(this);
  } else {
    // Here's how I move. moveMapX is a handy function that updates both
    // my "virtual 2d world" mapX, as well as my literal x + y values
    this.moveMapX(this.velX * timeElapsed)
    updateActor(this);
  }
}

Arrow.prototype.shoot = function() {
  this.x = avatar.x;
  this.y = avatar.y;
  this.z = avatar.z + 33; // Approximate height of IO's crossbow.
  this.platformID = avatar.platformID;
  this.parentPlatformID = avatar.parentPlatformID;
  this.rotZ = world.platforms[this.platformID].rotZ;
  this.mapX = avatar.mapX;

  if (Math.abs(avatar.rotZ - this.rotZ) < Math.PI/2) {
    this.velX = 50 * 20;
  } else {
    this.velX = -50 * 20;
    this.rotZ -= Math.PI;
  }
  soundPlayer.play('sound/arrow.mp3', 100);
  updateActor(this);
}

// TODO: This should be a generic concept, not an arrow-specifc
// thing.
Arrow.prototype.hide = function() {
  this.mapX = -10000;
}
