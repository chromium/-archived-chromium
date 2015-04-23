// This file comes from Box2D-JS, Copyright (c) 2008 ANDO Yasushi.
// The original version is available at http://box2d-js.sourceforge.net/ under the
// zlib/libpng license (see License.txt).
// This version has been modified to make it work with O3D.

// NOTE: Changed this file pretty significantly. The original uses a
// canvas element and drew lines on it. This one just updates the positions and
// orientations of o3d objects to match the physics as well as using a
// scaled box to stand in for joints.

/**
 * Draws all the object in the world.
 * @param {Object} world A B2World object managing the physics world.
 */
function drawWorld(world) {
  for (var j = world.m_jointList; j; j = j.m_next) {
    drawJoint(j);
  }
  for (var b = world.m_bodyList; b; b = b.m_next) {
    var o3dShape = b.GetUserData();
    if (o3dShape) {
      o3dShape.updateTransform(b);
    }
  }
}
/**
 * Scales a joint transform to represnet a line.
 * @param {Object} transformInfo An object with o3d information for the
 *     joint.
 * @param {Object} p1 An object with x and y fields that specify the origin of
 *     the joint in 2d.
 * @param {Object} p2 An object with x an dy fields that specify the far end of
 *     the joint in 2d.
 */
function scaleJointTransform(transformInfo, p1, p2) {
  var dx = p2.x - p1.x;
  var dy = p2.y - p1.y;
  var length = Math.sqrt(dx * dx + dy * dy);
  var transform = transformInfo.transform;
  transform.identity();
  transform.translate(p1.x, p1.y, 0);
  transform.rotateZ(Math.atan2(-dx, dy));
  transform.scale(2, length, 2);
}

/**
 * Draws a joint
 * @param {Object} joint A b2Joint object representing a joint.
 */
function drawJoint(joint) {
  var transformInfo = joint.m_o3dTransformInfo;
  if (!transformInfo) {
    // This joint did not already have something in o3d to represent it
    // so we create one here.
    var transform = g.pack.createObject('Transform');
    transform.parent = g.root;
    transform.addShape(g.lineShape);
    transformInfo = {
      transform: transform
    };
    joint.m_o3dTransformInfo = transformInfo;
  }
  var b1 = joint.m_body1;
  var b2 = joint.m_body2;
  var x1 = b1.m_position;
  var x2 = b2.m_position;
  var p1 = joint.GetAnchor1();
  var p2 = joint.GetAnchor2();
  switch (joint.m_type) {
  case b2Joint.e_distanceJoint:
    scaleJointTransform(transformInfo, p1, p2);
  break;

  case b2Joint.e_pulleyJoint:
    break;

  default:
    if (b1 == world.m_groundBody) {
      scaleJointTransform(transformInfo, p1, x2);
    }
    else if (b2 == world.m_groundBody) {
      scaleJointTransform(transformInfo, p1, x1);
    }
    else {
      scaleJointTransform(transformInfo, x1, x2);
    }
    break;
  }
}

