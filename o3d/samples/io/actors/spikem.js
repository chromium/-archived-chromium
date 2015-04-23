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
 * @fileoverview  This file defines the Spikem class.
 */

/**
 * A Horizontal Floater.
 */
function Spikem(initObj) {
  this.absorbNamedValues(initObj);
  this.width = 24;
  this.height = 55;
  this.velX = -200;
  this.velZ = 0;
  this.isDead = false;
  this.isHit = false;
  this.pauseFrames = 0;
  this.frameName = "spinning"
}
Spikem.prototype = new Actor;

Spikem.prototype.onTick = function(timeElapsed) {
  if (this.isDead == true) {
    return;
  }

  if (this.isHit == true) {
    if (this.z < -1000) {
      isDead = true;
    } else {
      this.velX = this.velX*.9;
      // Here's how I move. moveMapX is a handy function that updates both
      // my "virtual 2d world" mapX, as well as my literal x + y values
      this.z -= 20;
      this.moveMapX(this.velX * timeElapsed)
    }
  } else {
    // I move based off of the platform that I'm on. So get that now.
    var myPlatform = world.platforms[this.platformID];

    // I stay on my platform
    if (this.mapX - this.width/2 < myPlatform.left.mapX
        && this.velX < 0) {
      this.velX *= -1;
    } else if (this.mapX + this.width/2 > myPlatform.right.mapX
        && this.velX > 0) {
      this.velX *= -1;
    }

    this.rotZ += this.velX / 60 * timeElapsed;

    if (Math.abs(this.velX) < .1) {
      if (Math.random() > .5) {
        this.velX = -400;
      } else {
        this.velX = 400;
      }
      this.pauseFrames = Math.random() * 0.5 + 1;
    }

    if (this.pauseFrames > 0) {
      this.frameName = "spinning"
      this.pauseFrames -= timeElapsed;
    } else {
      this.frameName = "charging"
      this.velX = this.velX*.9;
      // Here's how I move. moveMapX is a handy function that updates both
      // my "virtual 2d world" mapX, as well as my literal x + y values
      this.moveMapX(this.velX * timeElapsed)
    }

    if (this.isHitBySword()) {
      this.isHit = true;
      soundPlayer.play('sound/_SMASH.mp3', 100);
      this.velX = 0;
    } else if (this.isHitByArrow()) {
      top.arrowActor.hide();
      this.isHit = true;
      soundPlayer.play('sound/_SMASH.mp3', 100);
      this.velX = top.arrowActor.velX;
    } else if (this.collidesWith(avatar)) {
      // Play an event sound @100% volume, 0 repeats
      soundPlayer.play('sound/ah.mp3', 100);

      this.velZ = 0;
      if (avatar.velX < 1) {
        avatar.velX = this.velX * 7;
      } else {
        avatar.velX = avatar.velX * -7;
      }
    }
  }

  updateActor(this);
}
