// This file comes from Box2D-JS, Copyright (c) 2008 ANDO Yasushi.
// The original version is available at http://box2d-js.sourceforge.net/ under the
// zlib/libpng license (see License.txt).
// This version has been modified to make it work with O3D.

demos.pendulum = {};
demos.pendulum.initWorld = function(world) {
	var i;
	var ground = world.GetGroundBody();
	var jointDef = new b2RevoluteJointDef();
	var L = 150;
	for (i = 0; i < 4; i++) {
		jointDef.anchorPoint.Set(250 + 40 * i, 200 - L);
		jointDef.body1 = ground;
		jointDef.body2 = createBall(world, 250 + 40 * i, 200);
		world.CreateJoint(jointDef);
	}
	jointDef.anchorPoint.Set(250 - 40, 200 - L);
	jointDef.body1 = ground;
	jointDef.body2 = createBall(world, 250 - 40 - L, 200 - L);
	world.CreateJoint(jointDef);
}
demos.InitWorlds.push(demos.pendulum.initWorld);


