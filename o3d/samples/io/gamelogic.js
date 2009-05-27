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
 * @fileoverview  This file sets up the game state JS structures, sets up
 * keyboard handlers, and contains the main game loop.
 */

 /**
 * Global constants that drive the game engine.
 */
var FRAMES_PER_SECOND = 20;             // Change for a faster or slower game.
var FRAME_DELAY = 1000 / FRAMES_PER_SECOND;   // Milliseconds between frames.
var GRAVITY = -2000;                   // Inches per second per second.
var JUMP_VELOCITY = 650;
var RUN_VELOCITY = 200;
var WORLD_ANGLE = 0;
var CAMERA_SOFTNESS = 15;


/**
 * These are some global pointers that we'll use for to point at our world
 * object (aka "current map" or "current level"), and our happy avatar (aka
 * the protagonist, Prince Io.)
 */
var world;
var avatar;

/**
 * Keyboard constants.
 */
var BACKSPACE = 8;
var TAB = 9;
var ENTER = 13;
var SHIFT = 16;
var CTRL = 17;
var ALT = 18;
var ESCAPE = 27;
var PAGEUP = 33;
var PAGEDOWN = 34;
var END = 35;
var HOME = 36;
var LEFT = 37;
var UP = 38;
var RIGHT = 39;
var DOWN = 40;
var DELETE = 46;
var SPACE = 32;

/**
 * These variables are special SketchUp constants that aren't really constant.
 * They are updated by SketchUp every frame to indicate what's around the
 * avatar.
 * TODO: ADDENDUM! These vars used to be "special constants,"
 * but now they're leftovers of a time when SU was the main engine, and the
 * game logic below should be cleaned up so we're not using all caps on these.
 */
var GROUNDZ;           // Absolute Z of the obstacle beneath the avatar.
var OBSTACLE_LEFT;     // Distance to nearest obstacle to the left.
var OBSTACLE_RIGHT;    // Distance to nearest obstacle on the right.

/**
 * Camera position stuff.
 * TODO: These are redundant vs. the camera.eye.x stuff in
 * init.js. Clean that outa thar!
 */
var eyeX = 0;
var eyeY = -1000;
var eyeZ = 1000;
var targetX = 0;
var targetY = 0;
var targetZ = 0;

/**
 * If the user presses the swing button while we're showing a swing, then we'll
 * mark the fact that we need to immediately swing again once the current
 * animation is done.
 */
var swingAgain = false;

/**
 * Way of determining that the user just pressed then unpressed certain keys.
 */
var upWasJustDown = false;
var ctrlWasJustUp = false;

/**
 * Identifier for our global game timer. This is only used when SketchUp is the
 * main CG engine.
 */
var timerID;

/**
 * Code to keep track of the current key state. Since these events can fire at
 * any time but we only want to apply changes to our game state at the start of
 * each game loop, we do this trick so that the nextFrame() function can poll
 * the keyIsDown data structure to determine what's happened since the last
 * frame.
 */
var keyIsDown = {};

function keyMessage(e, state) {
  keyIsDown[o3djs.event.getEventKeyChar(e ? e : window.event)] = state;
}

document.onkeydown = function(e) { keyMessage(e, true); };

document.onkeyup = function(e) { keyMessage(e, false); };


/**
 * Main Game Loop.... Here's where we actually run the game.
 * @param {number} elapsedTime  Number of seconds since we were last called.
 */
function nextFrame(elapsedTime) {
  // First, let's look at our avatar. If he's jumping, then figure out which
  // platform (if any) he's currently over.
  if (avatar.isJumping) {
    var lowestZFound = -5000;
    for (var i = 0; i < world.platforms.length; i++) {
      platform = world.platforms[i];
      if (platform.parentID == avatar.parentPlatformID ||
          i == avatar.parentPlatformID) {
        if (avatar.z > platform.z &&
            avatar.mapX > platform.left.mapX &&
            avatar.mapX < platform.right.mapX &&
            platform.z > lowestZFound && platform.isNotSolid != true) {
          lowestZFound = world.platforms[i].z
          avatar.platformID = i;
        }
      }
    }
  }

  // Create a myPlatform variable for convenience.
  var myPlatform = world.platforms[avatar.platformID]

  // Platforms without a parentID aren't really platforms. The actually define
  // the "world path" that meanders through our level. Therefore, if the only
  // platform that Io is over is one of these, then our current GROUNDZ (aka
  // the nearest walkable altitude),
  if (exists(myPlatform.parentID)) {
    GROUNDZ = myPlatform.z
    avatar.parentPlatformID = myPlatform.parentID;
  } else {
    GROUNDZ = -5000;
    avatar.parentPlatformID = avatar.platformID;
  }

  // The avatar's mapX is really important. It defines his position in the
  // imaginary 2d platformer that we weave through a 3D world. When he's at
  // mapX of 0, he's at the far left of the level (think super mario.) As
  // he proceeds through the level, his mapX will get bigger. Meanwhile, his
  // actual x, y, and z (aka altitude) values will be kept track of as well.
  // Anyway, every platform that Io can stand on has a "left" and a "right"
  // to them. Let's check to see if we've walked off of it...
  if (avatar.mapX < myPlatform.left.mapX) {
    // Whoa! We've walked off the left of our platform! If there's an adjacent
    // platform to step onto, then do that. Otherwise, it's fallin' time.
    if (exists(myPlatform.left.adjacentID)) {
      // Do some math to ensure that our actual x and y values are corrected
      // so Io stays perfectly in our "world path" plane.
      oldPlatform = myPlatform;
      overshot = avatar.mapX - myPlatform.left.mapX;
      avatar.platformID = myPlatform.left.adjacentID;
      myPlatform = world.platforms[avatar.platformID];
      avatar.x -= Math.cos(oldPlatform.rotZ) * overshot;
      avatar.y -= Math.sin(oldPlatform.rotZ) * overshot;
      avatar.x += Math.cos(myPlatform.rotZ) * overshot;
      avatar.y += Math.sin(myPlatform.rotZ) * overshot;
    } else {
      GROUNDZ = -5000;
    }
  } else if (avatar.mapX > myPlatform.right.mapX) {
    // Whoa! We've walked off the right of our platform!
    if (exists(myPlatform.right.adjacentID)) {
      oldPlatform = myPlatform;
      overshot = avatar.mapX - myPlatform.right.mapX;
      avatar.platformID = myPlatform.right.adjacentID;
      myPlatform = world.platforms[avatar.platformID];
      avatar.x -= Math.cos(oldPlatform.rotZ) * overshot;
      avatar.y -= Math.sin(oldPlatform.rotZ) * overshot;
      avatar.x += Math.cos(myPlatform.rotZ) * overshot;
      avatar.y += Math.sin(myPlatform.rotZ) * overshot;
    } else {
      GROUNDZ = -5000;
    }
  }

  // Now it's time to figure out if there's a "obstacle" nearby. Each platform
  // can potentially have a left.obstacleHeight and/or a right.obstableHeight.
  // We'll store how close any of them are in these OBSTABLE_LEFT type
  // "constants."
  OBSTACLE_RIGHT = 9000
  OBSTACLE_RIGHT_HEIGHT = 0
  OBSTACLE_LEFT = 9000
  OBSTACLE_LEFT_HEIGHT = 0

  if (myPlatform.right.obstacleHeight > 0) {
    OBSTACLE_RIGHT = myPlatform.right.mapX - avatar.mapX;
    OBSTACLE_RIGHT_HEIGHT = myPlatform.right.obstacleHeight - avatar.velZ + 20;
  }
  if (myPlatform.left.obstacleHeight > 0) {
    OBSTACLE_LEFT = avatar.mapX - myPlatform.left.mapX;
    OBSTACLE_LEFT_HEIGHT = myPlatform.left.obstacleHeight - avatar.velZ + 20;
  }

  // So... now that we've figured all of that stuff out, let's store how
  // far *above* the current platform Io is.
  avatar.relativeZ = avatar.z - myPlatform.z;

  // This special variable, avatar.groundZ, can be set by moving
  // platforms and other actors to override our actual groundZ. This allows
  // the avatar to "float" in space (or really, to stand on stuff that's
  // not a static platform but a moving one.) So if we find that variable,
  // it trumps the calculated GROUNDZ. However, we *always* clear that
  // variable out after using it. It's up to the Actor who is moving Io to
  // keep this updated inside their onTick event. (See horizontalpad.js for
  // an example of this.)
  if (exists(avatar.groundZ)) {
    GROUNDZ = avatar.groundZ;
    avatar.groundZ = undefined;
  }

  // Gah. Another one of our "special constants" that used to be updated
  // by SketchUp but now is done via JS data structures.
  // TODO: Clean 'tup.
  WORLD_ANGLE = myPlatform.rotZ;

  // Check for keyboard stuff.
  handleInput();

  // Handle falling and gravity.
  avatar.z += avatar.velZ * elapsedTime;
  if (avatar.z - GROUNDZ > 1) {
    avatar.isJumping = true;
  } else {
    if (avatar.isJumping == true) {
      soundPlayer.play('sound/step2.mp3', 100);
    }
    avatar.isJumping = false;
  }
  if (avatar.isJumping == true) {
    avatar.velZ += GRAVITY * elapsedTime;
  }
  if (avatar.z < GROUNDZ) {
    avatar.velZ = 0;
    avatar.z = GROUNDZ + .1;
    avatar.isJumping = false;
    avatar.animation = 'Hero_Run';

  }
  if (avatar.z < -1000) {
    avatar.x = 0;
    avatar.y = 0;
    avatar.z = 200;
    avatar.mapX = 0;
    avatar.platformID = 0;
    avatar.parentPlatformID = 0;
  }

  // Handle collision with things to the left and right of our avatar.
  // From here is where we start to decide which animation cycle we're on.
  // Now you can see how we're using avatar.relativeZ along with our
  // OBSTACLE_FOO "special constants" to determine not only if we've hit an
  // obstacle, but whether we've jumped high enough to get over it.
  if (OBSTACLE_LEFT < 25 && keyIsDown[LEFT] &&
      OBSTACLE_LEFT_HEIGHT > avatar.relativeZ) {
    avatar.targetVelX = 0;
    avatar.animation = 'Hero_Stand';
    avatar.frame = 1;
  }
  if (OBSTACLE_RIGHT < 25 && keyIsDown[RIGHT] &&
      OBSTACLE_RIGHT_HEIGHT > avatar.relativeZ) {
    avatar.targetVelX = 0;
    avatar.animation = 'Hero_Stand';
    avatar.frame = 1;
  }

  // If we're jumping, then use the jumping animation cycle.
  if (avatar.isJumping == true) {
    avatar.animation = 'Hero_Jump';
    if (avatar.velZ > 0) {
      avatar.frame += elapsedTime * 20;
      if (avatar.frame > 3) {
        avatar.frame = 3;
      }
    } else {
      if (avatar.z > GROUNDZ + 20) {
        avatar.frame = 4;
      } else {
        avatar.frame = 5;
      }
    }
  }

  // Figure out which frame to display inside our current cycle.
  didSwordStrike = '0';
  if (avatar.animation == 'Hero_Run') {
    avatar.frame += elapsedTime * 20;
    if (avatar.frame > 10) {
      avatar.frame = 1;
    }
    if (Math.floor(avatar.frame % 5) == 0) {
      var soundToPlay = 'sound/step'
      soundToPlay += (Math.floor(Math.random() * 3) + 1);
      soundToPlay += '.mp3'
      soundPlayer.play(soundToPlay, 100);
    }
  } else if (avatar.animation == 'Hero_Sword') {
    avatar.targetVelX = 0;
    avatar.framesSinceShot = 0;
    avatar.frame += avatar.swordRate * 20 * elapsedTime;
    if (avatar.frame < 1) {
      avatar.animation = 'Hero_Stand'
      avatar.frame = 1;
    } else if (avatar.frame > 7) {
      avatar.swordRate = -.5;
      avatar.frame = 6.2;
      didSwordStrike = '1';
    } else if (avatar.frame == 3 && avatar.swordRate < 0) {
      if (swingAgain == true) {
        avatar.swordRate = 1;
        swingAgain = false;
      } else {
        avatar.swordRate = -.5;
      }
    }

    if (avatar.frame == 3 && avatar.swordRate > 0) {
      soundPlayer.play('sound/_MISS.mp3', 100);
    }
  }

  // Update our position.
  avatar.velX = (avatar.velX + avatar.targetVelX + avatar.targetVelX) / 3;
  if (Math.abs(avatar.velX) < .2) {
    avatar.velX = 0;
  }
  avatar.mapX += avatar.velX * elapsedTime;
  avatar.x += avatar.velX * Math.cos(WORLD_ANGLE) * elapsedTime;
  avatar.y += avatar.velX * Math.sin(WORLD_ANGLE) * elapsedTime;
  avatar.rotZ = (avatar.rotZ + avatar.targetRotZ + avatar.targetRotZ) / 3;

  // Calculate our final frameName string to pass to SketchUp (or o3d).
  frameName = avatar.animation;
  if (avatar.frame < 10) {
    frameName += '0';
  }
  frameName += Math.floor(avatar.frame);
  if (avatar.framesSinceShot > 0) {
    frameName += 'Shoot';
    avatar.framesSinceShot--;
  }
  if (avatar.framesSinceShot == 3) {
    // TODO: Make this work with multiple arrows, perform
    // better, and be smart enough to handle level reloads.
    if (top.arrowActor == undefined) {
      for (var actorID = 0; actorID < world.actors.length; actorID++) {
        arrow = world.actors[actorID];
        if (arrow.name.indexOf('Arrow') == 0) {
          top.arrowActor = arrow;
          top.arrowActor.shoot();
        }
      }
    } else {
      top.arrowActor.shoot();
    }
  }
  avatar.frameName = frameName;

  // Here's where we send our state down to SketchUp or O3D.
  // Always do our avatar last, since he's the one likely to have
  // been modified by other things in the environment. (Our avatar
  // is guaranteed by the level exporter to be in world.actors[0])
  for (i = world.actors.length - 1; i >= 0; i--) {
    world.actors[i].onTick(elapsedTime);
  }

  // Now move the camera in a pleasing motion to float over Io's
  // right shoulder. (Well, it's his right shoulder when he's
  // facing "right" toward the end of the level.)
  newEyeX = avatar.x + 350 * Math.sin(WORLD_ANGLE);
  newEyeY = avatar.y - 350 * Math.cos(WORLD_ANGLE);
  newEyeZ = avatar.z + 150;
  eyeX = (newEyeX + eyeX * CAMERA_SOFTNESS) / (CAMERA_SOFTNESS + 1);
  eyeY = (newEyeY + eyeY * CAMERA_SOFTNESS) / (CAMERA_SOFTNESS + 1);
  eyeZ = (newEyeZ + eyeZ * CAMERA_SOFTNESS) / (CAMERA_SOFTNESS + 1);
  targetX = (avatar.x + targetX) / 2;
  targetY = (avatar.y + targetY) / 2;
  targetZ = (avatar.z + 200 + targetZ * 3) / 5;

  // Here's where we send our camera down to SketchUp or O3D.
  updateCamera(targetX, targetY, targetZ, eyeX, eyeY, eyeZ)

  // If we're running inside SketchUp, clear out our old timer and set a new
  // one.
  if (isSU) {
    clearTimeout(timerID);
    timerID = self.setTimeout('nextFrame()', FRAME_DELAY);
  }
}


/**
 * Sends our state to our render engine. (See knightgame.rb for the Sketchup
 * ruby handler.)
 * @param {Object} actor  An instance of an Actor object.
 */
function updateActor(actor) {
  var frameName = actor.frameName;
  var x = actor.x;
  var y = actor.y;
  var z = actor.z;
  var rotZ = actor.rotZ;

  // Sending stuff to SU vs. to O3D.
  if (isSU == true) {
    url = 'skp:push_frame_js@';
    url += 'name=' + actor.name;
    url += '&frame=' + frameName;
    url += '&x=' + x;
    url += '&y=' + y;
    url += '&z=' + z;
    url += '&rotz=' + rotZ;
    window.location.href = url;
  } else {
    // For performance reasons, we want to cache node pointers directly onto
    // our Actor object, so we don't have to look it all up every time.
    if (actor.nodesHaveBeenCached != true) {
      actor.hasBeenCached = true;
      nodes = client.getObjects(actor.colladaID, 'o3d.Transform');
      actor.node = nodes[0];

      var instance = actor.node.children[0];
      actor.children = instance.children;
      actor.nodesHaveBeenCached = true;
    }

    var m = math.matrix4.rotationZYX([0, rotZ, 0]);
    math.matrix4.setTranslation(m, [x, z, -y]);
    actor.node.localMatrix = m;

    if (!isEmpty(frameName)) {
      for (var c = 0; c < actor.children.length; c++) {
        var child = actor.children[c];
        child_name = child.name;
        child.visible = (child_name == frameName ||
            child_name == frameName + '1' ||
            child_name == frameName + '_1');
      }
    }
  }
}

/**
 * Sends our state down to SketchUp or o3d. (See knightgame.rb for the handler.)
 * @param {number} targetX  Where we want the camera to point at.
 * @param {number} targetY  Where we want the camera to point at.
 * @param {number} targetZ  Where we want the camera to point at.
 * @param {number} eyeX  Where we want the camera to point from.
 * @param {number} eyeY  Where we want the camera to point from.
 * @param {number} eyeZ  Where we want the camera to point from.
 */
function updateCamera(targetX, targetY, targetZ, eyeX, eyeY, eyeZ) {
  if (isSU == true) {
    url = 'skp:push_camera@';
    url += 'targetX=' + targetX;
    url += '&targetY=' + targetY;
    url += '&targetZ=' + targetZ;
    url += '&eyeX=' + eyeX;
    url += '&eyeY=' + eyeY;
    url += '&eyeZ=' + eyeZ;
    window.location.href = url;
  } else {
    var target = [targetX, targetZ, -targetY];
    var eye = [eyeX, eyeZ, -eyeY];
    var up = [0, 1, 0];
    var view_matrix = math.matrix4.lookAt(eye, target, up);

    drawContext.view = view_matrix;

    // !lacz modified!
    camera_eye_param.value = eye;
    camera_target_param.value = target;
  }
}

/**
 * Checks for keyboard stuff.
 */
function handleInput() {
  if (keyIsDown[SPACE]) {
    avatar.framesSinceShot = 5;
  }

  if (keyIsDown[DOWN]) {
    if (avatar.animation == 'Hero_Sword') {
      if (ctrlWasJustUp == true) {
        swingAgain = true;
      }
    } else {
      avatar.animation = 'Hero_Sword';
      avatar.frame = 1;
      avatar.swordRate = 1;
      ctrlWasJustUp = false;
    }
  } else {
    ctrlWasJustUp = true;
  }

  if (keyIsDown[UP] && avatar.isJumping != true && upWasJustDown == false) {
    avatar.velZ = JUMP_VELOCITY;
    avatar.isJumping = true;
    upWasJustDown = true;
  }
  if (keyIsDown[UP]) {
    upWasJustDown = true;
  } else {
    upWasJustDown = false;
  }

  if (keyIsDown[RIGHT] || keyIsDown[LEFT]) {
    if (avatar.animation == 'Hero_Stand' && avatar.frame == 1) {
      avatar.frame = 2;
    } else if (avatar.animation == 'Hero_Stand' && avatar.frame == 2) {
      avatar.animation = 'Hero_Run';
      avatar.frame = 1;
    }

  } else {
    if (avatar.animation == 'Hero_Run') {
      avatar.animation = 'Hero_Stand';
      avatar.frame = 2;
    } else if (avatar.animation == 'Hero_Stand' && avatar.frame == 2) {
      avatar.frame = 1;
    }
  }

  if (keyIsDown[RIGHT]) {
    avatar.targetVelX = RUN_VELOCITY;
    avatar.targetRotZ = WORLD_ANGLE;
  } else if (keyIsDown[LEFT]) {
    avatar.targetVelX = -RUN_VELOCITY;
    avatar.targetRotZ = -(Math.PI) + WORLD_ANGLE;
  } else {
    avatar.targetVelX = 0;
  }
}
