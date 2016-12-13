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
 * Our avatar object. Hooray!
 */
function Avatar(initObj) {
  this.absorbNamedValues(initObj);

  this.animation = "Hero_Stand"
  this.velX = 0;
  this.velY = 0;
  this.velZ = 0;
  this.targetRotZ = 0;  // Where we'd like to be facing.
  this.targetVelX = 0;  // Speed we'd like to be going.
  this.swordRate = .5;

  this.width = 30;
  this.height = 50;

  this.frame = 1;
  this.framesSinceShot = 0;  // If > 0, tack a "#1" onto our instance name.
  this.isJumping = false;

  // We always start on platform 0 to fix a placement bug.
  this.platformID = 0;
  this.x = 0;
  this.y = 0;
  this.mapX = 0;
  this.z = 200;
  this.parentPlatformID = 0;

}

Avatar.prototype = new Actor;

Avatar.prototype.onTick = function(timeElapsed) {
  updateActor(this);
}
