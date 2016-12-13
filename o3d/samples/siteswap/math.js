// @@REWRITE(insert js-copyright)
// @@REWRITE(delete-start)
// Copyright 2009 Google Inc.  All Rights Reserved
// @@REWRITE(delete-end)

/**
 * @fileoverview This file contains all the math for the siteswap animator.  It
 * handles all of the site-swap-related stuff [converting a sequence of integers
 * into a more-useful representation of a pattern, pattern validation, etc.] as
 * well as all the physics used for the simulation.
 */

/**
 * This is a container class that holds the coefficients of an equation
 * describing the motion of an object.
 * The basic equation is:
 *   f(x) := a t^2 + b t + c + d sin (f t) + e cos (f t).
 * However, sometimes we LERP between that function and this one:
 *   g(x) := lA t^2  + lB t + lC
 * lerpRate [so far] is always either 1 [LERP from f to g over 1 beat] or -1,
 * [LERP from g to f over one beat].
 *
 * Just plug in t to evaluate the equation.  There's no JavaScript function to
 * do this because it's always done on the GPU.
 *
 * @constructor
 */
EquationCoefficients = function(a, b, c, d, e, f, lA, lB, lC, lerpRate) {
  assert(!isNaN(a) && !isNaN(b) && !isNaN(c));
  d = d || 0;
  e = e || 0;
  f = f || 0;
  assert(!isNaN(d) && !isNaN(e) && !isNaN(f));
  lA = lA || 0;
  lB = lB || 0;
  lC = lC || 0;
  assert(!isNaN(lA) && !isNaN(lB) && !isNaN(lC));
  lerpRate = lerpRate || 0;
  this.a = a;
  this.b = b;
  this.c = c;
  this.d = d;
  this.e = e;
  this.f = f;
  this.lA = lA;
  this.lB = lB;
  this.lC = lC;
  this.lerpRate = lerpRate;
}

/**
 * Create a new equation that's equivalent to this equation's coefficients a-f
 * with a LERP to the polynomial portion of the supplied equation.
 * @param {!EquationCoefficients} eqn the source of coefficients.
 * @param {!number} lerpRate the rate and direction of the LERP; positive for
 *     "from this equation to the new one" and vice-versa.
 * @return {!EquationCoefficients} a new set of coefficients.
 */
EquationCoefficients.prototype.lerpIn = function(eqn, lerpRate) {
  assert(!this.lerpRate);
  return new EquationCoefficients(this.a, this.b, this.c, this.d, this.e,
      this.f, eqn.a, eqn.b, eqn.c, lerpRate);
};

/**
 * Convert the EquationCoefficients to a string for debugging.
 * @return {String} debugging output.
 */
EquationCoefficients.prototype.toString = function() {
  return 'F(t) := ' + this.a.toFixed(2) + ' t^2 + ' + this.b.toFixed(2) +
      ' t + ' + this.c.toFixed(2) + ' + ' +
      this.d.toFixed(2) + ' sin(' + this.f.toFixed(2) + ' t) + ' +
      this.e.toFixed(2) + ' cos(' + this.f.toFixed(2) + ' t) + LERP(' +
      this.lerpRate.toFixed(2) + ') of ' +
      this.lA.toFixed(2) + ' t^2 + ' + this.lB.toFixed(2) +
      ' t + ' + this.lC.toFixed(2);
};

/**
 * A set of equations which describe the motion of an object over time.
 * The three equations each supply one dimension of the motion, and the curve is
 * valid from startTime to startTime + duration.
 * @param {!number} startTime the initial time at which the curve is valid.
 * @param {!number} duration how long [in beats] the curve is valid.
 * @param {!EquationCoefficients} xEqn the equation for motion in x.
 * @param {!EquationCoefficients} yEqn the equation for motion in y.
 * @param {!EquationCoefficients} zEqn the equation for motion in z.
 * @constructor
 */
Curve = function(startTime, duration, xEqn, yEqn, zEqn) {
  this.startTime = startTime;
  this.duration = duration;
  this.xEqn = xEqn;
  this.yEqn = yEqn;
  this.zEqn = zEqn;
}

/**
 * Convert the Curve to a string for debugging.
 * @return {String} debugging output.
 */
Curve.prototype.toString = function() {
  var s = 'startTime: ' + this.startTime + '\n';
  s += 'duration: ' + this.duration + '\n';
  s += this.xEqn + '\n';
  s += this.yEqn + '\n';
  s += this.zEqn + '\n';
  return s;
};

/**
 * Modify this curve's coefficients to include a LERP to the polynomial
 * portion of the supplied curve.
 * @param {!Curve} curve the source of coefficients.
 * @param {!number} lerpRate the rate and direction of the LERP; positive for
 *     "from this equation to the new one" and vice-versa.
 * @return {!Curve} a new curve.
 */
Curve.prototype.lerpIn = function(curve, lerpRate) {
  assert(this.startTime == curve.startTime);
  assert(this.duration == curve.duration);
  var xEqn = this.xEqn.lerpIn(curve.xEqn, lerpRate);
  var yEqn = this.yEqn.lerpIn(curve.yEqn, lerpRate);
  var zEqn = this.zEqn.lerpIn(curve.zEqn, lerpRate);
  return new Curve(this.startTime, this.duration, xEqn, yEqn, zEqn);
};

/**
 * Produce a set of polynomial coefficients that describe linear motion between
 * two points in 1 dimension.
 * @param {!number} startPos the starting position.
 * @param {!number} endPos the ending position.
 * @param {!number} duration how long the motion takes.
 * @return {!EquationCoefficients} the equation for the motion.
 */
Curve.computeLinearCoefficients = function(startPos, endPos, duration) {
  return new EquationCoefficients(
      0, (endPos - startPos) / duration, startPos);
}

var GRAVITY = 1; // Higher means higher throws for the same duration.
/**
 * Produce a set of polynomial coefficients that describe parabolic motion
 * between two points in 1 dimension.
 * @param {!number} startPos the starting position.
 * @param {!number} endPos the ending position.
 * @param {!number} duration how long the motion takes.
 * @return {!EquationCoefficients} the equation for the motion.
 */
Curve.computeParabolicCoefficients = function(startPos, endPos, duration) {
  var dY = endPos - startPos;
  return new EquationCoefficients(-GRAVITY / 2,
                                  dY / duration + GRAVITY * duration / 2,
                                  startPos);
}

/**
 * Compute the curve taken by a ball given its throw and catch positions, the
 * time it was thrown, and how long it stayed in the air.
 *
 * We use duration rather than throwTime and catchTime because, what
 * with the modular arithmetic used in our records, catchTime might be before
 * throwTime, and in some representations the pattern could wrap around a few
 * times while the ball's in the air.  When the parabola computed here is used,
 * time must be supplied as an offset from the time of the throw, and must of
 * course not wrap at all.  That is, these coefficients work for f(0) ==
 * throwPos, f(duration) == catchPos.
 *
 * We treat the y axis as vertical and thus affected by gravity.
 *
 * @param {!EquationCoefficients} throwPos
 * @param {!EquationCoefficients} catchPos
 * @param {!number} startTime
 * @param {!number} duration
 * @return {!Curve}
 */
Curve.computeThrowCurve = function(throwPos, catchPos, startTime, duration) {
  var xEqn = Curve.computeLinearCoefficients(throwPos.x, catchPos.x, duration);
  var yEqn = Curve.computeParabolicCoefficients(throwPos.y, catchPos.y,
      duration);
  var zEqn = Curve.computeLinearCoefficients(throwPos.z, catchPos.z, duration);
  return new Curve(startTime, duration, xEqn, yEqn, zEqn);
}

/**
 * Compute a straight line Curve given start and end positions, the start time,
 * and the duration of the motion.
 *
 * @param {!EquationCoefficients} startPos
 * @param {!EquationCoefficients} endPos
 * @param {!number} startTime
 * @param {!number} duration
 * @return {!Curve}
 */
Curve.computeStraightLineCurve =
    function(startPos, endPos, startTime, duration) {
  var xEqn = Curve.computeLinearCoefficients(startPos.x, endPos.x, duration);
  var yEqn = Curve.computeLinearCoefficients(startPos.y, endPos.y, duration);
  var zEqn = Curve.computeLinearCoefficients(startPos.z, endPos.z, duration);
  return new Curve(startTime, duration, xEqn, yEqn, zEqn);
}

/**
 * Threshold horizontal distance below which computeCircularCurve won't bother
 * trying to approximate a circular curve.  See the comment above
 * computeCircularCurve for more info.
 * @type {number}
 */
Curve.EPSILON = .0001;

/**
 * Compute a circular curve, used as an approximation for the motion of a hand
 * between a catch and its following throw.
 *
 * Assumes a lot of stuff about this looking like a "normal" throw: the catch is
 * moving roughly the opposite direction as the throw, the throw and catch
 * aren't at the same place, and such.  Otherwise this looks very odd at best.
 * This is used for the height of the curve.
 * This produces coefficients for d sin(f t) + e cos(f t) for each of x, y, z.
 * It produces a vertical-ish circular curve from the start to the end, going
 * down, then up.  So if dV [the distance from the start to finish in the x-z
 * plane, ignoring y] is less than Curve.EPSILON, it doesn't know which way down
 * is, and it bails by returning a straight line instead.
 */
Curve.computeCircularCurve = function(startPos, endPos, startTime, duration) {
  var dX = endPos.x - startPos.x;
  var dY = endPos.y - startPos.y;
  var dZ = endPos.z - startPos.z;
  var dV = Math.sqrt(dX * dX + dZ * dZ);
  if (dV < Curve.EPSILON) {
    return Curve.computeStraightLineCurve(startPos, endPos, startTime,
        duration);
  }
  var negHalfdV = -0.5 * dV;
  var negHalfdY = -0.5 * dY;
  var f = Math.PI / duration;
  var yEqn = new EquationCoefficients(
      0, 0, startPos.y + dY / 2,
      negHalfdV, negHalfdY, f);
  var ratio = dX / dV;
  var xEqn = new EquationCoefficients(
      0, 0, startPos.x + dX / 2,
      negHalfdY * ratio, negHalfdV * ratio, f);
  ratio = dZ / dV;
  var zEqn = new EquationCoefficients(
      0, 0, startPos.z + dZ / 2,
      negHalfdY * ratio, negHalfdV * ratio, f);
  return new Curve(startTime, duration, xEqn, yEqn, zEqn);
}

/**
 * This is the abstract base class for an object that describes a throw, catch,
 * or empty hand [placeholder] in a site-swap pattern.
 * @constructor
 */
Descriptor = function() {
}

/**
 * Create an otherwise-identical copy of this descriptor at a given time offset.
 * Note that offset may put time past patternLength; the caller will have to fix
 * this up manually.
 * @param {number} offset how many beats to offset the new descriptor.
 * Derived classes must override this function.
 */
Descriptor.prototype.clone = function(offset) {
  throw new Error('Unimplemented.');
};

/**
 * Generate the Curve implied by this descriptor and the supplied hand
 * positions.
 * @param {!Array.HandPositionRecord} handPositions where the hands will be.
 * Derived classes must override this function.
 */
Descriptor.prototype.generateCurve = function(handPositions) {
  throw new Error('Unimplemented.');
};

/**
 * Adjust the start time of this Descriptor to be in [0, pathLength).
 * @param {!number} pathLength the duration of a path, in beats.
 * @return {!Descriptor} this.
 */
Descriptor.prototype.fixUpModPathLength = function(pathLength) {
  this.time = this.time % pathLength;
  return this;
};

/**
 * This describes a throw in a site-swap pattern.
 * @param {!number} throwNum the site-swap number of the throw.
 * @param {!number} throwTime the time this throw occurs.
 * @param {!number} sourceHand the index of the throwing hand.
 * @param {!number} destHand the index of the catching hand.
 * @constructor
 */
ThrowDescriptor = function(throwNum, throwTime, sourceHand, destHand) {
  this.throwNum = throwNum;
  this.sourceHand = sourceHand;
  this.destHand = destHand;
  this.time = throwTime;
}

/**
 * This is a subclass of Descriptor.
 */
ThrowDescriptor.prototype = new Descriptor();

/**
 * Set up the constructor, just to be neat.
 */
ThrowDescriptor.prototype.constructor = ThrowDescriptor;

/**
 * We label each Descriptor subclass with a type for debugging.
 */
ThrowDescriptor.prototype.type = 'THROW';

/**
 * Create an otherwise-identical copy of this descriptor at a given time offset.
 * Note that offset may put time past patternLength; the caller will have to fix
 * this up manually.
 * @param {number} offset how many beats to offset the new descriptor.
 * @return {!Descriptor} the new copy.
 */
ThrowDescriptor.prototype.clone = function(offset) {
  offset = offset || 0;  // Turn null into 0.
  return new ThrowDescriptor(this.throwNum, this.time + offset,
      this.sourceHand, this.destHand);
};

/**
 * Convert the ThrowDescriptor to a string for debugging.
 * @return {String} debugging output.
 */
ThrowDescriptor.prototype.toString = function() {
  return '(' + this.throwNum + ' from hand ' + this.sourceHand + ' to hand ' +
    this.destHand + ')';
};

/**
 * Generate the Curve implied by this descriptor and the supplied hand
 * positions.
 * @param {!Array.HandPositionRecord} handPositions where the hands will be.
 * @return {!Curve} the curve.
 */
ThrowDescriptor.prototype.generateCurve = function(handPositions) {
  var startPos = handPositions[this.sourceHand].throwPositions[this.destHand];
  var endPos = handPositions[this.destHand].catchPosition;
  return Curve.computeThrowCurve(startPos, endPos, this.time,
      this.throwNum - 1); };

/**
 * This describes a catch in a site-swap pattern.
 * @param {!number} hand the index of the catching hand.
 * @param {!number} sourceThrowNum the site-swap number of the preceeding throw.
 * @param {!number} destThrowNum the site-swap number of the following throw.
 * @param {!number} sourceHand the index of the hand throwing the source throw.
 * @param {!number} destHand the index of the hand catching the following throw.
 * @param {!number} catchTime the time at which the catch occurs.
 * @constructor
 */
CarryDescriptor = function(hand, sourceThrowNum, destThrowNum, sourceHand,
    destHand, catchTime) {
  this.hand = hand;
  this.sourceThrowNum = sourceThrowNum;
  this.destThrowNum = destThrowNum;
  this.sourceHand = sourceHand;
  this.destHand = destHand;
  this.time = catchTime;
}

/**
 * This is a subclass of Descriptor.
 */
CarryDescriptor.prototype = new Descriptor();

/**
 * Set up the constructor, just to be neat.
 */
CarryDescriptor.prototype.constructor = CarryDescriptor;

/**
 * We label each Descriptor subclass with a type for debugging.
 */
CarryDescriptor.prototype.type = 'CARRY';

/**
 * Since this gets pathLength, not patternLength, we'll have to collapse sets
 * of CarryDescriptors later, as they may be spread sparsely through the full
 * animation and we'll only want them to be distributed over the full pattern
 * length.  We may have dupes to throw away as well.
 * @param {!ThrowDescriptor} inThrowDescriptor
 * @param {!ThrowDescriptor} outThrowDescriptor
 * @param {!number} pathLength
 * @return {!CarryDescriptor}
 */
CarryDescriptor.fromThrowDescriptors = function(inThrowDescriptor,
    outThrowDescriptor, pathLength) {
  assert(inThrowDescriptor.destHand == outThrowDescriptor.sourceHand);
  assert((inThrowDescriptor.time + inThrowDescriptor.throwNum) %
      pathLength == outThrowDescriptor.time);
  return new CarryDescriptor(inThrowDescriptor.destHand,
      inThrowDescriptor.throwNum, outThrowDescriptor.throwNum,
      inThrowDescriptor.sourceHand, outThrowDescriptor.destHand,
      (outThrowDescriptor.time + pathLength - 1) % pathLength);
};

/**
 * Create an otherwise-identical copy of this descriptor at a given time offset.
 * Note that offset may put time past patternLength; the caller will have to fix
 * this up manually.
 * @param {number} offset how many beats to offset the new descriptor.
 * @return {!Descriptor} the new copy.
 */
CarryDescriptor.prototype.clone = function(offset) {
  offset = offset || 0;  // Turn null into 0.
  return new CarryDescriptor(this.hand, this.sourceThrowNum,
      this.destThrowNum, this.sourceHand, this.destHand, this.time + offset);
};

/**
 * Convert the CarryDescriptor to a string for debugging.
 * @return {String} debugging output.
 */
CarryDescriptor.prototype.toString = function() {
  return 'time: ' + this.time + ' (hand ' + this.hand + ' catches ' +
    this.sourceThrowNum + ' from hand ' + this.sourceHand + ' then throws ' +
    this.destThrowNum + ' to hand ' + this.destHand + ')';
};

/**
 * Test if this CarryDescriptor is equivalent to another, mod patternLength.
 * @param {!CarryDescriptor} cd the other CarryDescriptor.
 * @param {!number} patternLength the length of the pattern.
 * @return {!bool}
 */
CarryDescriptor.prototype.equalsWithMod = function(cd, patternLength) {
  if (!(cd instanceof CarryDescriptor)) {
    return false;
  }
  if (this.hand != cd.hand) {
    return false;
  }
  if (this.sourceThrowNum != cd.sourceThrowNum) {
    return false;
  }
  if (this.destThrowNum != cd.destThrowNum) {
    return false;
  }
  if (this.sourceHand != cd.sourceHand) {
    return false;
  }
  if (this.destHand != cd.destHand) {
    return false;
  }
  if (this.time % patternLength != cd.time % patternLength) {
    return false;
  }
  return true;
};

/**
 * Generate the Curve implied by this descriptor and the supplied hand
 * positions.
 * @param {!Array.HandPositionRecord} handPositions where the hands will be.
 * @return {!Curve} the curve.
 */
CarryDescriptor.prototype.generateCurve = function(handPositions) {
  var startPos = handPositions[this.hand].catchPosition;
  var endPos = handPositions[this.hand].throwPositions[this.destHand];
  return Curve.computeCircularCurve(startPos, endPos, this.time, 1);
};

/**
 * This describes a carry of a "1" in a site-swap pattern.
 * The flags isThrow and isCatch tell whether this is the actual 1 [isThrow] or
 * the carry that receives the handoff [isCatch].  It's legal for both to be
 * true, which happens when there are two 1s in a row.
 * @param {!number} sourceThrowNum the site-swap number of the prev throw
 * [including this one if isCatch].
 * @param {!number} sourceHand the index of the hand throwing sourceThrowNum.
 * @param {!number} destThrowNum the site-swap number of the next throw
 * [including this one if isThrow].
 * @param {!number} destHand the index of the hand catching destThrowNum.
 * @param {!number} hand the index of the hand doing this carry.
 * @param {!number} time the time at which the carry starts.
 * @param {!bool} isThrow whether this is a 1.
 * @param {!bool} isCatch whether this is the carry after a 1.
 * @constructor
 */
CarryOneDescriptor = function(sourceThrowNum, sourceHand, destThrowNum,
    destHand, hand, time, isThrow, isCatch) {
  // It's possible to have !isCatch with sourceThrowNum == 1 temporarily, if we
  // just haven't handled that 1 yet [we're doing the throw of this one, and
  // will later get to the previous one, due to wraparound], and vice-versa.
  assert(isThrow || (sourceThrowNum == 1));
  assert(isCatch || (destThrowNum == 1));
  this.sourceThrowNum = sourceThrowNum;
  this.sourceHand = sourceHand;
  this.destHand = destHand;
  this.destThrowNum = destThrowNum;
  this.hand = hand;
  this.time = time;
  this.isThrow = isThrow;
  this.isCatch = isCatch;
  return this;
}

/**
 * This is a subclass of Descriptor.
 */
CarryOneDescriptor.prototype = new Descriptor();

/**
 * Set up the constructor, just to be neat.
 */
CarryOneDescriptor.prototype.constructor = CarryOneDescriptor;

/**
 * We label each Descriptor subclass with a type for debugging.
 */
CarryOneDescriptor.prototype.type = 'CARRY_ONE';

/**
 * Create a pair of CarryOneDescriptors to describe the carry that is a throw of
 * 1.  A 1 spends all its time being carried, so these two carries surrounding
 * it represent [and therefore don't have] a throw between them.
 * Prev and post are generally the ordinary CarryDescriptors surrounding the
 * throw of 1 that we're trying to implement.  However, they could each [or
 * both] independently be CarryOneDescriptors implementing other 1 throws.
 * @param {!Descriptor} prev the carry descriptor previous to the 1.
 * @param {!Descriptor} post the carry descriptor subsequent to the 1.
 * @return {!Array.CarryOneDescriptor} a pair of CarryOneDescriptors.
 */
CarryOneDescriptor.getDescriptorPair = function(prev, post) {
  assert(prev instanceof CarryDescriptor || prev instanceof CarryOneDescriptor);
  assert(post instanceof CarryDescriptor || post instanceof CarryOneDescriptor);
  assert(prev.destHand == post.hand);
  assert(prev.hand == post.sourceHand);
  var newPrev;
  var newPost;
  if (prev instanceof CarryOneDescriptor) {
    assert(prev.isCatch && !prev.isThrow);
    newPrev = prev;
    newPrev.isThrow = true;
    assert(newPrev.destHand == post.hand);
  } else {
    newPrev = new CarryOneDescriptor(prev.sourceThrowNum, prev.sourceHand, 1,
        post.hand, prev.hand, prev.time, true, false);
  }
  if (post instanceof CarryOneDescriptor) {
    assert(post.isThrow && !post.isCatch);
    newPost = post;
    newPost.isCatch = true;
    assert(newPost.sourceHand == prev.hand);
    assert(newPost.sourceThrowNum == 1);
  } else {
    newPost = new CarryOneDescriptor(1, prev.hand, post.destThrowNum,
        post.destHand, post.hand, post.time, false, true);
  }
  return [newPrev, newPost];
};

/**
 * Convert the CarryOneDescriptor to a string for debugging.
 * @return {String} debugging output.
 */
CarryOneDescriptor.prototype.toString = function() {
  var s;
  if (this.isThrow) {
    s = 'Hand ' + this.hand + ' catches a ' + this.sourceThrowNum + ' from ' +
        this.sourceHand + ' at time ' + this.time + ' and then passes a 1 to ' +
        this.destHand + '.';
  } else {
    assert(this.isCatch && this.sourceThrowNum == 1);
    s = 'Hand ' + this.hand + ' catches a 1 from ' + this.sourceHand +
        ' at time ' + this.time + ' and then passes a ' + this.destThrowNum +
        ' to ' + this.destHand + '.';
  }
  return s;
};

/**
 * Compute the curve taken by a ball during the carry representing a 1, as long
 * as it's not both a catch and a throw of a 1, which is handled elsewhere.
 * It's either a LERP from a circular curve [a catch of a throw > 1] to a
 * straight line to the handoff point [for isThrow] or a LERP from a straight
 * line from the handoff to a circular curve for the next throw > 1 [for
 * isCatch].
 *
 * @param {!EquationCoefficients} catchPos
 * @param {!EquationCoefficients} throwPos
 * @param {!EquationCoefficients} handoffPos
 * @param {!number} startTime
 * @param {!bool} isCatch whether this is the carry after a 1.
 * @param {!bool} isThrow whether this is a 1.
 * @return {!Curve}
 */
Curve.computeCarryOneCurve = function(catchPos, throwPos, handoffPos, startTime,
    isCatch, isThrow) {
  assert(!isCatch != !isThrow);
  var curve = Curve.computeCircularCurve(catchPos, throwPos, startTime, 1);
  var curve2 = Curve.computeStraightLineCurve(handoffPos, handoffPos,
      startTime, 1);
  return curve.lerpIn(curve2, isThrow ? 1 : -1);
}

/**
 * Compute the curve taken by a ball during the carry representing a 1 that is
 * both the catch of one 1 and the immediately-following throw of another 1.
 *
 * @param {!EquationCoefficients} leadingHandoffPos
 * @param {!EquationCoefficients} trailingHandoffPos
 * @param {!Array.HandPositionRecord} handPositions where the hands will be.
 * @param {!number} hand
 * @param {!number} time the time at which the first 1's catch takes place.
 * @return {!Curve}
 */
Curve.computeConsecutiveCarryOneCurve = function(leadingHandoffPos,
    trailingHandoffPos, handPositions, hand, time) {
  var curve = Curve.computeStraightLineCurve(leadingHandoffPos,
      handPositions[hand].basePosition, time, 1);
  var curve2 =
    Curve.computeStraightLineCurve(handPositions[hand].basePosition,
        trailingHandoffPos, time, 1);
  return curve.lerpIn(curve2, 1);
}

/**
 * Generate the Curve implied by this descriptor and the supplied hand
 * positions.
 * @param {!Array.HandPositionRecord} handPositions where the hands will be.
 * @return {!Curve} the curve.
 */
CarryOneDescriptor.prototype.generateCurve = function(handPositions) {
  var leadingHandoffPos, trailingHandoffPos;
  if (this.isCatch) {
    var p0 = handPositions[this.hand].basePosition;
    var p1 = handPositions[this.sourceHand].basePosition;
    handoffPos = leadingHandoffPos = p0.add(p1).scale(0.5);
  }
  if (this.isThrow) {
    var p0 = handPositions[this.hand].basePosition;
    var p1 = handPositions[this.destHand].basePosition;
    handoffPos = trailingHandoffPos = p0.add(p1).scale(0.5);
  }
  if (!this.isCatch || !this.isThrow) {
    return Curve.computeCarryOneCurve(handPositions[this.hand].catchPosition,
        handPositions[this.hand].throwPositions[this.destHand], handoffPos,
        this.time, this.isCatch, this.isThrow);
  } else {
    return Curve.computeConsecutiveCarryOneCurve(leadingHandoffPos,
        trailingHandoffPos, handPositions, this.hand, this.time);
  }
};

/**
 * Create an otherwise-identical copy of this descriptor at a given time offset.
 * Note that offset may put time past patternLength; the caller will have to fix
 * this up manually.
 * @param {number} offset how many beats to offset the new descriptor.
 * @return {!Descriptor} the new copy.
 */
CarryOneDescriptor.prototype.clone = function(offset) {
  offset = offset || 0;  // Turn null into 0.
  return new CarryOneDescriptor(this.sourceThrowNum, this.sourceHand,
      this.destThrowNum, this.destHand, this.hand, this.time + offset,
      this.isThrow, this.isCatch);
};

/**
 * Test if this CarryOneDescriptor is equivalent to another, mod patternLength.
 * @param {!CarryOneDescriptor} cd the other CarryOneDescriptor.
 * @param {!number} patternLength the length of the pattern.
 * @return {!bool}
 */
CarryOneDescriptor.prototype.equalsWithMod = function(cd, patternLength) {
  if (!(cd instanceof CarryOneDescriptor)) {
    return false;
  }
  if (this.hand != cd.hand) {
    return false;
  }
  if (this.sourceThrowNum != cd.sourceThrowNum) {
    return false;
  }
  if (this.destThrowNum != cd.destThrowNum) {
    return false;
  }
  if (this.sourceHand != cd.sourceHand) {
    return false;
  }
  if (this.destHand != cd.destHand) {
    return false;
  }
  if (this.isCatch != cd.isCatch) {
    return false;
  }
  if (this.isThrow != cd.isThrow) {
    return false;
  }
  if (this.time % patternLength != cd.time % patternLength) {
    return false;
  }
  return true;
};

/**
 * This describes an empty hand in a site-swap pattern.
 * @param {!Descriptor} cd0 the CarryDescriptor or CarryOneDescriptor describing
 * this hand immediately before it was emptied.
 * @param {!Descriptor} cd1 the CarryDescriptor or CarryOneDescriptor describing
 * this hand immediately after it's done being empty.
 * @param {!number} patternLength the length of the pattern.
 * @constructor
 */
EmptyHandDescriptor = function(cd0, cd1, patternLength) {
  assert(cd0.hand == cd1.hand);
  this.hand = cd0.hand;
  this.prevThrowDest = cd0.destHand;
  this.sourceThrowNum = cd0.destThrowNum;
  this.nextCatchSource = cd1.sourceHand;
  this.destThrowNum = cd1.sourceThrowNum;
  // This code assumes that each CarryDescriptor and CarryOneDescriptor always
  // has a duration of 1 beat.  If we want to be able to allow long-held balls
  // [instead of thrown twos, for example], we'll have to fix that here and a
  // number of other places.
  this.time = (cd0.time + 1) % patternLength;
  this.duration = cd1.time - this.time;
  if (this.duration < 0) {
    this.duration += patternLength;
    assert(this.duration > 0);
  }
}

/**
 * This is a subclass of Descriptor.
 */
EmptyHandDescriptor.prototype = new Descriptor();

/**
 * Set up the constructor, just to be neat.
 */
EmptyHandDescriptor.prototype.constructor = EmptyHandDescriptor;

/**
 * We label each Descriptor subclass with a type for debugging.
 */
EmptyHandDescriptor.prototype.type = 'EMPTY';

/**
 * Convert the EmptyHandDescriptor to a string for debugging.
 * @return {String} debugging output.
 */
EmptyHandDescriptor.prototype.toString = function() {
  return 'time: ' + this.time + ' for ' + this.duration + ' (hand ' +
      this.hand + ', after throwing a ' + this.sourceThrowNum + ' to hand ' +
      this.prevThrowDest + ' then catches a ' + this.destThrowNum +
      ' from hand ' + this.nextCatchSource + ')';
};

/**
 * Generate the Curve implied by this descriptor and the supplied hand
 * positions.
 * @param {!Array.HandPositionRecord} handPositions where the hands will be.
 * @return {!Curve} the curve.
 */
EmptyHandDescriptor.prototype.generateCurve = function(handPositions) {
  var startPos, endPos;
  if (this.sourceThrowNum == 1) {
    var p0 = handPositions[this.hand].basePosition;
    var p1 = handPositions[this.prevThrowDest].basePosition;
    startPos = p0.add(p1).scale(0.5);
  } else {
    startPos = handPositions[this.hand].throwPositions[this.prevThrowDest];
  }
  if (this.destThrowNum == 1) {
    var p0 = handPositions[this.hand].basePosition;
    var p1 = handPositions[this.nextCatchSource].basePosition;
    endPos = p0.add(p1).scale(0.5);
  } else {
    endPos = handPositions[this.hand].catchPosition;
  }
  // TODO: Replace with a good empty-hand curve.
  return Curve.computeStraightLineCurve(startPos, endPos, this.time,
      this.duration);
};

/**
 * A series of descriptors that describes the full path of an object during a
 * pattern.
 * @param {!Array.Descriptor} descriptors all descriptors for the object.
 * @param {!number} pathLength the length of the path in beats.
 * @constructor
 */
Path = function(descriptors, pathLength) {
  this.descriptors = descriptors;
  this.pathLength = pathLength;
}

/**
 * Create a Path representing a ball, filling in the gaps between the throws
 * with carry descriptors.  Since it's a ball's path, there are no
 * EmptyHandDescriptors in the output.
 * @param {!Array.ThrowDescriptor} throwDescriptors the ball's part of the
 * pattern.
 * @param {!number} pathLength the length of the pattern in beats.
 * @return {!Path} the ball's full path.
 */
Path.ballPathFromThrowDescriptors = function(throwDescriptors, pathLength) {
  return new Path(
      Path.createDescriptorList(throwDescriptors, pathLength), pathLength);
};

/**
 * Create the sequence of ThrowDescriptors, CarryDescriptors, and
 * CarryOneDescriptor describing the path of a ball through a pattern.
 * A sequence such as (h j k) generally maps to an alternating series of throw
 * and carry descriptors [Th Chj Tj Cjk Tk Ck? ...].  However, when j is a 1,
 * you remove the throw descriptor and modify the previous and subsequent carry
 * descriptors, since the throw descriptor has zero duration and the carry
 * descriptors need to take into account the handoff.
 * @param {!Array.ThrowDescriptor} throwDescriptors the ball's part of the
 * pattern.
 * @param {!number} pathLength the length of the pattern in beats.
 * @return {!Array.Descriptor} the full set of descriptors for the ball.
 */
Path.createDescriptorList = function(throwDescriptors, pathLength) {
  var descriptors = [];
  var prevThrow;
  for (var index in throwDescriptors) {
    var td = throwDescriptors[index];
    if (prevThrow) {
      descriptors.push(
          CarryDescriptor.fromThrowDescriptors(prevThrow, td, pathLength));
    } // Else it's handled after the loop.
    descriptors.push(td);
    prevThrow = td;
  }
  descriptors.push(
      CarryDescriptor.fromThrowDescriptors(prevThrow, throwDescriptors[0],
        pathLength));
  // Now post-process to take care of throws of 1.  It's easier to do it here
  // than during construction since we can now assume that the previous and
  // subsequent carry descriptors are already in place [modulo pathLength].
  for (var i = 0; i < descriptors.length; ++i) {
    var descriptor = descriptors[i];
    if (descriptor instanceof ThrowDescriptor) {
      if (descriptor.throwNum == 1) {
        var prevIndex = (i + descriptors.length - 1) % descriptors.length;
        var postIndex = (i + 1) % descriptors.length;
        var replacements = CarryOneDescriptor.getDescriptorPair(
            descriptors[prevIndex], descriptors[postIndex]);
        descriptors[prevIndex] = replacements[0];
        descriptors[postIndex] = replacements[1];
        descriptors.splice(i, 1);
        // We've removed a descriptor from the array, but since we can never
        // have 2 ThrowDescriptors in a row, we don't need to decrement i.
      }
    }
  }
  return descriptors;
};

/**
 * Convert the Path to a string for debugging.
 * @return {String} debugging output.
 */
Path.prototype.toString = function() {
  var ret = 'pathLength is ' + this.pathLength + '; [';
  for (var index in this.descriptors) {
    ret += this.descriptors[index].toString();
  }
  ret += ']';
  return ret;
};

/**
 * Create an otherwise-identical copy of this path at a given time offset.
 * Note that offset may put time references in the Path past the length of the
 * pattern.  The caller must fix this up manually.
 * @param {number} offset how many beats to offset the new Path.
 * @return {!Path} the new copy.
 */
Path.prototype.clone = function(offset) {
  offset = offset || 0;  // Turn null into 0.
  var descriptors = [];
  for (var index in this.descriptors) {
    descriptors.push(this.descriptors[index].clone(offset));
  }
  return new Path(descriptors, this.pathLength);
};

/**
 * Adjust the start time of all descriptors to be in [0, pathLength) via modular
 * arithmetic.  Reorder the array such that they're sorted in increasing order
 * of time.
 * @return {!Path} this.
 */
Path.prototype.fixUpModPathLength = function() {
  var splitIndex;
  var prevTime = 0;
  for (var index in this.descriptors) {
    var d = this.descriptors[index];
    d.fixUpModPathLength(this.pathLength);
    if (d.time < prevTime) {
      assert(null == splitIndex);
      splitIndex = index; // From here to the end should move to the start.
    }
    prevTime = d.time;
  }
  if (null != splitIndex) {
    var temp = this.descriptors.slice(splitIndex);
    this.descriptors.length = splitIndex;
    this.descriptors = temp.concat(this.descriptors);
  }
  return this;
};

/**
 * Take a standard asynch siteswap pattern [expressed as an array of ints] and
 * a number of hands, and expand it into a 2D grid of ThrowDescriptors with one
 * row per hand.
 * Non-asynch patterns are more complicated, since their linear forms aren't
 * fully-specified, so we don't handle them here.
 * You'll want to expand your pattern to the LCM of numHands and minimal pattern
 * length before calling this.
 * The basic approach doesn't really work for one-handed patterns.  It ends up
 * with catches and throws happening at the same time [having removed all
 * empty-hand time in between them].  To fix this, we double all throw heights
 * and space them out, as if doing a two-handed pattern with all zeroes from the
 * other hand.  Yes, this points out that the overall approach we're taking is a
 * bit odd [since you end up with hands empty for time proportional to the
 * number of hands], but you have to make some sort of assumptions to generalize
 * siteswaps to N hands, and that's what I chose.
 * @param {!Array.number} pattern an asynch siteswap pattern.
 * @param {!number} numHands the number of hands.
 * @return {!Array.Array.ThrowDescriptor} the expanded pattern.
 */
function expandPattern(pattern, numHands) {
  var fullPattern = [];
  assert(numHands > 0);
  if (numHands == 1) {
    numHands = 2;
    var temp = [];
    for (var i = 0; i < pattern.length; ++i) {
      temp[2 * i] = 2 * pattern[i];
      temp[2 * i + 1] = 0;
    }
    pattern = temp;
  }
  for (var hand = 0; hand < numHands; ++hand) {
    fullPattern[hand] = [];
  }
  for (var time = 0; time < pattern.length; ++time) {
    for (var hand = 0; hand < numHands; ++hand) {
      var t;
      if (hand == time % numHands) {
        t = new ThrowDescriptor(pattern[time], time, hand,
            (hand + pattern[time]) % numHands);
      } else {
        // These are ignored during analysis, so they don't appear in BallPaths.
        t = new ThrowDescriptor(0, time, hand, hand);
      }
      fullPattern[hand].push(t);
    }
  }
  return fullPattern;
}

// TODO: Wrap the final pattern in a class, then make the remaining few global
// functions be members of that class to clean up the global namespace.

/**
 * Given a valid site-swap for a nonzero number of balls, stored as an expanded
 * pattern array-of-arrays, with pattern length the LCM of hands and minimal
 * pattern length, produce Paths for all the balls.
 * @param {!Array.Array.ThrowDescriptor} pattern a valid pattern.
 * @return {!Array.Path} the paths of all the balls.
 */
function generateBallPaths(pattern) {
  var numHands = pattern.length;
  assert(numHands > 0);
  var patternLength = pattern[0].length;
  assert(patternLength > 0);
  var sum = 0;
  for (var hand in pattern) {
    for (var time in pattern[hand]) {
      sum += pattern[hand][time].throwNum;
    }
  }
  var numBalls = sum / patternLength;
  assert(numBalls == Math.round(numBalls));
  assert(numBalls > 0);

  var ballsToAllocate = numBalls;
  var ballPaths = [];
  // NOTE: The indices of locationsChecked are reversed from those of pattern
  // for simplicity of allocation.  This might be worth flipping to match.
  var locationsChecked = [];
  for (var time = 0; time < patternLength && ballsToAllocate; ++time) {
    locationsChecked[time] = locationsChecked[time] || [];
    for (var hand = 0; hand < numHands && ballsToAllocate; ++hand) {
      if (locationsChecked[time][hand]) {
        continue;
      }
      var curThrowDesc = pattern[hand][time];
      var curThrow = curThrowDesc.throwNum;
      if (!curThrow) {
        assert(curThrow === 0);
        continue;
      }
      var throwDescriptors = [];
      var curTime = time;
      var curHand = hand;
      var wraps = 0;
      do {
        if (!locationsChecked[curTime]) {
          locationsChecked[curTime] = [];
        }
        assert(!locationsChecked[curTime][curHand]);
        locationsChecked[curTime][curHand] = true;
        // We copy curThrowDesc here, adding wraps * patternLength, to get
        // the true throw time relative to offset.  Later we'll add in offset
        // when we clone again, then mod by pathLength.
        throwDescriptors.push(curThrowDesc.clone(wraps * patternLength));
        var nextThrowTime = curThrow + curTime;
        wraps += Math.floor(nextThrowTime / patternLength);
        curTime = nextThrowTime % patternLength;
        assert(curTime >= time); // Else we'd have covered it earlier.
        curHand = curThrowDesc.destHand;
        var tempThrowDesc = curThrowDesc;
        curThrowDesc = pattern[curHand][curTime];
        curThrow = curThrowDesc.throwNum;
        assert(tempThrowDesc.destHand == curThrowDesc.sourceHand);
        assert(curThrowDesc.time ==
            (tempThrowDesc.throwNum + tempThrowDesc.time) % patternLength);
      } while (curTime != time || curHand != hand);
      var pathLength = wraps * patternLength;
      var ballPath =
        Path.ballPathFromThrowDescriptors(throwDescriptors, pathLength);
      for (var i = 0; i < wraps; ++i) {
        var offset = i * patternLength % pathLength;
        ballPaths.push(ballPath.clone(offset, pathLength).fixUpModPathLength());
      }
      ballsToAllocate -= wraps;
      assert(ballsToAllocate >= 0);
    }
  }
  return ballPaths;
}

/**
 * Given an array of ball paths, produce the corresponding set of hand paths.
 * @param {!Array.Path} ballPaths the Paths of all the balls in the pattern.
 * @param {!number} numHands how many hands to use in the pattern.
 * @param {!number} patternLength the length, in beats, of the pattern.
 * @return {!Array.Path} the paths of all the hands.
 */
function generateHandPaths(ballPaths, numHands, patternLength) {
  assert(numHands > 0);
  assert(patternLength > 0);
  var handRecords = []; // One record per hand.
  for (var idxBR in ballPaths) {
    var descriptors = ballPaths[idxBR].descriptors;
    for (var idxD in descriptors) {
      var descriptor = descriptors[idxD];
      // TODO: Fix likely needed for throws of 1.
      if (!(descriptor instanceof ThrowDescriptor)) {
        // It's a CarryDescriptor or a CarryOneDescriptor.
        var hand = descriptor.hand;
        if (!handRecords[hand]) {
          handRecords[hand] = [];
        }
        // TODO: Should we not shorten stuff here if we're going to lengthen
        // everything later anyway?  Is there a risk of inconsistency due to
        // ball paths of different lengths?
        var catchTime = descriptor.time % patternLength;
        if (!handRecords[hand][catchTime]) {
          // We pass in this offset to set the new descriptor's time to
          // catchTime, so as to keep it within [0, patternLength).
          handRecords[hand][catchTime] =
              descriptor.clone(catchTime - descriptor.time);
        } else {
          assert(
              handRecords[hand][catchTime].equalsWithMod(
                  descriptor, patternLength));
        }
      }
    }
  }
  var handPaths = [];
  for (var hand in handRecords) {
    var outDescriptors = [];
    var inDescriptors = handRecords[hand];
    var prevDescriptor = null;
    var descriptor;
    for (var idxD in inDescriptors) {
      descriptor = inDescriptors[idxD];
      assert(descriptor);  // Enumeration should skip array holes.
      assert(descriptor.hand == hand);
      if (prevDescriptor) {
        outDescriptors.push(new EmptyHandDescriptor(prevDescriptor, descriptor,
              patternLength));
      }
      outDescriptors.push(descriptor.clone());
      prevDescriptor = descriptor;
    }
    // Note that this EmptyHandDescriptor that wraps around the end lives at the
    // end of the array, not the beginning, despite the fact that it may be the
    // active one at time zero.  This is the same behavior as with Paths for
    // balls.
    descriptor = new EmptyHandDescriptor(prevDescriptor, outDescriptors[0],
        patternLength);
    if (descriptor.time < outDescriptors[0].time) {
      assert(descriptor.time + descriptor.duration == outDescriptors[0].time);
      outDescriptors.unshift(descriptor);
    } else {
      assert(descriptor.time ==
          outDescriptors[outDescriptors.length - 1].time + 1);
      outDescriptors.push(descriptor);
    }
    handPaths[hand] =
        new Path(outDescriptors, patternLength).fixUpModPathLength();
  }
  return handPaths;
}

// NOTE: All this Vector stuff does lots of object allocations.  If that's a
// problem for your browser [e.g. IE6], you'd better stick with the embedded V8.
// This code predates the creation of o3djs/math.js; I should probably switch it
// over at some point, but for now it's not worth the trouble.

/**
 * A simple 3-dimensional vector.
 * @constructor
 */
Vector = function(x, y, z) {
  this.x = x;
  this.y = y;
  this.z = z;
}

Vector.prototype.sub = function(v) {
  return new Vector(this.x - v.x, this.y - v.y, this.z - v.z);
};

Vector.prototype.add = function(v) {
  return new Vector(this.x + v.x, this.y + v.y, this.z + v.z);
};

Vector.prototype.dot = function(v) {
  return this.x * v.x + this.y * v.y + this.z * v.z;
};

Vector.prototype.length = function() {
  return Math.sqrt(this.dot(this));
};

Vector.prototype.scale = function(s) {
  return new Vector(this.x * s, this.y * s, this.z * s);
};

Vector.prototype.set = function(v) {
  this.x = v.x;
  this.y = v.y;
  this.z = v.z;
};

Vector.prototype.normalize = function() {
  var length = this.length();
  assert(length);
  this.set(this.scale(1 / length));
  return this;
};

/**
 * Convert the Vector to a string for debugging.
 * @return {String} debugging output.
 */
Vector.prototype.toString = function() {
  return '{' + this.x.toFixed(3) + ', ' + this.y.toFixed(3) + ', ' +
      this.z.toFixed(3) + '}';
};

/**
 * A container class that holds the positions relevant to a hand: where it is
 * when it's not doing anything, where it likes to catch balls, and where it
 * likes to throw balls to each of the other hands.
 * @param {!Vector} basePosition the centroid of throw and catch positions when
 * the hand throws to itself.
 * @param {!Vector} catchPosition where the hand likes to catch balls.
 * @constructor
 */
HandPositionRecord = function(basePosition, catchPosition) {
  this.basePosition = basePosition;
  this.catchPosition = catchPosition;
  this.throwPositions = [];
}

/**
 * Convert the HandPositionRecord to a string for debugging.
 * @return {String} debugging output.
 */
HandPositionRecord.prototype.toString = function() {
  var s = 'base: ' + this.basePosition.toString() + ';\n';
  s += 'catch: ' + this.catchPosition.toString() + ';\n';
  s += 'throws:\n';
  for (var i = 0; i < this.throwPositions.length; ++i) {
    s += '[' + i + '] ' + this.throwPositions[i].toString() + '\n';
  }
  return s;
};

/**
 * Compute all the hand positions used in a pattern given a number of hands and
 * a grouping style ["even" for evenly-spaced hands, "pairs" to group them in
 * pairs, as with 2-handed jugglers].
 * @param {!number} numHands the number of hands to use.
 * @param {!String} style the grouping style.
 * @return {!Array.HandPositionRecord} a full set of hand positions.
 */
function computeHandPositions(numHands, style) {
  assert(numHands > 0);
  var majorRadiusScale = 0.75;
  var majorRadius = majorRadiusScale * (numHands - 1);
  var throwCatchOffset = 0.45;
  var catchRadius = majorRadius + throwCatchOffset;
  var handPositionRecords = [];
  for (var hand = 0; hand < numHands; ++hand) {
    var circleFraction;
    if (style == 'even') {
      circleFraction = hand / numHands;
    } else {
      assert(style == 'pairs');
      circleFraction = (hand + Math.floor(hand / 2)) / (1.5 * numHands);
    }
    var cos = Math.cos(Math.PI * 2 * circleFraction);
    var sin = Math.sin(Math.PI * 2 * circleFraction);
    var cX = catchRadius * cos;
    var cY = 0;
    var cZ = catchRadius * sin;
    var bX = majorRadius * cos;
    var bY = 0;
    var bZ = majorRadius * sin;
    handPositionRecords[hand] = new HandPositionRecord(
      new Vector(bX, bY, bZ), new Vector(cX, cY, cZ));
  }
  // Now that we've got all the hands' base and catch positions, we need to
  // compute the appropriate throw positions for each hand pair.
  for (var source = 0; source < numHands; ++source) {
    var throwHand = handPositionRecords[source];
    for (var target = 0; target < numHands; ++target) {
      var catchHand = handPositionRecords[target];
      if (throwHand == catchHand) {
        var baseV = throwHand.basePosition;
        throwHand.throwPositions[target] =
            baseV.add(baseV.sub(throwHand.catchPosition));
      } else {
        var directionV =
            catchHand.catchPosition.sub(throwHand.basePosition).normalize();
        var offsetV = directionV.scale(throwCatchOffset);
        throwHand.throwPositions[target] =
            throwHand.basePosition.add(offsetV);
      }
    }
  }
  return handPositionRecords;
}

/**
 * Convert an array of HandPositionRecord to a string for debugging.
 * @param {!Array.HandPositionRecord} positions the positions to display.
 * @return {String} debugging output.
 */
function getStringFromHandPositions(positions) {
  var s = '';
  for (index in positions) {
    s += positions[index].toString();
  }
  return s;
}

/**
 * The set of curves an object passes through throughout a full animation cycle.
 * @param {!number} duration the length of the animation in beats.
 * @param {!Array.Curve} curves the full set of Curves.
 * @constructor
 */
CurveSet = function(duration, curves) {
  this.duration = duration;
  this.curves = curves;
}

/**
 * Looks up what curve is active at a particular time.  This is slower than
 * getCurveForTime, but can be used even if no Curve starts precisely at
 * unsafeTime % this.duration.
 * @param {!number} unsafeTime the time at which to check.
 * @return {!Curve} the curve active at unsafeTime.
 */
CurveSet.prototype.getCurveForUnsafeTime = function(unsafeTime) {
  unsafeTime %= this.duration;
  time = Math.floor(unsafeTime);
  if (this.curves[time]) {
    return this.curves[time];
  }
  var curve;
  for (var i = time; i >= 0; --i) {
    curve = this.curves[i];
    if (curve) {
      assert(i + curve.duration >= unsafeTime);
      return curve;
    }
  }
  // We must want the last one.  There's always a last one, given how we
  // construct the CurveSets; they're sparse, but the length gets set by adding
  // elements at the end.
  curve = this.curves[this.curves.length - 1];
  unsafeTime += this.duration;
  assert(curve.startTime <= unsafeTime);
  assert(curve.startTime + curve.duration > unsafeTime);
  return curve;
};

/**
 * Looks up what curve is active at a particular time.  This is faster than
 * getCurveForUnsafeTime, but can only be used if if a Curve starts precisely at
 * unsafeTime % this.duration.
 * @param {!number} time the time at which to check.
 * @return {!Curve} the curve starting at time.
 */
CurveSet.prototype.getCurveForTime = function(time) {
  return this.curves[time % this.duration];
};

/**
 * Convert the CurveSet to a string for debugging.
 * @return {String} debugging output.
 */
CurveSet.prototype.toString = function() {
  var s = 'Duration: ' + this.duration + '\n';
  for (var c in this.curves) {
    s += this.curves[c].toString();
  }
  return s;
};

/**
 * Namespace object to hold the pure math functions.
 * TODO: Consider just rolling these into the Pattern object, when it gets
 * created.
 */
var JugglingMath = {};

/**
 * Computes the greatest common devisor of integers a and b.
 * @param {!number} a an integer.
 * @param {!number} b an integer.
 * @return {!number} the GCD of a and b.
 */
JugglingMath.computeGCD = function(a, b) {
  assert(Math.round(a) == a);
  assert(Math.round(b) == b);
  assert(a >= 0);
  assert(b >= 0);
  if (!b) {
    return a;
  } else {
    return JugglingMath.computeGCD(b, a % b);
  }
}

/**
 * Computes the least common multiple of integers a and b, by making use of the
 * fact that LCM(a, b) * GCD(a, b) == a * b.
 * @param {!number} a an integer.
 * @param {!number} b an integer.
 * @return {!number} the LCM of a and b.
 */
JugglingMath.computeLCM = function(a, b) {
  assert(Math.round(a) == a);
  assert(Math.round(b) == b);
  assert(a >= 0);
  assert(b >= 0);
  var ret = a * b / JugglingMath.computeGCD(a, b);
  assert(Math.round(ret) == ret);
  return ret;
}

/**
 * Given a Path and a set of hand positions, compute the corresponding set of
 * Curves.
 * @param {!Path} path the path of an object.
 * @param {!Array.HandPositionRecord} handPositions the positions of the hands
 * juggling the pattern containing the path.
 * @return {!CurveSet} the full set of curves.
 */
CurveSet.getCurveSetFromPath = function(path, handPositions) {
  var curves = [];
  var pathLength = path.pathLength;
  for (var index in path.descriptors) {
    var descriptor = path.descriptors[index];
    var curve = descriptor.generateCurve(handPositions);
    assert(!curves[curve.startTime]);
    assert(curve.startTime < pathLength);
    curves[curve.startTime] = curve;
  }
  return new CurveSet(pathLength, curves);
}

/**
 * Given a set of Paths and a set of hand positions, compute the corresponding
 * CurveSets.
 * @param {!Array.Path} paths the paths of a number of objects.
 * @param {!Array.HandPositionRecord} handPositions the positions of the hands
 * juggling the pattern containing the paths.
 * @return {!Array.CurveSet} the CurveSets.
 */
CurveSet.getCurveSetsFromPaths = function(paths, handPositions) {
  var curveSets = [];
  for (var index in paths) {
    var path = paths[index];
    curveSets[index] = CurveSet.getCurveSetFromPath(path, handPositions);
  }
  return curveSets;
}

/**
 * This is a temporary top-level calculation function that converts a standard
 * asynchronous siteswap, expressed as a string of digits, into a full
 * ready-to-animate set of CurveSets.  Later on we'll be using an interface that
 * can create a richer set of patterns than those expressable in the traditional
 * string-of-ints format.
 * @param {!String} patternString the siteswap.
 * @param {!number} numHands the number of hands to use for the pattern.
 * @param {!String} style how to space the hands ["pairs" or "even"].
 * @return {!Object} a fully-analyzed pattern as CurveSets and associated data.
 */
function computeFullPatternFromString(patternString, numHands, style) {
  var patternAsStrings = patternString.split(/[ ,]+ */);
  var patternSegment = [];
  for (var index in patternAsStrings) {
    if (patternAsStrings[index]) {  // Beware extra whitespace at the ends.
      patternSegment.push(parseInt(patternAsStrings[index]));
    }
  }
  var pattern = [];
  // Now expand the pattern out to the length of the LCM of pattern length and
  // number of hands, so that each throw gets done in each of its incarnations.
  var multiple = JugglingMath.computeLCM(patternSegment.length, numHands) /
      patternSegment.length;
  for (var i = 0; i < multiple; ++i) {
    pattern = pattern.concat(patternSegment);
  }

  var fullPattern = expandPattern(pattern, numHands);
  var patternLength = fullPattern[0].length;

  var ballPaths = generateBallPaths(fullPattern);
  var handPaths = generateHandPaths(ballPaths, numHands, patternLength);

  var handPositions = computeHandPositions(numHands, style);
  var ballCurveSets = CurveSet.getCurveSetsFromPaths(ballPaths, handPositions);
  var handCurveSets = CurveSet.getCurveSetsFromPaths(handPaths, handPositions);

  // Find the LCM of all the curveSet durations.  This will be the length of the
  // fully-expanded queue.  We could expand to this before computing the
  // CurveSets, but this way's probably just a little cheaper.
  var lcmDuration = 1;
  for (var i in ballCurveSets) {
    var duration = ballCurveSets[i].duration;
    if (duration > lcmDuration || lcmDuration % duration) {
      lcmDuration = JugglingMath.computeLCM(lcmDuration, duration);
    }
  }
  for (var i in handCurveSets) {
    var duration = handCurveSets[i].duration;
    if (duration > lcmDuration || lcmDuration % duration) {
      lcmDuration = JugglingMath.computeLCM(lcmDuration, duration);
    }
  }
  return {
    numBalls: ballPaths.length,
    numHands: handPaths.length,
    duration: lcmDuration,
    handCurveSets: handCurveSets,
    ballCurveSets: ballCurveSets
  }
}
