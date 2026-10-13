#include "bg_flux.h"
#include <math.h>
#include <string.h>

static float Flux_Dot(const float *a, const float *b)
{
	return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

static float Flux_Clamp(float value, float low, float high)
{
	return value < low ? low : (value > high ? high : value);
}

float Flux_Speed(const flux_state_t *state)
{
	return sqrtf(state->velocity[0]*state->velocity[0] + state->velocity[1]*state->velocity[1]);
}

const char *Flux_TrickName(int trick)
{
	static const char *names[] = { "Cruising", "Ollie", "Kickflip", "Heelflip", "Edge slide", "Lift ollie", "Bail" };
	return trick >= 0 && trick <= FLUX_TRICK_BAIL ? names[trick] : names[0];
}

static void Flux_Trace(const flux_world_t *world, flux_trace_t *hit,
	const float *start, const float *end, int point)
{
	static const float zero[3] = { 0.0f, 0.0f, 0.0f };
	memset(hit, 0, sizeof(*hit));
	hit->fraction = 1.0f;
	hit->entity = -1;
	world->trace(world->context, hit, start, point ? zero : world->mins,
		point ? zero : world->maxs, end);
}

static void Flux_Clip(float *velocity, const float *normal)
{
	float into = Flux_Dot(velocity, normal);
	int axis;
	if (into >= 0.0f)
		return;
	for (axis = 0; axis < 3; ++axis)
		velocity[axis] -= normal[axis] * into;
}

static int Flux_Slide(flux_state_t *state, const flux_world_t *world, float dt)
{
	float planes[4][3];
	int count = 0;
	int bump;
	for (bump = 0; bump < 4 && dt > 0.00001f; ++bump)
	{
		float end[3];
		flux_trace_t hit;
		int axis, plane;
		for (axis = 0; axis < 3; ++axis)
			end[axis] = state->origin[axis] + state->velocity[axis]*dt;
		Flux_Trace(world, &hit, state->origin, end, 0);
		if (hit.solid)
		{
			memset(state->velocity, 0, sizeof(state->velocity));
			return 1;
		}
		for (axis = 0; axis < 3; ++axis)
			state->origin[axis] += (end[axis] - state->origin[axis])*hit.fraction;
		if (hit.fraction >= 1.0f)
			return 0;
		dt *= 1.0f - hit.fraction;
		memcpy(planes[count++], hit.normal, sizeof(hit.normal));
		for (plane = 0; plane < count; ++plane)
			Flux_Clip(state->velocity, planes[plane]);
		for (plane = 0; plane < count; ++plane)
		{
			if (Flux_Dot(state->velocity, planes[plane]) < -0.01f)
			{
				memset(state->velocity, 0, sizeof(state->velocity));
				return 1;
			}
		}
	}
	return 0;
}

static void Flux_Tick(flux_state_t *state, const flux_input_t *input,
	const flux_world_t *world, int msec, flux_result_t *result)
{
	float dt = (float)msec * 0.001f;
	float down[3], forward[3], right[3];
	float speed = Flux_Speed(state);
	float steer = input->control ? Flux_Clamp(input->steer, -1.0f, 1.0f) : 0.0f;
	float push = input->control ? Flux_Clamp(input->forward, -1.0f, 1.0f) : 0.0f;
	float scale = Flux_Clamp(input->speed_scale, 1.0f, 2.0f);
	flux_trace_t ground;
	int grounded, axis;
	memcpy(down, state->origin, sizeof(down));
	down[2] -= 2.0f;
	Flux_Trace(world, &ground, state->origin, down, 0);
	grounded = !ground.solid && ground.fraction < 1.0f && ground.normal[2] >= 0.65f
		&& Flux_Dot(state->velocity, ground.normal) <= 1.0f;
	state->flags &= ~FLUX_GROUNDED;
	result->ground_entity = -1;
	if (grounded)
	{
		state->flags |= FLUX_GROUNDED;
		result->ground_entity = ground.entity;
		for (axis = 0; axis < 3; ++axis)
			state->origin[axis] += (down[axis] - state->origin[axis])*ground.fraction;
		Flux_Clip(state->velocity, ground.normal);
	}
	state->trick_ms = state->trick_ms > msec ? state->trick_ms-msec : 0;
	if (!state->trick_ms)
		state->trick = FLUX_TRICK_NONE;
	if (grounded)
	{
		if (state->air_ms && (state->trick == FLUX_TRICK_KICKFLIP || state->trick == FLUX_TRICK_HEELFLIP)
			&& state->trick_ms > 1050)
		{
			state->trick = FLUX_TRICK_BAIL;
			state->trick_ms = 1000;
			state->velocity[0] *= 0.4f;
			state->velocity[1] *= 0.4f;
		}
		state->air_ms = 0;
		state->flags &= ~(FLUX_BOOSTED | FLUX_OLLIE);
		if (input->control && input->jump && !(state->flags & FLUX_JUMP_HELD))
		{
			state->velocity[2] = 270.0f;
			state->flags |= FLUX_OLLIE;
			state->flags &= ~(FLUX_GROUNDED | FLUX_GRINDING);
			state->trick = FLUX_TRICK_OLLIE;
			state->trick_ms = 1400;
			grounded = 0;
			result->ground_entity = -1;
		}
	}
	if (!grounded)
	{
		state->air_ms = state->air_ms < 32000-msec ? state->air_ms+msec : 32000;
		if (input->control && (state->flags & FLUX_OLLIE))
		{
			if (state->trick == FLUX_TRICK_OLLIE && state->air_ms >= 40 && state->air_ms <= 260)
			{
				if (steer < -0.5f && !(state->flags & FLUX_LEFT_HELD))
					state->trick = FLUX_TRICK_KICKFLIP;
				if (steer > 0.5f && !(state->flags & FLUX_RIGHT_HELD))
					state->trick = FLUX_TRICK_HEELFLIP;
				if (state->trick != FLUX_TRICK_OLLIE)
					state->trick_ms = 1400;
			}
			if (input->jump && state->air_ms >= 150 && state->air_ms <= 260
				&& !(state->flags & FLUX_BOOSTED) && input->boost > 0.0f)
			{
				state->velocity[2] += Flux_Clamp(input->boost, 0.0f, 240.0f);
				state->flags |= FLUX_BOOSTED;
				result->force_cost += 10;
				if (state->trick == FLUX_TRICK_OLLIE)
					state->trick = FLUX_TRICK_BOOST;
			}
		}
	}
	if (input->control && input->crouch && grounded)
		state->pump_ms = state->pump_ms < 2000-msec ? state->pump_ms+msec : 2000;
	else
	{
		if (input->control && grounded && !input->crouch && (state->flags & FLUX_CROUCH_HELD)
			&& state->pump_ms >= 160 && state->pump_ms <= 650 && speed > 80.0f && speed < 700.0f*scale)
		{
			float gain = 1.0f + (30.0f + fabsf(state->velocity[2])*0.10f)/speed;
			state->velocity[0] *= gain;
			state->velocity[1] *= gain;
		}
		state->pump_ms = 0;
	}
	state->flags &= ~(FLUX_JUMP_HELD | FLUX_CROUCH_HELD | FLUX_LEFT_HELD | FLUX_RIGHT_HELD);
	if (input->jump) state->flags |= FLUX_JUMP_HELD;
	if (input->crouch) state->flags |= FLUX_CROUCH_HELD;
	if (steer < -0.5f) state->flags |= FLUX_LEFT_HELD;
	if (steer > 0.5f) state->flags |= FLUX_RIGHT_HELD;
	state->yaw -= steer * (grounded ? 115.0f : 75.0f) * dt * Flux_Clamp(speed / 160.0f, 0.2f, 1.0f);
	state->yaw = fmodf(state->yaw + 360.0f, 360.0f);
	forward[0] = cosf(state->yaw * 0.01745329252f);
	forward[1] = sinf(state->yaw * 0.01745329252f);
	forward[2] = 0.0f;
	right[0] = forward[1];
	right[1] = -forward[0];
	right[2] = 0.0f;
	if (grounded)
	{
		float lateral = Flux_Dot(state->velocity, right);
		float drag = (12.0f + 0.00010f*speed*speed + (push < 0.0f ? 330.0f*-push : 0.0f))*dt;
		float fraction = speed > 0.0f ? fmaxf(0.0f, speed-drag)/speed : 0.0f;
		for (axis = 0; axis < 2; ++axis)
			state->velocity[axis] = (state->velocity[axis] - right[axis]*lateral*fminf(1.0f, 6.0f*dt))*fraction;
		if (push > 0.0f && speed < 620.0f*scale)
		{
			for (axis = 0; axis < 2; ++axis)
				state->velocity[axis] += forward[axis]*push*240.0f*scale*dt;
		}
		for (axis = 0; axis < 3; ++axis)
			state->velocity[axis] += ground.normal[axis]*ground.normal[2]*world->gravity*dt;
		state->velocity[2] -= world->gravity*dt;
		Flux_Clip(state->velocity, ground.normal);
	}
	else
	{
		for (axis = 0; axis < 2; ++axis)
			state->velocity[axis] += (forward[axis]*push + right[axis]*steer)*40.0f*dt;
		state->velocity[2] -= world->gravity*dt;
	}
	result->blocked |= Flux_Slide(state, world, dt);
}

void Flux_Step(flux_state_t *state, const flux_input_t *input,
	const flux_world_t *world, int msec, flux_result_t *result)
{
	memset(result, 0, sizeof(*result));
	result->ground_entity = -1;
	if (msec <= 0 || !world->trace)
		return;
	if (msec > 200)
		msec = 200;
	while (msec > 0)
	{
		int step = msec > 8 ? 8 : msec;
		Flux_Tick(state, input, world, step, result);
		msec -= step;
	}
}
