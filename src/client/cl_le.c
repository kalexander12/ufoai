/**
 * @file cl_le.c
 * @brief Local entity managament.
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "client.h"

/* */
lm_t LMs[MAX_LOCALMODELS];
int numLMs;

/* */
le_t LEs[MAX_EDICTS];
int numLEs;

/* */

vec3_t player_mins = { -PLAYER_WIDTH, -PLAYER_WIDTH, -PLAYER_MIN };
vec3_t player_maxs = { PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND };


/*=========================================================================== */
/* */
/* LM handling */
/* */
/*=========================================================================== */

char *lmList[MAX_LOCALMODELS + 1];


/*
==================
LM_GenerateList
==================
*/
void LM_GenerateList(void)
{
	lm_t *lm;
	int i, l;

	l = 0;
	for (i = 0, lm = LMs; i < numLMs; i++, lm++)
		if (*lm->name == '*')
			lmList[l++] = lm->name;
	lmList[l] = NULL;
}


/*
==============
LM_AddToScene
==============
*/
void LM_AddToScene(void)
{
	lm_t *lm;
	entity_t ent;
	int i;

	for (i = 0, lm = LMs; i < numLMs; i++, lm++) {
		/* check for visibility */
		if (!((1 << (int) cl_worldlevel->value) & lm->levelflags))
			continue;

		/* set entity values */
		memset(&ent, 0, sizeof(entity_t));
		VectorCopy(lm->origin, ent.origin);
		VectorCopy(lm->origin, ent.oldorigin);
		VectorCopy(lm->angles, ent.angles);
		ent.model = lm->model;
		ent.skinnum = lm->skin;

		if (lm->flags & LMF_NOSMOOTH)
			ent.flags |= RF_NOSMOOTH;
		if (lm->flags & LMF_LIGHTFIXED) {
			ent.flags |= RF_LIGHTFIXED;
			ent.lightparam = lm->lightorigin;
			ent.lightcolor = lm->lightcolor;
			ent.lightambient = lm->lightambient;
		} else
			ent.lightparam = &lm->sunfrac;

		/* add it to the scene */
		V_AddEntity(&ent);
	}
}


/*
==============
LM_Find
==============
*/
lm_t *LM_Find(int num)
{
	int i;

	for (i = 0; i < numLMs; i++)
		if (LMs[i].num == num)
			return &LMs[i];

	Com_Printf("LM_Perish: Can't find model %i\n", num);
	return NULL;
}


/*
==============
LM_Delete
==============
*/
void LM_Delete(lm_t * lm)
{
	lm_t backup;

	backup = *lm;
	numLMs--;
	memcpy(lm, lm + 1, (numLMs - (lm - LMs)) * sizeof(lm_t));

	LM_GenerateList();
	Grid_RecalcRouting(&clMap, backup.name, lmList);
	if (selActor)
		Grid_MoveCalc(&clMap, selActor->pos, MAX_ROUTE, fb_list, fb_length);
}


/*
==============
LM_Perish
==============
*/
void LM_Perish(sizebuf_t * sb)
{
	lm_t *lm;

	lm = LM_Find(MSG_ReadShort(sb));
	if (!lm)
		return;

	LM_Delete(lm);
}


/*
==============
LM_Explode
==============
*/
void LM_Explode(sizebuf_t * sb)
{
	lm_t *lm;

	lm = LM_Find(MSG_ReadShort(sb));
	if (!lm)
		return;

	if (lm->particle[0]) {
		cmodel_t *mod;
		vec3_t center;

		/* create particles */
		mod = CM_InlineModel(lm->name);
		VectorAdd(mod->mins, mod->maxs, center);
		VectorScale(center, 0.5, center);
		CL_ParticleSpawn(lm->particle, 0, center, NULL, NULL);
	}

	LM_Delete(lm);
}


/*
==================
CL_RegisterLocalModels
==================
*/
void CL_RegisterLocalModels(void)
{
	lm_t *lm;
	vec3_t sunDir, sunOrigin;
	int i;

	VectorCopy(map_sun.dir, sunDir);

	for (i = 0, lm = LMs; i < numLMs; i++, lm++) {
		/* register the model and recalculate routing info */
		lm->model = re.RegisterModel(lm->name);

		/* calculate sun lighting and register model if not yet done */
		VectorMA(lm->origin, 512, sunDir, sunOrigin);
		if (!CM_TestLine(lm->origin, sunOrigin))
			lm->sunfrac = 1.0f;
		else
			lm->sunfrac = 0.0f;
	}
}


/*
==================
CL_AddLocalModel
==================
*/
lm_t *CL_AddLocalModel(char *model, char *particle, vec3_t origin, vec3_t angles, int num, int levelflags)
{
	lm_t *lm;

	lm = &LMs[numLMs++];

	if (numLMs >= MAX_LOCALMODELS)
		Sys_Error("Too many local models\n");

	memset(lm, 0, sizeof(lm_t));
	Q_strncpyz(lm->name, model, MAX_VAR);
	Q_strncpyz(lm->particle, particle, MAX_VAR);
	VectorCopy(origin, lm->origin);
	VectorCopy(angles, lm->angles);
	lm->num = num;
	lm->levelflags = levelflags;

	LM_GenerateList();
	Grid_RecalcRouting(&clMap, lm->name, lmList);
	/*  Com_Printf( "adding model %s %i\n", lm->name, numLMs ); */

	return lm;
}


/*=========================================================================== */
/* */
/* LE thinking */
/* */
/*=========================================================================== */

/*
==============
LE_Status

Checks whether there are soldiers alive
if not - end round automatically
==============
*/
void LE_Status(void)
{
	le_t *le;
	int i;
	qboolean endRound = qtrue;

	if (!numLEs)
		return;

	/* only multiplayer - but maybe not our round? */
	if ((int) Cvar_VariableValue("maxclients") > 1 && cls.team != cl.actTeam)
		return;

	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && le->team == cls.team && !(le->state & STATE_DEAD))
			/* call think function */
			endRound = qfalse;

	/* ok, no players alive in multiplayer - end this round automatically */
	if (endRound) {
		CL_NextRound();
		Com_Printf("End round automatically - no soldiers left\n");
	}
}

/*
==============
LE_Think
==============
*/
void LE_Think(void)
{
	le_t *le;
	int i;

	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && le->think)
			/* call think function */
			le->think(le);
}


/*=========================================================================== */
/* */
/* LE think functions */
/* */
/*=========================================================================== */

char retAnim[MAX_VAR];

/*
==============
LE_GetAnim
==============
*/
char *LE_GetAnim(char *anim, int right, int left, int state)
{
	char *mod;
	qboolean akimbo;
	char category, *type;

	if (!anim)
		return "";

	mod = retAnim;

	/* add crouched flag */
	if (state & STATE_CROUCHED)
		*mod++ = 'c';

	/* determine relevant data */
	akimbo = qfalse;
	if (right == NONE) {
		category = '0';
		if (left == NONE)
			type = "item";
		else {
			akimbo = qtrue;
			type = csi.ods[left].type;
		}
	} else {
		category = csi.ods[right].category;
		type = csi.ods[right].type;
		if (left != NONE && !Q_strncmp(csi.ods[right].type, "pistol", 6) && !Q_strncmp(csi.ods[left].type, "pistol", 6))
			akimbo = qtrue;
	}

	if (!Q_strncmp(anim, "stand", 5) || !Q_strncmp(anim, "walk", 4)) {
		Q_strncpyz(mod, anim, MAX_VAR);
		mod += strlen(anim);
		*mod++ = category;
		*mod++ = 0;
	} else {
		Q_strncpyz(mod, anim, MAX_VAR);
		Q_strcat(mod, MAX_VAR, "_");
		Q_strcat(mod, MAX_VAR, type);
		if (akimbo)
			Q_strcat(mod, MAX_VAR, "_d");
	}

	return retAnim;
}


/*
==============
LET_StartIdle
==============
*/
void LET_StartIdle(le_t * le)
{
	if (le->state & STATE_DEAD)
		re.AnimChange(&le->as, le->model1, va("dead%i", le->state & STATE_DEAD));
	else if (le->state & STATE_PANIC)
		re.AnimChange(&le->as, le->model1, "panic0");
	else
		re.AnimChange(&le->as, le->model1, LE_GetAnim("stand", le->right, le->left, le->state));

	le->think = NULL;
}


/*
==============
LET_DoAppear
==============
*/
/*#define APPEAR_TIME		1000

void LET_DoAppear( le_t *le )
{
	if ( cl.time > le->startTime + APPEAR_TIME )
	{
		le->alpha = 1.0;
		le->think = NULL;
		return;
	}

	le->alpha = (float)(cl.time - le->startTime) / APPEAR_TIME;
}*/


/*
==============
LET_Appear
==============
*/
/*void LET_Appear( le_t *le )
{
	LET_StartIdle( le );

	le->startTime = cl.time;
	le->think = LET_DoAppear;
	le->think( le );
}*/


/*
==============
LET_Perish
==============
*/
/*#define PERISH_TIME		1000

void LET_Perish( le_t *le )
{
	if ( cl.time > le->startTime + PERISH_TIME )
	{
		le->inuse = qfalse;
		return;
	}

	le->alpha = 1.0 - (float)(cl.time - le->startTime) / PERISH_TIME;
}*/


/*
==============
LET_PathMove
==============
*/
void LET_PathMove(le_t * le)
{
	byte dv;
	float frac;
	vec3_t start, dest, delta;

	/* check for start */
	if (cl.time <= le->startTime)
		return;

	/* move ahead */
	while (cl.time > le->endTime) {
		VectorCopy(le->pos, le->oldPos);

		if (le->pathPos < le->pathLength) {
			/* next part */
			dv = le->path[le->pathPos++];
			PosAddDV(le->pos, dv);
			le->dir = dv & 7;
			le->angles[YAW] = dangle[le->dir];
			le->startTime = le->endTime;
			le->endTime += ((dv & 7) > 3 ? US * 1.41 : US) * 1000 / le->speed;
			if (le->team == cls.team && le == selActor && (int) cl_worldlevel->value == le->oldPos[2] && le->pos[2] != le->oldPos[2]) {
				/*PosToVec( le->pos, dest ); */
				/*VectorCopy( dest, cl.cam.reforg ); */
				Cvar_SetValue("cl_worldlevel", le->pos[2]);
			}
		} else {
			/* end of move */
			le_t *floor;

			Grid_PosToVec(&clMap, le->pos, le->origin);

			/* calculate next possible moves */
			CL_BuildForbiddenList();
			if (selActor == le)
				Grid_MoveCalc(&clMap, le->pos, MAX_ROUTE, fb_list, fb_length);

			floor = LE_Find(ET_ITEM, le->pos);
			if (floor)
				le->i.c[csi.idFloor] = floor->i.c[csi.idFloor];

			blockEvents = qfalse;
			le->think = LET_StartIdle;
			le->think(le);
			if (camera_mode == CAMERA_MODE_FIRSTPERSON) {
				PosToVec(le->pos, dest);
				VectorCopy(dest, cl.cam.camorg);
				VectorCopy(selActor->angles, cl.cam.angles);
			}
			return;
		}
	}

	/* interpolate the position */
	Grid_PosToVec(&clMap, le->oldPos, start);
	Grid_PosToVec(&clMap, le->pos, dest);
	VectorSubtract(dest, start, delta);

	frac = (float) (cl.time - le->startTime) / (float) (le->endTime - le->startTime);

	VectorMA(start, frac, delta, le->origin);
}

/*
==============
LET_StartPathMove
==============
*/
void LET_StartPathMove(le_t * le)
{
	re.AnimChange(&le->as, le->model1, LE_GetAnim("walk", le->right, le->left, le->state));

	le->think = LET_PathMove;
	le->think(le);
}


/*
==============
LET_Projectile
==============
*/
void LET_Projectile(le_t * le)
{
	if (cl.time >= le->endTime) {
		le->ptl->inuse = qfalse;
		le->inuse = qfalse;
		if (le->ref1 && le->ref1[0]) {
			vec3_t impact;

			VectorCopy(le->ptl->s, impact);
			le->ptl = CL_ParticleSpawn(le->ref1, 0, impact, bytedirs[le->state], NULL);
			VecToAngles(bytedirs[le->state], le->ptl->angles);
		}
		if (le->ref2 && le->ref2[0])
			S_StartLocalSound(le->ref2);
	}
}

/*=========================================================================== */
/* */
/* LE Special Effects */
/* */
/*=========================================================================== */

void LE_AddProjectile(fireDef_t * fd, int flags, vec3_t muzzle, vec3_t impact, int normal)
{
	le_t *le;
	vec3_t delta;
	float dist;

	/* add le */
	le = LE_Add(0);
	le->invis = qtrue;

	/* bind particle */
	le->ptl = CL_ParticleSpawn(fd->projectile, 0, muzzle, NULL, NULL);
	if (!le->ptl) {
		le->inuse = qfalse;
		return;
	}

	/* calculate parameters */
	VectorSubtract(impact, muzzle, delta);
	dist = VectorLength(delta);

	VecToAngles(delta, le->ptl->angles);
	le->state = normal;

	if (!fd->speed) {
		/* infinite speed projectile */
		ptl_t *ptl;

		le->inuse = qfalse;
		le->ptl->size[0] = dist;
		VectorMA(muzzle, 0.5, delta, le->ptl->s);
		if (flags & (SF_IMPACT | SF_BODY) || fd->selfDetonate) {
			ptl = NULL;
			if (flags & SF_BODY) {
				if (fd->hitBodySound[0])
					S_StartLocalSound(fd->hitBodySound);
				if (fd->hitBody[0])
					ptl = CL_ParticleSpawn(fd->hitBody, 0, impact, bytedirs[normal], NULL);
			} else {
				if (fd->impactSound[0])
					S_StartLocalSound(fd->impactSound);
				if (fd->impact[0])
					ptl = CL_ParticleSpawn(fd->impact, 0, impact, bytedirs[normal], NULL);
			}
			if (ptl)
				VecToAngles(bytedirs[normal], ptl->angles);
		}
		return;
	}
	/* particle properties */
	VectorScale(delta, fd->speed / dist, le->ptl->v);
	le->endTime = cl.time + 1000 * dist / fd->speed;

	/* think function */
	if (flags & SF_BODY) {
		le->ref1 = fd->hitBody;
		le->ref2 = fd->hitBodySound;
	} else if (flags & SF_IMPACT || fd->selfDetonate) {
		le->ref1 = fd->impact;
		le->ref2 = fd->impactSound;
	} else {
		le->ref1 = NULL;
		if (flags & SF_BOUNCING)
			le->ref2 = fd->bounceSound;
	}

	le->think = LET_Projectile;
	le->think(le);
}


void LE_AddGrenade(fireDef_t * fd, int flags, vec3_t muzzle, vec3_t v0, int dt)
{
	le_t *le;
	vec3_t accel;

	/* add le */
	le = LE_Add(0);
	le->invis = qtrue;

	/* bind particle */
	VectorSet(accel, 0, 0, -GRAVITY);
	le->ptl = CL_ParticleSpawn(fd->projectile, 0, muzzle, v0, accel);
	if (!le->ptl) {
		le->inuse = qfalse;
		return;
	}
	/* particle properties */
	VectorSet(le->ptl->angles, 360 * crand(), 360 * crand(), 360 * crand());
	VectorSet(le->ptl->omega, 500 * crand(), 500 * crand(), 500 * crand());

	/* think function */
	if (flags & SF_BODY) {
		le->ref1 = fd->hitBody;
		le->ref2 = fd->hitBodySound;
	} else if (flags & SF_IMPACT || fd->selfDetonate) {
		le->ref1 = fd->impact;
		le->ref2 = fd->impactSound;
	} else {
		le->ref1 = NULL;
		if (flags & SF_BOUNCING)
			le->ref2 = fd->bounceSound;
	}

	le->endTime = cl.time + dt;
	le->state = 5;				/* direction (0,0,1) */
	le->think = LET_Projectile;
	le->think(le);
}


/*=========================================================================== */
/* */
/* LE Management functions */
/* */
/*=========================================================================== */


/*
==============

LE_Add
==============
*/
le_t *LE_Add(int entnum)
{
	int i;
	le_t *le;

	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (!le->inuse)
			/* found a free LE */
			break;

	/* list full, try to make list longer */
	if (i == numLEs) {
		if (numLEs >= MAX_EDICTS - numLMs) {
			/* no free LEs */
			Com_Error(ERR_DROP, "Too many LEs\n");
			return NULL;
		}

		/* list isn't too long */
		numLEs++;
	}

	/* initialize the new LE */
	memset(le, 0, sizeof(le_t));
	le->inuse = qtrue;
	le->entnum = entnum;
	return le;
}

/*
==============
LE_Get
==============
*/
le_t *LE_Get(int entnum)
{
	int i;
	le_t *le;

	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && le->entnum == entnum)
			/* found the LE */
			return le;

	/* didn't find it */
	return NULL;
}


/*
==============
LE_Find
==============
*/
le_t *LE_Find(int type, pos3_t pos)
{
	int i;
	le_t *le;

	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && le->type == type && VectorCompare(le->pos, pos))
			/* found the LE */
			return le;

	/* didn't find it */
	return NULL;
}


/*
==============
LE_AddToScene
==============
*/
void LE_AddToScene(void)
{
	le_t *le;
	entity_t ent;
	vec3_t sunOrigin;
	int i;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (le->inuse && !le->invis && le->pos[2] <= cl_worldlevel->value) {
			memset(&ent, 0, sizeof(entity_t));

			/* calculate sun lighting */
			if (!VectorCompare(le->origin, le->oldOrigin)) {
				VectorCopy(le->origin, le->oldOrigin);
				VectorMA(le->origin, 512, map_sun.dir, sunOrigin);
				if (!CM_TestLine(le->origin, sunOrigin))
					le->sunfrac = 1.0f;
				else
					le->sunfrac = 0.0f;
			}
			ent.lightparam = &le->sunfrac;
			ent.alpha = le->alpha;

			/* set entity values */
			VectorCopy(le->origin, ent.origin);
			VectorCopy(le->origin, ent.oldorigin);
			VectorCopy(le->angles, ent.angles);
			ent.model = le->model1;
			ent.skinnum = le->skinnum;

			/* do animation */
			re.AnimRun(&le->as, ent.model, cls.frametime * 1000);
			ent.as = le->as;

			/* call add function */
			/* if it returns false, don't draw */
			if (le->addFunc)
				if (!le->addFunc(le, &ent))
					continue;

			/* add it to the scene */
			V_AddEntity(&ent);
		}
	}
}


/*=========================================================================== */
/* */
/* LE Tracing */
/* */
/*=========================================================================== */


typedef struct {
	vec3_t boxmins, boxmaxs;	/* enclose the test object along entire move */
	float *mins, *maxs;			/* size of the moving object */
	vec3_t mins2, maxs2;		/* size when clipping against mosnters */
	float *start, *end;
	trace_t trace;
	le_t *passle;
	int contentmask;
} moveclip_t;


/*
====================
CL_ClipMoveToLEs

====================
*/
void CL_ClipMoveToLEs(moveclip_t * clip)
{
	int i;
	le_t *le;
	trace_t trace;
	int headnode;

	if (clip->trace.allsolid)
		return;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (!le->inuse || !(le->contents & clip->contentmask))
			continue;
		if (le == clip->passle)
			continue;

		/* might intersect, so do an exact clip */
		headnode = CM_HeadnodeForBox(0, le->mins, le->maxs);

		trace = CM_TransformedBoxTrace(clip->start, clip->end, clip->mins, clip->maxs, 0, headnode, clip->contentmask, le->origin, vec3_origin);

		if (trace.allsolid || trace.startsolid || trace.fraction < clip->trace.fraction) {
			trace.le = le;
			clip->trace = trace;
			if (clip->trace.startsolid)
				clip->trace.startsolid = qtrue;
		} else if (trace.startsolid)
			clip->trace.startsolid = qtrue;
	}
}


/*
==================
CL_TraceBounds
==================
*/
void CL_TraceBounds(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (end[i] > start[i]) {
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		} else {
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
}

/*
==================
CL_Trace

Moves the given mins/maxs volume through the world from start to end.

Passedict and edicts owned by passedict are explicitly not checked.

==================
*/
trace_t CL_Trace(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, le_t * passle, int contentmask)
{
	moveclip_t clip;

	/* clip to world */
	clip.trace = CM_CompleteBoxTrace(start, end, mins, maxs, (1 << ((int) cl_worldlevel->value + 1)) - 1, contentmask);
	clip.trace.le = NULL;
	if (clip.trace.fraction == 0)
		return clip.trace;		/* blocked by the world */

	clip.contentmask = contentmask;
	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passle = passle;

	/* create the bounding box of the entire move */
	CL_TraceBounds(start, mins, maxs, end, clip.boxmins, clip.boxmaxs);

	/* clip to other solid entities */
	CL_ClipMoveToLEs(&clip);

	return clip.trace;
}
