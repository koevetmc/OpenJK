#include "g_local.h"
#include "bg_flux.h"

static qboolean G_IsFluxBoard(const gentity_t *board)
{
	return board->inuse && board->s.eType == ET_GENERAL
		&& board->s.generic1 == FLUX_BOARD_MARKER && board->classname
		&& !strcmp(board->classname, "misc_fluxboard");
}

static gentity_t *G_FluxBoard(const gentity_t *rider)
{
	int index = rider->client->ps.stats[STAT_FLUX_BOARD]-1;
	if (index < MAX_CLIENTS || index >= level.num_entities)
		return NULL;
	if (!G_IsFluxBoard(&g_entities[index]) || g_entities[index].s.otherEntityNum != rider->s.number)
		return NULL;
	return &g_entities[index];
}

static void G_FluxTrace(void *context, flux_trace_t *hit, const float *start,
	const float *mins, const float *maxs, const float *end)
{
	gentity_t *board = (gentity_t *)context;
	trace_t trace;
	trap->Trace(&trace, start, mins, maxs, end, board->s.number, MASK_SOLID, qfalse, 0, 0);
	hit->fraction = trace.fraction;
	hit->solid = trace.startsolid || trace.allsolid;
	hit->entity = trace.entityNum;
	VectorCopy(trace.plane.normal, hit->normal);
}

void G_FluxRelease(gentity_t *rider)
{
	gentity_t *board;
	int i, held;
	if (!rider->client)
		return;
	board = G_FluxBoard(rider);
	held = rider->client->ps.stats[STAT_FLUX_FLAGS] & FLUX_USE_HELD;
	for (i = STAT_FLUX_BOARD; i <= STAT_FLUX_PUMP_MS; ++i)
		rider->client->ps.stats[i] = 0;
	rider->client->ps.stats[STAT_FLUX_FLAGS] = held;
	if (!board)
		return;
	board->s.otherEntityNum = ENTITYNUM_NONE;
	board->s.time = 0;
	board->s.time2 = 0;
	if ((board->spawnflags & 2) || rider->health <= 0 || rider->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		G_SetOrigin(board, board->pos1);
		VectorClear(board->s.pos.trDelta);
	}
	else
	{
		VectorCopy(rider->client->ps.velocity, board->s.pos.trDelta);
	}
	board->s.pos.trType = TR_INTERPOLATE;
	trap->LinkEntity((sharedEntity_t *)board);
}

void G_FluxUpdate(gentity_t *rider)
{
	gentity_t *board;
	vec3_t origin;
	if (!rider->client)
		return;
	board = G_FluxBoard(rider);
	if (!board)
	{
		if (rider->client->ps.stats[STAT_FLUX_BOARD])
			G_FluxRelease(rider);
		return;
	}
	if (!BG_FluxRiding(&rider->client->ps) || rider->health <= 0
		|| rider->client->sess.sessionTeam == TEAM_SPECTATOR
		|| rider->client->pers.connected != CON_CONNECTED || rider->waterlevel > 1)
	{
		G_FluxRelease(rider);
		return;
	}
	VectorCopy(rider->client->ps.origin, origin);
	origin[2] += MINS_Z - 6.0f;
	G_SetOrigin(board, origin);
	board->s.pos.trType = TR_INTERPOLATE;
	board->s.apos.trBase[YAW] = (float)rider->client->ps.stats[STAT_FLUX_YAW]/64.0f;
	board->s.time = rider->client->ps.stats[STAT_FLUX_TRICK];
	board->s.time2 = rider->client->ps.stats[STAT_FLUX_TRICK_MS];
	board->s.apos.trTime = level.time;
	trap->LinkEntity((sharedEntity_t *)board);
}

static void G_FluxThink(gentity_t *board)
{
	int owner = board->s.otherEntityNum;
	int msec = level.time-board->genericValue2;
	board->genericValue2 = level.time;
	board->nextthink = level.time + 50;
	if (owner >= 0 && owner < MAX_CLIENTS)
	{
		gentity_t *rider = &g_entities[owner];
		if (rider->inuse && rider->client && G_FluxBoard(rider) == board)
		{
			G_FluxUpdate(rider);
			return;
		}
		board->s.otherEntityNum = ENTITYNUM_NONE;
		G_SetOrigin(board, board->pos1);
	}
	if (!board->genericValue1)
	{
		flux_state_t state;
		flux_input_t input;
		flux_world_t world;
		flux_result_t result;
		memset(&state, 0, sizeof(state));
		memset(&input, 0, sizeof(input));
		VectorCopy(board->r.currentOrigin, state.origin);
		VectorCopy(board->s.pos.trDelta, state.velocity);
		state.yaw = board->s.apos.trBase[YAW];
		VectorCopy(board->r.mins, world.mins);
		VectorCopy(board->r.maxs, world.maxs);
		world.gravity = g_gravity.value;
		world.trace = G_FluxTrace;
		world.context = board;
		Flux_Step(&state, &input, &world, msec, &result);
		G_SetOrigin(board, state.origin);
		VectorCopy(state.velocity, board->s.pos.trDelta);
		board->s.pos.trType = TR_INTERPOLATE;
		if (board->r.currentOrigin[2] < -60000.0f || (trap->PointContents(board->r.currentOrigin, board->s.number) & CONTENTS_NODROP))
			G_SetOrigin(board, board->pos1);
		trap->LinkEntity((sharedEntity_t *)board);
	}
	board->genericValue2 = level.time;
}

static void G_FluxUse(gentity_t *board, gentity_t *other, gentity_t *activator)
{
	(void)other;
	(void)activator;
	if (board->s.otherEntityNum >= 0 && board->s.otherEntityNum < MAX_CLIENTS)
		G_FluxRelease(&g_entities[board->s.otherEntityNum]);
	board->genericValue1 = !board->genericValue1;
	if (board->genericValue1)
		board->s.eFlags |= EF_NODRAW;
	else
		board->s.eFlags &= ~EF_NODRAW;
}

void SP_misc_fluxboard(gentity_t *board)
{
	board->s.eType = ET_GENERAL;
	board->s.generic1 = FLUX_BOARD_MARKER;
	board->s.otherEntityNum = ENTITYNUM_NONE;
	board->r.contents = 0;
	VectorSet(board->r.mins, -18.0f, -18.0f, 0.0f);
	VectorSet(board->r.maxs, 18.0f, 18.0f, 6.0f);
	VectorCopy(board->s.origin, board->pos1);
	G_SetOrigin(board, board->s.origin);
	G_SetAngles(board, board->s.angles);
	board->genericValue1 = (board->spawnflags & 1) != 0;
	board->genericValue2 = level.time;
	if (board->genericValue1)
		board->s.eFlags |= EF_NODRAW;
	board->think = G_FluxThink;
	board->use = G_FluxUse;
	board->nextthink = level.time + 50;
	trap->LinkEntity((sharedEntity_t *)board);
}

void G_FluxInput(gentity_t *rider, const usercmd_t *cmd)
{
	playerState_t *ps;
	gentity_t *nearest = NULL;
	float best = 80.0f*80.0f;
	int held, i;
	if (!rider->client || rider->s.number >= MAX_CLIENTS)
		return;
	ps = &rider->client->ps;
	held = ps->stats[STAT_FLUX_FLAGS] & FLUX_USE_HELD;
	ps->stats[STAT_FLUX_FLAGS] &= ~FLUX_USE_HELD;
	if (!(cmd->buttons & BUTTON_USE) || (cmd->buttons & BUTTON_TALK))
		return;
	ps->stats[STAT_FLUX_FLAGS] |= FLUX_USE_HELD;
	if (ps->stats[STAT_FLUX_BOARD])
	{
		ps->useDelay = level.time + 200;
		if (!held)
			G_FluxRelease(rider);
		return;
	}
	if (held || ps->pm_type != PM_NORMAL || rider->health <= 0 || ps->m_iVehicleNum
		|| ps->emplacedIndex || ps->groundEntityNum == ENTITYNUM_NONE
		|| rider->client->sess.sessionTeam == TEAM_SPECTATOR || rider->waterlevel > 1)
		return;
	for (i = MAX_CLIENTS; i < level.num_entities; ++i)
	{
		gentity_t *board = &g_entities[i];
		vec3_t delta, feet;
		trace_t trace;
		float distance;
		if (!G_IsFluxBoard(board) || board->genericValue1 || board->s.otherEntityNum != ENTITYNUM_NONE)
			continue;
		VectorCopy(ps->origin, feet);
		feet[2] += MINS_Z;
		VectorSubtract(board->r.currentOrigin, feet, delta);
		distance = VectorLengthSquared(delta);
		if (distance >= best)
			continue;
		trap->Trace(&trace, ps->origin, NULL, NULL, board->r.currentOrigin, rider->s.number, MASK_SOLID, qfalse, 0, 0);
		if (trace.startsolid || trace.allsolid || trace.fraction < 1.0f)
			continue;
		nearest = board;
		best = distance;
	}
	if (nearest)
	{
		vec3_t destination, start, mins = { -15.0f, -15.0f, MINS_Z };
		vec3_t maxs = { 15.0f, 15.0f, 32.0f };
		trace_t trace;
		VectorCopy(ps->origin, start);
		start[2] += 6.0f;
		maxs[2] = ps->standheight;
		trap->Trace(&trace, ps->origin, mins, maxs, start, rider->s.number, MASK_PLAYERSOLID, qfalse, 0, 0);
		if (trace.startsolid || trace.allsolid || trace.fraction < 1.0f)
			return;
		mins[2] -= 6.0f;
		VectorCopy(nearest->r.currentOrigin, destination);
		destination[2] -= mins[2];
		trap->Trace(&trace, start, mins, maxs, destination, rider->s.number, MASK_PLAYERSOLID, qfalse, 0, 0);
		if (trace.startsolid || trace.allsolid || trace.fraction < 1.0f)
			return;
		VectorCopy(destination, ps->origin);
		VectorCopy(nearest->s.pos.trDelta, ps->velocity);
		ps->stats[STAT_FLUX_BOARD] = nearest->s.number+1;
		ps->stats[STAT_FLUX_YAW] = (int)(AngleNormalize360(ps->viewangles[YAW])*64.0f);
		ps->stats[STAT_FLUX_FLAGS] = FLUX_USE_HELD;
		ps->useDelay = level.time+200;
		nearest->s.otherEntityNum = rider->s.number;
		G_FluxUpdate(rider);
	}
}
