// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_view.c -- setup all the parameters (position, angle, etc)
// for a 3D rendering
#include "cg_local.h"


/*
=============================================================================

  MODEL TESTING

The viewthing and gun positioning tools from Q2 have been integrated and
enhanced into a single model testing facility.

Model viewing can begin with either "testmodel <modelname>" or "testgun <modelname>".

The names must be the full pathname after the basedir, like
"models/weapons/v_launch/tris.md3" or "players/male/tris.md3"

Testmodel will create a fake entity 100 units in front of the current view
position, directly facing the viewer.  It will remain immobile, so you can
move around it to view it from different angles.

Testgun will cause the model to follow the player around and supress the real
view weapon model.  The default frame 0 of most guns is completely off screen,
so you will probably have to cycle a couple frames to see it.

"nextframe", "prevframe", "nextskin", and "prevskin" commands will change the
frame or skin of the testmodel.  These are bound to F5, F6, F7, and F8 in
q3default.cfg.

If a gun is being tested, the "gun_x", "gun_y", and "gun_z" variables will let
you adjust the positioning.

Note that none of the model testing features update while the game is paused, so
it may be convenient to test with deathmatch set to 1 so that bringing down the
console doesn't pause the game.

=============================================================================
*/

/*
=================
CG_TestModel_f

Creates an entity in front of the current position, which
can then be moved around
=================
*/
void CG_TestModel_f (void)
{
	vec3_t		angles;

	memset(&cg.testModelEntity, 0, sizeof(cg.testModelEntity));
	if (trap_Argc() < 2)
	{
		return;
	}

	Q_strncpyz (cg.testModelName, CG_Argv(1), MAX_QPATH);
	cg.testModelEntity.hModel = trap_R_RegisterModel(cg.testModelName);

	if (trap_Argc() == 3)
	{
		cg.testModelEntity.backlerp = atof(CG_Argv(2));
		cg.testModelEntity.frame = 1;
		cg.testModelEntity.oldframe = 0;
	}
	if (! cg.testModelEntity.hModel)
	{
		CG_Printf("Can't register model\n");
		return;
	}

	VectorMA(cg.refdef.vieworg, 100, cg.refdef.viewaxis[0], cg.testModelEntity.origin);

	angles[PITCH] = 0;
	angles[YAW] = 180 + cg.refdefViewAngles[1];
	angles[ROLL] = 0;

	AnglesToAxis(angles, cg.testModelEntity.axis);
	cg.testGun = qfalse;
}

/*
=================
CG_TestGun_f

Replaces the current view weapon with the given model
=================
*/
void CG_TestGun_f (void)
{
	CG_TestModel_f();
	cg.testGun = qtrue;
	cg.testModelEntity.renderfx = RF_MINLIGHT | RF_DEPTHHACK | RF_FIRST_PERSON;
}


void CG_TestModelNextFrame_f (void)
{
	cg.testModelEntity.frame++;
	CG_Printf("frame %i\n", cg.testModelEntity.frame);
}

void CG_TestModelPrevFrame_f (void)
{
	cg.testModelEntity.frame--;
	if (cg.testModelEntity.frame < 0)
	{
		cg.testModelEntity.frame = 0;
	}
	CG_Printf("frame %i\n", cg.testModelEntity.frame);
}

void CG_TestModelNextSkin_f (void)
{
	cg.testModelEntity.skinNum++;
	CG_Printf("skin %i\n", cg.testModelEntity.skinNum);
}

void CG_TestModelPrevSkin_f (void)
{
	cg.testModelEntity.skinNum--;
	if (cg.testModelEntity.skinNum < 0)
	{
		cg.testModelEntity.skinNum = 0;
	}
	CG_Printf("skin %i\n", cg.testModelEntity.skinNum);
}

static void CG_AddTestModel (void)
{
	int		i;

	// re-register the model, because the level may have changed
	cg.testModelEntity.hModel = trap_R_RegisterModel(cg.testModelName);
	if (! cg.testModelEntity.hModel)
	{
		CG_Printf ("Can't register model\n");
		return;
	}

	// if testing a gun, set the origin reletive to the view origin
	if (cg.testGun)
	{
		VectorCopy(cg.refdef.vieworg, cg.testModelEntity.origin);
		VectorCopy(cg.refdef.viewaxis[0], cg.testModelEntity.axis[0]);
		VectorCopy(cg.refdef.viewaxis[1], cg.testModelEntity.axis[1]);
		VectorCopy(cg.refdef.viewaxis[2], cg.testModelEntity.axis[2]);

		// allow the position to be adjusted
		for (i=0; i<3; i++)
		{
			cg.testModelEntity.origin[i] += cg.refdef.viewaxis[0][i] * cg_gun_x.value;
			cg.testModelEntity.origin[i] += cg.refdef.viewaxis[1][i] * cg_gun_y.value;
			cg.testModelEntity.origin[i] += cg.refdef.viewaxis[2][i] * cg_gun_z.value;
		}
	}

	trap_R_AddRefEntityToScene(&cg.testModelEntity);
}



//============================================================================


/*
=================
CG_CalcVrect

Sets the coordinates of the rendered window
=================
*/
static void CG_CalcVrect (void)
{
	int		size;

	// the intermission should allways be full screen
	if (cg.snap->ps.pm_type == PM_INTERMISSION)
	{
		size = 100;
	}
	else
	{
		// bound normal viewsize
		if (cg_viewsize.integer < 30)
		{
			trap_Cvar_Set ("cg_viewsize","30");
			size = 30;
		}
		else if (cg_viewsize.integer > 100)
		{
			trap_Cvar_Set ("cg_viewsize","100");
			size = 100;
		}
		else
		{
			size = cg_viewsize.integer;
		}

	}
	cg.refdef.width = cgs.glconfig.vidWidth*size/100;
	cg.refdef.width &= ~1;

	cg.refdef.height = cgs.glconfig.vidHeight*size/100;
	cg.refdef.height &= ~1;

	cg.refdef.x = (cgs.glconfig.vidWidth - cg.refdef.width)/2;
	cg.refdef.y = (cgs.glconfig.vidHeight - cg.refdef.height)/2;
}

//==============================================================================


/*
===============
CG_OffsetThirdPersonView

===============
*/
#define	FOCUS_DISTANCE	512
static void CG_OffsetThirdPersonView(void)
{
	vec3_t		forward, right, up;
	vec3_t		view;
	vec3_t		focusAngles;
	trace_t		trace;
	static vec3_t	mins = { -4, -4, -4 };
	static vec3_t	maxs = { 4, 4, 4 };
	vec3_t		focusPoint;
	float		focusDist;
	float		forwardScale, sideScale;
	float Range, Pitch;
	vec3_t Gravity;
	int i;

	Inv_GetVectorFromStat(cg.predictedPlayerState.stats[STAT_GRAVITY], Gravity);

	if (!(cg.snap->ps.pm_flags & PMF_FOLLOW) || cgs.Inv_FollowMode == 1)
	{
		for (i = 0; i < 3; ++i)
			cg.refdef.vieworg[i] -= Gravity[i] * cg.predictedPlayerState.viewheight;

		VectorCopy(cg.refdefViewAngles, focusAngles);

		// if dead, look at killer
		if (cg.predictedPlayerState.stats[STAT_HEALTH] <= 0)
		{
			focusAngles[YAW] = cg.predictedPlayerState.stats[STAT_DEAD_YAW];
			cg.refdefViewAngles[YAW] = cg.predictedPlayerState.stats[STAT_DEAD_YAW];
		}

		if (focusAngles[PITCH] > 45)
		{
			focusAngles[PITCH] = 45;		// don't go too far overhead
		}
		else if (focusAngles[PITCH] < -45)
		{
			focusAngles[PITCH] = -45;
		}

		VectorCopy(cg.refdef.vieworg, view);

		for (i = 0; i < 3; ++i)
			view[i] -= Gravity[i] * 8;

		cg.refdefViewAngles[PITCH] *= 0.5;

		i = (cg.predictedPlayerState.stats[STAT_SPEC1] << 16)
			+ (cg.predictedPlayerState.stats[STAT_SPEC2] & 65535);

		if (!i)
		{
			AngleVectors(focusAngles, forward, NULL, NULL);
			VectorMA(cg.refdef.vieworg, FOCUS_DISTANCE, forward, focusPoint);

			AngleVectors(cg.refdefViewAngles, forward, right, up);
		}
		else
		{
			vec3_t Matrix[3];
			vec4_t Quat;

			Inv_GetQuatFromStat(i, Quat, NULL);

			AngleVectors(focusAngles, Matrix[0], Matrix[1], Matrix[2]);
			Inv_QuatMultiply(Quat, Matrix);

			VectorMA(cg.refdef.vieworg, FOCUS_DISTANCE, Matrix[0], focusPoint);

			AngleVectors(cg.refdefViewAngles, Matrix[0], Matrix[1], Matrix[2]);

			Inv_QuatMultiply(Quat, Matrix);

			VectorCopy(Matrix[0], forward);
			VectorCopy(Matrix[1], right);
			VectorCopy(Matrix[2], up);
		}

		if (!(cg.snap->ps.pm_flags & PMF_FOLLOW))
			Range = cg_thirdPersonRange.value;
		else
			Range = 80.0f;

		forwardScale = cos(cg_thirdPersonAngle.value / 180 * M_PI);
		sideScale = sin(cg_thirdPersonAngle.value / 180 * M_PI);
		VectorMA(view, -Range * forwardScale, forward, view);
		VectorMA(view, -Range * sideScale, right, view);

		// trace a ray from the origin to the viewpoint to make sure the view isn't
		// in a solid block.  Use an 8 by 8 block to prevent the view from near clipping anything

		if (!cg_cameraMode.integer)
		{
			CG_Trace(&trace, cg.refdef.vieworg, mins, maxs, view, cg.predictedPlayerState.clientNum, MASK_SOLID);

			if (trace.fraction != 1.0)
			{
				VectorCopy(trace.endpos, view);

				for (i = 0; i < 3; ++i)
					view[i] -= (1.0 - trace.fraction) * 32 * Gravity[i];
				//view[2] += (1.0 - trace.fraction) * 32;

				// try another trace to this position, because a tunnel may have the ceiling
				// close enogh that this is poking out

				CG_Trace(&trace, cg.refdef.vieworg, mins, maxs, view, cg.predictedPlayerState.clientNum, MASK_SOLID);
				VectorCopy(trace.endpos, view);
			}
		}

		VectorCopy(view, cg.refdef.vieworg);

		// select pitch to look at focus point from vieword
		VectorSubtract(focusPoint, cg.refdef.vieworg, focusPoint);

		Range = DotProduct(Gravity, focusPoint);

		for (i = 0; i < 3; ++i)
			forward[i] = focusPoint[i] - Gravity[i] * Range;

		for (i = 0; i < 3; ++i)
			up[i] = Gravity[i] * Range;
		if (Range > 0)
			Range = -VectorLength(up);
		else
			Range = VectorLength(up);

		focusDist = VectorLength(forward);
		if (focusDist < 1)
			focusDist = 1;	// should never happen

		cg.refdefViewAngles[PITCH] = -180 / M_PI * atan2(Range, focusDist);
		cg.refdefViewAngles[YAW] -= cg_thirdPersonAngle.value;
	}
	else
	{
		usercmd_t Cmd;

		Range = 80.0f;
		trap_GetUserCmd(trap_GetCurrentCmdNumber(), &Cmd);
		cg.refdefViewAngles[YAW] = SHORT2ANGLE(Cmd.angles[YAW]);// + cg.snap->ps.viewangles[YAW];
		Pitch = SHORT2ANGLE(Cmd.angles[PITCH]);//cg.snap->ps.viewangles[PITCH];
		if (Pitch > 180)
			Pitch -= 360;
		Pitch *= 2;
		if (Pitch < -90)
			Pitch = -90;
		else if (Pitch > 90)
			Pitch = 90;

		cg.refdefViewAngles[PITCH] = Pitch;

		for (i = 0; i < 3; ++i)
			cg.refdef.vieworg[i] -= cg.predictedPlayerState.viewheight * Gravity[i];

		VectorCopy(cg.refdef.vieworg, view);

		for (i = 0; i < 3; ++i)
			view[i] -= Gravity[i] * 8;

		AngleVectors(cg.refdefViewAngles, forward, NULL, NULL);

		VectorMA(view, -Range, forward, view);

		// trace a ray from the origin to the viewpoint to make sure the view isn't
		// in a solid block.  Use an 8 by 8 block to prevent the view from near clipping anything
		if (!cg_cameraMode.integer)
		{
			CG_Trace(&trace, cg.refdef.vieworg, mins, maxs, view, cg.predictedPlayerState.clientNum, MASK_SOLID);

			if (trace.fraction != 1.0)
				VectorCopy(trace.endpos, view);
		}

		VectorCopy(view, cg.refdef.vieworg);
	}
}


// this causes a compiler bug on mac MrC compiler
static void CG_StepOffset(void)
{
	int timeDelta, i;

	// smooth out stair climbing
	timeDelta = cg.time - cg.stepTime;
	if (timeDelta < STEP_TIME)
	{
		for (i = 0; i < 3; ++i)
			cg.refdef.vieworg[i] -= cg.stepChange[i] * (STEP_TIME - timeDelta) / STEP_TIME;
	}
}

/*
===============
CG_OffsetFirstPersonView

===============
*/
static void CG_OffsetFirstPersonView(void)
{
	float			*origin;
	float			*angles;
	float			bob;
	float			ratio;
	float			delta;
	float			speed;
	float			f;
	vec3_t			predictedVelocity;
	int			timeDelta, i;
	vec3_t Gravity;

	if (cg.snap->ps.pm_type == PM_INTERMISSION)
	{
		return;
	}

	origin = cg.refdef.vieworg;
	angles = cg.refdefViewAngles;

	// if dead, fix the angle and don't add any kick
	if (cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		angles[ROLL] = 40;
		angles[PITCH] = -15;
		angles[YAW] = cg.snap->ps.stats[STAT_DEAD_YAW];
		origin[2] += cg.predictedPlayerState.viewheight;
		return;
	}

	// add angles based on weapon kick
	VectorAdd (angles, cg.kick_angles, angles);

	// add angles based on damage kick
	if (cg.damageTime)
	{
		ratio = cg.time - cg.damageTime;
		if (ratio < DAMAGE_DEFLECT_TIME)
		{
			ratio /= DAMAGE_DEFLECT_TIME;
			angles[PITCH] += ratio * cg.v_dmg_pitch;
			angles[ROLL] += ratio * cg.v_dmg_roll;
		}
		else
		{
			ratio = 1.0 - (ratio - DAMAGE_DEFLECT_TIME) / DAMAGE_RETURN_TIME;
			if (ratio > 0)
			{
				angles[PITCH] += ratio * cg.v_dmg_pitch;
				angles[ROLL] += ratio * cg.v_dmg_roll;
			}
		}
	}

	// add pitch based on fall kick
#if 0
	ratio = (cg.time - cg.landTime) / FALL_TIME;
	if (ratio < 0)
		ratio = 0;
	angles[PITCH] += ratio * cg.fall_value;
#endif

	// add angles based on velocity
	VectorCopy(cg.predictedPlayerState.velocity, predictedVelocity);

	delta = DotProduct (predictedVelocity, cg.refdef.viewaxis[0]);
	angles[PITCH] += delta * cg_runpitch.value;

	delta = DotProduct (predictedVelocity, cg.refdef.viewaxis[1]);
	angles[ROLL] -= delta * cg_runroll.value;

	// add angles based on bob

	// make sure the bob is visible even at low speeds
	speed = cg.xyspeed > 200 ? cg.xyspeed : 200;

	delta = cg.bobfracsin * cg_bobpitch.value * speed;
	if (cg.predictedPlayerState.pm_flags & PMF_DUCKED)
		delta *= 3;		// crouching
	angles[PITCH] += delta;
	delta = cg.bobfracsin * cg_bobroll.value * speed;
	if (cg.predictedPlayerState.pm_flags & PMF_DUCKED)
		delta *= 3;		// crouching accentuates roll
	if (cg.bobcycle & 1)
		delta = -delta;
	angles[ROLL] += delta;

//===================================

	// add view height
	Inv_GetVectorFromStat(cg.predictedPlayerState.stats[STAT_GRAVITY], Gravity);

	for (i = 0; i < 3; ++i)
		cg.refdef.vieworg[i] -= cg.predictedPlayerState.viewheight * Gravity[i];

	// smooth out duck height changes
	timeDelta = cg.time - cg.duckTime;
	if (timeDelta < DUCK_TIME)
	{
		for (i = 0; i < 3; ++i)
			cg.refdef.vieworg[i] += Gravity[i] * cg.duckChange
									* (DUCK_TIME - timeDelta) / DUCK_TIME;
	}

	// add bob height
	bob = cg.bobfracsin * cg.xyspeed * cg_bobup.value;
	if (bob > 6)
	{
		bob = 6;
	}

	for (i = 0; i < 3; ++i)
		origin[i] += bob * Gravity[i];


	// add fall height
	delta = cg.time - cg.landTime;
	if (delta < LAND_DEFLECT_TIME)
	{
		f = delta / LAND_DEFLECT_TIME;
		cg.refdef.vieworg[2] += cg.landChange * f;
	}
	else if (delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME)
	{
		delta -= LAND_DEFLECT_TIME;
		f = 1.0 - (delta / LAND_RETURN_TIME);
		cg.refdef.vieworg[2] += cg.landChange * f;
	}

	// add step offset
	CG_StepOffset();

	// add kick offset

	VectorAdd (origin, cg.kick_origin, origin);

	// pivot the eye based on a neck length
#if 0
	{
#define	NECK_LENGTH		8
	vec3_t			forward, up;

	cg.refdef.vieworg[2] -= NECK_LENGTH;
	AngleVectors(cg.refdefViewAngles, forward, NULL, up);
	VectorMA(cg.refdef.vieworg, 3, forward, cg.refdef.vieworg);
	VectorMA(cg.refdef.vieworg, NECK_LENGTH, up, cg.refdef.vieworg);
	}
#endif
}

//======================================================================

void CG_ZoomDown_f(void)
{
	if (cg.zoomed)
	{
		return;
	}
	cg.zoomed = qtrue;
	cg.zoomTime = cg.time;
}

void CG_ZoomUp_f(void)
{
	if (!cg.zoomed)
	{
		return;
	}
	cg.zoomed = qfalse;
	cg.zoomTime = cg.time;
}


/*
====================
CG_CalcFov

Fixed fov at intermissions, otherwise account for fov variable and zooms.
====================
*/
#define	WAVE_AMPLITUDE	1
#define	WAVE_FREQUENCY	0.4

int InvasionAlienFov[e_Selection_MaxRace] = { 100, 110, 90, 120 };

static int CG_CalcFov(void)
{
	float	x;
	float	phase;
	float	v;
	int		contents;
	float	fov_x = 90, fov_y;
	float	zoomFov;
	float	f;
	int		inwater;
	qboolean NormalFov = qtrue;
	float Sensitivity = 1.0f;

	if (cg.predictedPlayerState.pm_type == PM_INTERMISSION || cg.renderingThirdPerson)
	{
		// if in intermission, use a fixed value
		fov_x = 90;
		NormalFov = qfalse;
	}
	else
	{
		trap_Cvar_Update(&cg_fov);

		if (cg.snap->ps.weapon == WP_RAILGUN)
		{
			usercmd_t cmd;
			int cmdNum;

			cmdNum = trap_GetCurrentCmdNumber();// - CMD_BACKUP + 1;
			trap_GetUserCmd(cmdNum, &cmd);

			if ((cmd.buttons & BUTTON_ATTACK2) && !(cg.LastButtons & BUTTON_ATTACK2))
				cg.Zoom = !cg.Zoom;

			cg.LastButtons = cmd.buttons;
		}
		else
			cg.Zoom = 0;

		if (cg.snap->ps.persistant[PERS_TEAM] == cgs.InvasionInfo.MarineTeam)
		{
			if (cg.Zoom)
			{
				fov_x = 30;
				Sensitivity = 0.4f;
			}
			else
				fov_x = 90;

			if (cg_fov.integer != fov_x)
			{
				cg_fov.integer = fov_x;
				//trap_Cvar_SetValue("cg_fov", fov_x);
			}
			NormalFov = qfalse;
		}
		else if (cg.snap->ps.persistant[PERS_TEAM] == cgs.InvasionInfo.AlienTeam)
		{
			int Race = cg.snap->ps.persistant[PERS_CLASS] & e_Class_AlienRaceMask;

			if (cg.Zoom)
			{
				fov_x = 30;
				Sensitivity = 0.4f;
			}
			else
				fov_x = InvasionAlienFov[Race];

			if (cg_fov.integer != fov_x)
			{
				cg_fov.integer = fov_x;
				//trap_Cvar_SetValue("cg_fov", fov_x);
			}
			NormalFov = qfalse;
		}
		/*else if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
		{
			fov_x = 100;

			if (cg_fov.integer != fov_x)
			{
				cg_fov.integer = fov_x;
				//trap_Cvar_SetValue("cg_fov", fov_x);
			}
			NormalFov = qfalse;
		}*/
	}

	if (NormalFov)
	{
		// user selectable
		if (cgs.dmflags & DF_FIXED_FOV)
		{
			// dmflag to prevent wide fov for all clients
			fov_x = 90;
		}
		else
		{
			fov_x = cg_fov.value;
			if (fov_x < 1)
			{
				fov_x = 1;
			}
			else if (fov_x > 160)
			{
				fov_x = 160;
			}
		}

		// account for zooms
		zoomFov = cg_zoomFov.value;
		if (zoomFov < 1)
		{
			zoomFov = 1;
		}
		else if (zoomFov > 160)
		{
			zoomFov = 160;
		}

		if (cg.zoomed)
		{
			f = (cg.time - cg.zoomTime) / (float)ZOOM_TIME;
			if (f > 1.0)
			{
				fov_x = zoomFov;
			}
			else
			{
				fov_x = fov_x + f * (zoomFov - fov_x);
			}
		}
		else
		{
			f = (cg.time - cg.zoomTime) / (float)ZOOM_TIME;
			if (f > 1.0)
			{
				fov_x = fov_x;
			}
			else
			{
				fov_x = zoomFov + f * (fov_x - zoomFov);
			}
		}
	}

	x = cg.refdef.width / tan(fov_x / 360 * M_PI);
	fov_y = atan2(cg.refdef.height, x);
	fov_y = fov_y * 360 / M_PI;

	// warp if underwater
	contents = CG_PointContents(cg.refdef.vieworg, -1);
	if (contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)){
		phase = cg.time / 1000.0 * WAVE_FREQUENCY * M_PI * 2;
		v = WAVE_AMPLITUDE * sin(phase);
		fov_x += v;
		fov_y -= v;
		inwater = qtrue;
	}
	else
	{
		inwater = qfalse;
	}

	// set it
	cg.refdef.fov_x = fov_x;
	cg.refdef.fov_y = fov_y;

	if (!cg.zoomed)
	{
		cg.zoomSensitivity = Sensitivity;
	}
	else
	{
		cg.zoomSensitivity = cg.refdef.fov_y / 75.0;
	}

	return inwater;
}



/*
===============
CG_DamageBlendBlob

===============
*/
static void CG_DamageBlendBlob(void)
{
	int			t;
	int			maxTime;
	refEntity_t		ent;

	if (!cg.damageValue)
	{
		return;
	}

	//if (cg.cameraMode)
	//{
			//return;
	//}

	// ragePro systems can't fade blends, so don't obscure the screen
	if (cgs.glconfig.hardwareType == GLHW_RAGEPRO)
	{
		return;
	}

	maxTime = DAMAGE_TIME;
	t = cg.time - cg.damageTime;
	if (t <= 0 || t >= maxTime)
	{
		return;
	}


	memset(&ent, 0, sizeof(ent));
	ent.reType = RT_SPRITE;
	ent.renderfx = RF_FIRST_PERSON;

	VectorMA(cg.refdef.vieworg, 8, cg.refdef.viewaxis[0], ent.origin);
	VectorMA(ent.origin, cg.damageX * -8, cg.refdef.viewaxis[1], ent.origin);
	VectorMA(ent.origin, cg.damageY * 8, cg.refdef.viewaxis[2], ent.origin);

	ent.radius = cg.damageValue * 3;
	ent.customShader = cgs.media.viewBloodShader;
	ent.shaderRGBA[0] = 255;
	ent.shaderRGBA[1] = 255;
	ent.shaderRGBA[2] = 255;
	ent.shaderRGBA[3] = 200 * (1.0 - ((float)t / maxTime));
	trap_R_AddRefEntityToScene(&ent);
}


/*
===============
CG_CalcViewValues

Sets cg.refdef view values
===============
*/
static int CG_CalcViewValues(void)
{
	playerState_t	*ps;

	memset(&cg.refdef, 0, sizeof(cg.refdef));

	// strings for in game rendering
	// Q_strncpyz(cg.refdef.text[0], "Park Ranger", sizeof(cg.refdef.text[0]));
	// Q_strncpyz(cg.refdef.text[1], "19", sizeof(cg.refdef.text[1]));

	// calculate size of 3D view
	CG_CalcVrect();

	ps = &cg.predictedPlayerState;
/*
	if (cg.cameraMode) {
		vec3_t origin, angles;
		if (trap_getCameraInfo(cg.time, &origin, &angles)) {
			VectorCopy(origin, cg.refdef.vieworg);
			angles[ROLL] = 0;
			VectorCopy(angles, cg.refdefViewAngles);
			AnglesToAxis( cg.refdefViewAngles, cg.refdef.viewaxis );
			return CG_CalcFov();
		} else {
			cg.cameraMode = qfalse;
		}
	}
*/

	// intermission view
	if (ps->pm_type == PM_INTERMISSION)
	{
		VectorCopy(ps->origin, cg.refdef.vieworg);
		VectorCopy(ps->viewangles, cg.refdefViewAngles);
		AnglesToAxis(cg.refdefViewAngles, cg.refdef.viewaxis);
		return CG_CalcFov();
	}

	cg.bobcycle = (ps->bobCycle & 128) >> 7;
	cg.bobfracsin = fabs(sin((ps->bobCycle & 127) / 127.0 * M_PI));
	cg.xyspeed = sqrt(ps->velocity[0] * ps->velocity[0] +
		ps->velocity[1] * ps->velocity[1]);


	VectorCopy(ps->origin, cg.refdef.vieworg);
	VectorCopy(ps->viewangles, cg.refdefViewAngles);

	if (cg_cameraOrbit.integer)
	{
		if (cg.time > cg.nextOrbitTime)
		{
			cg.nextOrbitTime = cg.time + cg_cameraOrbitDelay.integer;
			cg_thirdPersonAngle.value += cg_cameraOrbit.value;
		}
	}
	// add error decay
	if (cg_errorDecay.value > 0)
	{
		int		t;
		float	f;

		t = cg.time - cg.predictedErrorTime;
		f = (cg_errorDecay.value - t) / cg_errorDecay.value;
		if (f > 0 && f < 1)
		{
			VectorMA(cg.refdef.vieworg, f, cg.predictedError, cg.refdef.vieworg);
		}
		else
		{
			cg.predictedErrorTime = 0;
		}
	}

	if (cg.renderingThirdPerson)
	{
		// back away from character
		CG_OffsetThirdPersonView();
	}
	else
	{
		// offset for local bobbing and kicks
		CG_OffsetFirstPersonView();
	}

	// position eye reletive to origin
	AnglesToAxis(cg.refdefViewAngles, cg.refdef.viewaxis);

	if (!cg.renderingThirdPerson)
	{
		vec4_t Quat;
		int Current = (cg.predictedPlayerState.stats[STAT_SPEC1] << 16)
						+ (cg.predictedPlayerState.stats[STAT_SPEC2] & 65535);

		if (Current)
		{
			Inv_GetQuatFromStat(Current, Quat, NULL);
			Inv_QuatMultiply(Quat, cg.refdef.viewaxis);
		}
	}
	else if (!(cg.snap->ps.pm_flags & PMF_FOLLOW) || cgs.Inv_FollowMode == 1)
	{
		vec4_t Quat;

		Inv_GetVectorFromStat(cg.predictedPlayerState.stats[STAT_GRAVITY], Gravity);
		Inv_ApplyGravityRotation(-3, Quat);
		Inv_QuatMultiply(Quat, cg.refdef.viewaxis);
	}

	if (cg.hyperspace)
	{
		cg.refdef.rdflags |= RDF_NOWORLDMODEL | RDF_HYPERSPACE;
	}

	// field of view
	return CG_CalcFov();
}


/*
=====================
CG_PowerupTimerSounds
=====================
*/
static void CG_PowerupTimerSounds(void)
{
	int		i;
	int		t;

	// powerup timers going away
	for (i = 1; i < MAX_POWERUPS; i++)
	{
		t = cg.snap->ps.powerups[i];
		if (t <= cg.time)
		{
			continue;
		}
		if (t - cg.time >= POWERUP_BLINKS * POWERUP_BLINK_TIME)
		{
			continue;
		}
		if ((t - cg.time) / POWERUP_BLINK_TIME != (t - cg.oldTime) / POWERUP_BLINK_TIME)
		{
			trap_S_StartSound(NULL, cg.snap->ps.clientNum, CHAN_ITEM, cgs.media.wearOffSound);
		}
	}
}

/*
=====================
CG_AddBufferedSound
=====================
*/
void CG_AddBufferedSound(sfxHandle_t sfx)
{
	if (!sfx)
		return;
	cg.soundBuffer[cg.soundBufferIn] = sfx;
	cg.soundBufferIn = (cg.soundBufferIn + 1) % MAX_SOUNDBUFFER;
	if (cg.soundBufferIn == cg.soundBufferOut)
	{
		cg.soundBufferOut++;
	}
}

/*
=====================
CG_PlayBufferedSounds
=====================
*/
static void CG_PlayBufferedSounds(void)
{
	if (cg.soundTime < cg.time)
	{
		if (cg.soundBufferOut != cg.soundBufferIn && cg.soundBuffer[cg.soundBufferOut])
		{
			trap_S_StartLocalSound(cg.soundBuffer[cg.soundBufferOut], CHAN_ANNOUNCER);
			cg.soundBuffer[cg.soundBufferOut] = 0;
			cg.soundBufferOut = (cg.soundBufferOut + 1) % MAX_SOUNDBUFFER;
			cg.soundTime = cg.time + 750;
		}
	}
}

//=========================================================================

/*
=================
CG_DrawActiveFrame

Generates and draws a game scene and status information at the given time.
=================
*/
void CG_DrawActiveFrame(int serverTime, stereoFrame_t stereoView, qboolean demoPlayback)
{
	int		inwater;
	int Back3dPerson;

	cg.time = serverTime;
	cg.demoPlayback = demoPlayback;

	// update cvars
	CG_UpdateCvars();

	// if we are only updating the screen as a loading
	// pacifier, don't even try to read snapshots
	if (cg.infoScreenText[0] != 0)
	{
		CG_DrawInformation();
		return;
	}

	// any looped sounds will be respecified as entities
	// are added to the render list
	trap_S_ClearLoopingSounds(qfalse);

	// clear all the render lists
	trap_R_ClearScene();

	// set up cg.snap and possibly cg.nextSnap
	CG_ProcessSnapshots();

	// if we haven't received any snapshots yet, all
	// we can draw is the information screen
	if (!cg.snap || (cg.snap->snapFlags & SNAPFLAG_NOT_ACTIVE))
	{
		CG_DrawInformation();
		return;
	}

	// let the client system know what our weapon and zoom settings are
	trap_SetUserCmdValue(cg.weaponSelect, cg.zoomSensitivity);

	// this counter will be bumped for every valid scene we generate
	cg.clientFrame++;

	// update cg.predictedPlayerState
	CG_PredictPlayerState();

	Back3dPerson = cg_thirdPerson.integer;

	if ((cg.snap->ps.pm_flags & PMF_FOLLOW))
	{
		if (cgs.Inv_FollowMode != 2)
			cg_thirdPerson.integer = 1;
		else
			cg_thirdPerson.integer = 0;
	}
	else if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
		cg_thirdPerson.integer = 0;

	// decide on third person view
	cg.renderingThirdPerson = cg_thirdPerson.integer || (cg.snap->ps.stats[STAT_HEALTH] <= 0);

	cg_thirdPerson.integer = Back3dPerson;

	// build cg.refdef
	inwater = CG_CalcViewValues();

	if (cg.snap->ps.pm_type == PM_INTERMISSION
		|| (cg.snap->ps.pm_flags & PMF_FOLLOW)
		|| cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		cg.Zoom = cg.NightVision = qfalse;
	}

	if (cg.renderingThirdPerson)
		cg.Zoom = qfalse;

	// first person blend blobs, done after AnglesToAxis
	if (!cg.renderingThirdPerson)
	{
		CG_DamageBlendBlob();
	}

	//trap_Cvar_Set("r_drawworld", "1");
	/*if (cg.NightVision)
	{
		trap_R_RenderScene(&cg.refdef);

		if (0)//cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR)
		{
			int n;
			vec4_t hcolor[3] = { { 0.2f, 1.0f, 0.2f, 1.0f },
										{ 1.0f, 0.1f, 0.1f, 1.0f },
										{ 0.2f, 1.0f, 0.2f, 1.0f } };
										//{ 0.33f, 0.33f, 0.33f, 1.0f } };

			if (cg.NightVision == 1)
				n = 2;
			else if (cg.snap->ps.persistant[PERS_TEAM] == cgs.InvasionInfo.MarineTeam)
				n = 0;
			else
				n = 1;

			trap_R_SetColor(hcolor[n]);

			CG_DrawPic(0, 0, 640, 480, cgs.media.NightVision[cg.NightVision-1]);
			trap_R_SetColor(NULL);

			//CG_DrawUpperRight();
			//CG_DrawCrosshair();
			//return;
		}
	}*/

	// build the render lists
	if (!cg.hyperspace)
	{
		CG_AddPacketEntities();			// adter calcViewValues, so predicted player state is correct
		CG_AddMarks();
		CG_AddParticles ();
		CG_AddLocalEntities();
	}
	CG_AddViewWeapon(&cg.predictedPlayerState);

	// add buffered sounds
	CG_PlayBufferedSounds();

	// play buffered voice chats
	CG_PlayBufferedVoiceChats();

	// finish up the rest of the refdef
	if (cg.testModelEntity.hModel)
	{
		CG_AddTestModel();
	}
	cg.refdef.time = cg.time;
	memcpy(cg.refdef.areamask, cg.snap->areamask, sizeof(cg.refdef.areamask));

	// warning sounds when powerup is wearing off
	CG_PowerupTimerSounds();

	// update audio positions
	trap_S_Respatialize(cg.snap->ps.clientNum, cg.refdef.vieworg, cg.refdef.viewaxis, inwater);

	// make sure the lagometerSample and frame timing isn't done twice when in stereo
	if (stereoView != STEREO_RIGHT)
	{
		cg.frametime = cg.time - cg.oldTime;
		if (cg.frametime < 0)
		{
			cg.frametime = 0;
		}
		cg.oldTime = cg.time;
		CG_AddLagometerFrameInfo();
	}
	if (cg_timescale.value != cg_timescaleFadeEnd.value)
	{
		if (cg_timescale.value < cg_timescaleFadeEnd.value)
		{
			cg_timescale.value += cg_timescaleFadeSpeed.value * ((float)cg.frametime) / 1000;
			if (cg_timescale.value > cg_timescaleFadeEnd.value)
				cg_timescale.value = cg_timescaleFadeEnd.value;
		}
		else
		{
			cg_timescale.value -= cg_timescaleFadeSpeed.value * ((float)cg.frametime) / 1000;
			if (cg_timescale.value < cg_timescaleFadeEnd.value)
				cg_timescale.value = cg_timescaleFadeEnd.value;
		}
		if (cg_timescaleFadeSpeed.value)
		{
			trap_Cvar_Set("timescale", va("%f", cg_timescale.value));
		}
	}

	// actually issue the rendering calls
	CG_DrawActive(stereoView);

	if (cg_stats.integer)
	{
		CG_Printf("cg.clientFrame:%i\n", cg.clientFrame);
	}


}

/*==================== EOF because of buggy VSS ===========*/