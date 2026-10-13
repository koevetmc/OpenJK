#include "qcommon/q_shared.h"
#include "bg_public.h"
#include "bg_flux.h"

qboolean BG_FluxRiding(const playerState_t *ps)
{
	return ps->stats[STAT_FLUX_BOARD] > MAX_CLIENTS
		&& ps->stats[STAT_FLUX_BOARD] <= ENTITYNUM_WORLD
		&& ps->stats[STAT_HEALTH] > 0 && ps->pm_type == PM_NORMAL
		&& !ps->m_iVehicleNum && !ps->emplacedIndex;
}

static void BG_FluxTrace(void *context, flux_trace_t *hit, const float *start,
	const float *mins, const float *maxs, const float *end)
{
	pmove_t *move = (pmove_t *)context;
	trace_t trace;
	int i;
	move->trace(&trace, start, mins, maxs, end, move->ps->clientNum, move->tracemask);
	hit->fraction = trace.fraction;
	hit->solid = trace.allsolid || trace.startsolid;
	hit->entity = trace.entityNum;
	VectorCopy(trace.plane.normal, hit->normal);
	if (trace.fraction < 1.0f && trace.entityNum != ENTITYNUM_WORLD)
	{
		for (i = 0; i < move->numtouch; ++i)
			if (move->touchents[i] == trace.entityNum)
				return;
		if (move->numtouch < MAXTOUCH)
			move->touchents[move->numtouch++] = trace.entityNum;
	}
}

void BG_FluxMove(pmove_t *move, int msec)
{
	playerState_t *ps = move->ps;
	flux_state_t state;
	flux_input_t input;
	flux_world_t world;
	flux_result_t result;
	VectorCopy(ps->origin, state.origin);
	VectorCopy(ps->velocity, state.velocity);
	state.yaw = (float)ps->stats[STAT_FLUX_YAW] / 64.0f;
	state.flags = ps->stats[STAT_FLUX_FLAGS];
	state.trick = ps->stats[STAT_FLUX_TRICK];
	state.air_ms = ps->stats[STAT_FLUX_AIR_MS];
	state.trick_ms = ps->stats[STAT_FLUX_TRICK_MS];
	state.pump_ms = ps->stats[STAT_FLUX_PUMP_MS];
	memset(&input, 0, sizeof(input));
	input.forward = (float)move->cmd.forwardmove / 127.0f;
	input.steer = (float)move->cmd.rightmove / 127.0f;
	input.jump = move->cmd.upmove > 10;
	input.crouch = move->cmd.upmove < 0;
	input.speed_scale = (ps->fd.forcePowersActive & (1 << FP_SPEED)) ? 1.6f : 1.0f;
	input.control = !(ps->pm_flags & PMF_TIME_KNOCKBACK)
		&& ps->forceHandExtend != HANDEXTEND_KNOCKDOWN
		&& ps->saberLockTime < move->cmd.serverTime;
	if (ps->fd.forcePower >= 10 && ps->fd.forcePowerLevel[FP_LEVITATION] > 0
		&& BG_CanUseFPNow(move->gametype, ps, move->cmd.serverTime, FP_LEVITATION))
		input.boost = 80.0f + 40.0f*(float)ps->fd.forcePowerLevel[FP_LEVITATION];
	VectorCopy(move->mins, world.mins);
	VectorCopy(move->maxs, world.maxs);
	world.gravity = (float)ps->gravity;
	world.trace = BG_FluxTrace;
	world.context = move;
	Flux_Step(&state, &input, &world, msec, &result);
	VectorCopy(state.origin, ps->origin);
	VectorCopy(state.velocity, ps->velocity);
	ps->stats[STAT_FLUX_YAW] = (int)(state.yaw*64.0f + 0.5f) % 23040;
	ps->stats[STAT_FLUX_FLAGS] = state.flags;
	ps->stats[STAT_FLUX_TRICK] = state.trick;
	ps->stats[STAT_FLUX_AIR_MS] = state.air_ms;
	ps->stats[STAT_FLUX_TRICK_MS] = state.trick_ms;
	ps->stats[STAT_FLUX_PUMP_MS] = state.pump_ms;
	ps->groundEntityNum = result.ground_entity < 0 ? ENTITYNUM_NONE : result.ground_entity;
	ps->fd.forcePower -= result.force_cost;
	if (result.force_cost)
		ps->fd.forceJumpZStart = ps->origin[2];
	if (input.jump)
		ps->pm_flags |= PMF_JUMP_HELD;
	else
		ps->pm_flags &= ~PMF_JUMP_HELD;
	move->xyspeed = Flux_Speed(&state);
}
