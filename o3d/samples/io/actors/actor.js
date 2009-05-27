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
 * @fileoverview  This file defines the Actor class.
 */

/**
 * Pulls in all names attributes of a source object and applies them to a
 * target object. Handy for pulling in a bunch of name:value pairs that were
 * passed all at once.
 */
function Actor() {
  // Create some defaults for our "required" attributes.
  this.x = 0;
  this.y = 0;
  this.z = 0;
  this.width = 20;
  this.height = 20;
  this.mapX = 0;
  this.platformID = 0;
  this.rotZ = 0;
  this.frameName = '';
}

Actor.prototype.absorbNamedValues = function(initObj) {
  for (var key in initObj) {
    this[key] = initObj[key];
  }
};

/**
 * Note that for collision detection, all actors are envisioned as 2d rectangles
 * inside a single plane. These rectangles have a width and a height, and their
 * "origin" is on the bottom center of the rectangle. So if you create a new
 * actor, be sure to give it a reasonable width and height.
 */
Actor.prototype.collidesWith = function(otherActor) {
  thisLeft = this.mapX - this.width/2;
  thisRight = this.mapX + this.width/2;
  thisTop = this.z + this.height;
  thisBottom = this.z;

  otherLeft = otherActor.mapX - otherActor.width/2;
  otherRight = otherActor.mapX + otherActor.width/2;
  otherTop = otherActor.z + otherActor.height;
  otherBottom = otherActor.z;

  // First see if we're not overlapping in any dimension.
  if (thisRight < otherLeft ||
      thisLeft > otherRight ||
      thisBottom > otherTop ||
       thisTop < otherBottom) {
    return false;
  }

  // Next check for overlap along X.
  if (thisRight >= otherLeft &&
      thisLeft <= otherRight) {
    // then we're still in the running...
  } else {
    return false;
  }

  // Next check for overlap along Y.
  if (thisBottom <= otherTop &&
      thisTop >= otherBottom) {
    return true;
  }

  // We're not overlapping in Y, so bomb.
  return false;
};

Actor.prototype.isHitBySword = function() {
  var isHit = false;
  if (avatar.animation == "Hero_Sword" && avatar.frame > 3) {
    avatar.width += 80;
    avatar.height += avatar.frame * 5;
    if (this.collidesWith(avatar)) {
      var isHit = true;
    }
    avatar.width -= 80;
    avatar.height -= avatar.frame * 5;
  }
  return isHit;
}

Actor.prototype.isHitByArrow = function() {
  var isHit = false;
  // TODO: Make this allow multiple arrows, perform better, etc.
  if (top.arrowActor != undefined) {
    // If we don't have the same "parentPlatform, meaning we're in different
    // world path space, then we don't collide.
    if (world.platforms[top.arrowActor.platformID].parentID !=
        world.platforms[this.platformID].parentID) {
      return false;
    }
    if (this.collidesWith(top.arrowActor)) {
      var isHit = true;
    }
  }
  return isHit;
}

Actor.prototype.moveMapX = function(change) {
  var platformAngle = world.platforms[this.platformID].rotZ;
  this.mapX += change;
  this.x += change * Math.cos(platformAngle);
  this.y += change * Math.sin(platformAngle);
};




