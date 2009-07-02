// @@REWRITE(insert js-copyright)
// @@REWRITE(delete-start)
// Copyright 2009 Google Inc.  All Rights Reserved
// @@REWRITE(delete-end)

/**
 * This file contains the animation-management code for the siteswap animator.
 * This is encapsulated in the EventQueue and QueueEvent classes, the event
 * handler, and startAnimation, the main external interface to the animation.
 */

/**
 * A record, held in the EventQueue, that describes the curve that should be
 * given to a shape at a time.
 * @constructor
 * @param {!number} time base time at which the event occurs/the curve starts.
 * @param {!o3d.Shape} shape the shape to be updated.
 * @param {Curve} curve the path for the shape to follow.
 */
function QueueEvent(time, shape, curve) {
  this.time = time;
  this.shape = shape;
  this.curve = curve;
  return this;
}

/**
 * A circular queue of events that will happen during the course of an animation
 * that's duration beats long.  The queue is ordered by the time each curve
 * starts.  Note that a curve may start after it ends, since time loops
 * endlessly.  The nextEvent field is the index of the next event to occur; it
 * keeps track of how far we've gotten in the queue.
 * @constructor
 * @param {!number} duration the length of the animation in beats.
 */
function EventQueue(duration) {
  this.events = [];
  this.nextEvent = 0;
  this.duration = duration;
  this.timeCorrection = 0; // Corrects from queue entry time to real time.
  return this;
}

/**
 * Add an event to the queue, inserting into order by its time field.
 * A heap-based priority queue would be faster, but likely overkill, as this
 * won't ever contain that many items, and isn't likely to be speed-critical.
 * @param {!QueueEvent} event the event to add.
 */
EventQueue.prototype.addEvent = function(event) {
  var i = 0;
  while (i < this.events.length && event.time > this.events[i].time) {
    ++i;
  }
  this.events.splice(i, 0, event);
};

/**
 * Pull the next event off the queue.
 * @return {!QueueEvent} the event.
 */
EventQueue.prototype.shift = function() {
  var e = this.events[this.nextEvent];
  if (++this.nextEvent >= this.events.length) {
    assert(this.nextEvent > 0);
    this.nextEvent = 0;
    this.timeCorrection += this.duration;
  }
  return e;
};

/**
 * Process all current events, updating all animated objects with their new
 * curves, until we find that the next event in the queue is in the future.
 * @param {!number} time the current time in beats.  This number is an absolute,
 * not locked to the range of the duration of the pattern, so getNextTime()
 * returns a doctored number to add in the offset from in-pattern time to real
 * time.
 * @return {!number} the time of the next future event.
 */
EventQueue.prototype.processEvents = function(time) {
  while (this.getNextTime() <= time) {
    var e = this.shift();
    setParamCurveInfo(e.curve, e.shape, time);
  }
  return this.getNextTime();  // In case you want to set a callback.
};

/**
 * Look up the initial curve for a shape [the curve that it'll be starting or in
 * the middle of at time 0].
 * @param {!CurveSet} curveSet the complete set of curves for a Shape.
 * @return {!Object} the curve and the time at which it would have started.
 */
function getInitialCurveInfo(curveSet) {
  var curve = curveSet.getCurveForUnsafeTime(0);
  var curveBaseTime;
  if (!curve.startTime) {
    curveBaseTime = 0;
  } else {
    // If the curve isn't starting now, it must have wrapped around.
    assert(curve.startTime + curve.duration > curveSet.duration);
    // So subtract off one wrap so that its startTime is in the right space of
    // numbers.  We assume here that no curve duration is longer than the
    // pattern, which must be guaranteed by the code that generates patterns.
    assert(curve.duration <= curveSet.duration);
    curveBaseTime = curve.startTime - curveSet.duration;
  }
  return { curve: curve, curveBaseTime: curveBaseTime };
}

/**
 * Set up the event queue with a complete pattern starting at time 0.
 * @param {!Array.CurveSet} curveSets the curve sets for all shapes.
 * @param {!Array.o3d.Shape} shapes all the shapes to animate.
 */
EventQueue.prototype.setUp = function(curveSets, shapes) {
  assert(curveSets.length == shapes.length);
  for (var i = 0; i < shapes.length; ++i) {
    var curveSet = curveSets[i];
    assert(this.duration % curveSet.duration == 0);
    var shape = shapes[i];
    var record = getInitialCurveInfo(curveSet);
    var curveBaseTime = record.curveBaseTime;
    var curve = record.curve;
    setParamCurveInfo(curve, shape, curveBaseTime);
    do {
      curveBaseTime += curve.duration;
      curve = curveSet.getCurveForTime(curveBaseTime);
      var e = new QueueEvent(curveBaseTime % this.duration, shape, curve);
      this.addEvent(e);
    } while (curveBaseTime + curve.duration <= this.duration);
  }
};

/**
 * Return the time of the next future event.
 * @return {!number} the time.
 */
EventQueue.prototype.getNextTime = function() {
  return this.events[this.nextEvent].time + this.timeCorrection;
};

/**
 * This is the event handler that runs the whole animation.  When triggered by
 * the counter, it updates the curves on all objects whose curves have expired.
 *
 * The current time will be some time around when we wanted to be called.  It
 * might be exact, but it might be a bit off due to floating point error, or a
 * lot off due to the system getting bogged down somewhere for a second or
 * two.  e.g. if we wanted to get a call at time 7, it's likely to be
 * something like 7.04, but might even be 11.  We then use 7, not 7.04, as the
 * start time for each of the curves set, so as to remove clock drift.  Since
 * the time we wanted to be called is stored in the next item in the queue, we
 * can just pull that out and use it.  However, if we then find that we're
 * setting our callback in the past, we repeat the process until our callback
 * is set safely in the future.  We may get some visual artifacts, but at
 * least we won't drop any events [leading to stuff drifting endlessly off
 * into the distance].
 */
function handler() {
  var eventTime = g.eventQueue.getNextTime();
  var trueCurrentTime;
  do {
    g.counter.removeCallback(eventTime);
    eventTime = g.eventQueue.processEvents(eventTime);
    g.counter.addCallback(eventTime, handler);
    trueCurrentTime = g.counter.count;
  } while (eventTime < trueCurrentTime);
}

/**
 * Given a precomputed juggling pattern, this sets up the O3D objects,
 * EventQueue, and callback necessary to start an animation, then calls
 * updateAnimating to kick it off if enabled.
 *
 * @param {!number} numBalls the number of balls in the animation.
 * @param {!number} numHands the number of hands in the animation.
 * @param {!number} duration the length of the full animation cycle in beats.
 * @param {!Array.CurveSet} ballCurveSets one CurveSet per ball.
 * @param {!Array.CurveSet} handCurveSets one CurveSet per hand.
 */
function startAnimation(numBalls, numHands, duration, ballCurveSets,
    handCurveSets) {
  g.counter.running = false;
  g.counter.reset();

  setNumBalls(numBalls);
  setNumHands(numHands);

  g.eventQueue = new EventQueue(duration);
  g.eventQueue.setUp(handCurveSets, g.handShapes);
  g.eventQueue.setUp(ballCurveSets, g.ballShapes);
  g.counter.addCallback(g.eventQueue.getNextTime(), handler);

  updateAnimating();
}

