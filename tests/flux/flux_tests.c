#include "bg_flux.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); exit(1); } } while (0)

typedef struct {
	float slope;
	int wall;
	int solid;
	int rail;
} test_world_t;

static void TraceBox(flux_trace_t *hit, const float *start, const float *mins,
	const float *maxs, const float *end, const float *low, const float *high)
{
	float enter = -1.0f, leave = 1.0f;
	float normal[3] = { 0.0f, 0.0f, 0.0f };
	int inside = 1, axis;
	for (axis = 0; axis < 3; ++axis)
	{
		float lo = low[axis] - maxs[axis], hi = high[axis] - mins[axis];
		float delta = end[axis] - start[axis];
		float a, b, sign;
		if (start[axis] <= lo || start[axis] >= hi)
			inside = 0;
		if (fabsf(delta) < 0.000001f)
		{
			if (start[axis] <= lo || start[axis] >= hi)
				return;
			continue;
		}
		a = (lo-start[axis])/delta;
		b = (hi-start[axis])/delta;
		sign = -1.0f;
		if (a > b)
		{
			float swap = a;
			a = b;
			b = swap;
			sign = 1.0f;
		}
		if (a > enter)
		{
			enter = a;
			memset(normal, 0, sizeof(normal));
			normal[axis] = sign;
		}
		if (b < leave)
			leave = b;
		if (enter > leave)
			return;
	}
	if (inside)
	{
		hit->solid = 1;
		hit->fraction = 0.0f;
	}
	else if (enter >= 0.0f && enter < hit->fraction && enter <= leave)
	{
		hit->fraction = enter;
		memcpy(hit->normal, normal, sizeof(normal));
		hit->entity = 7;
	}
}

static void TestTrace(void *context, flux_trace_t *hit, const float *start,
	const float *mins, const float *maxs, const float *end)
{
	test_world_t *world = (test_world_t *)context;
	float offset = mins[2] - world->slope*(world->slope > 0.0f ? maxs[0] : mins[0]);
	float from = start[2] - world->slope*start[0] + offset;
	float to = end[2] - world->slope*end[0] + offset;
	hit->fraction = 1.0f;
	hit->entity = -1;
	if (world->solid || from < -0.05f)
	{
		hit->solid = 1;
		hit->fraction = 0.0f;
		return;
	}
	if (to < from && to < 0.0f)
	{
		float length = sqrtf(1.0f+world->slope*world->slope);
		hit->fraction = fmaxf(0.0f, from/(from-to));
		hit->normal[0] = -world->slope/length;
		hit->normal[1] = 0.0f;
		hit->normal[2] = 1.0f/length;
		hit->entity = 0;
	}
	if (world->wall)
	{
		static const float low[3] = { 160.0f, -10000.0f, 0.0f };
		static const float high[3] = { 180.0f, 10000.0f, 400.0f };
		TraceBox(hit, start, mins, maxs, end, low, high);
	}
	if (world->rail)
	{
		static const float low[3] = { -200.0f, -5.0f, 0.0f };
		static const float high[3] = { 400.0f, 5.0f, 40.0f };
		TraceBox(hit, start, mins, maxs, end, low, high);
	}
}

static flux_world_t World(test_world_t *geometry)
{
	flux_world_t world = { { -15.0f, -15.0f, -24.0f }, { 15.0f, 15.0f, 32.0f }, 800.0f, TestTrace, NULL };
	world.context = geometry;
	return world;
}

static flux_state_t Rider(void)
{
	flux_state_t state;
	memset(&state, 0, sizeof(state));
	state.origin[2] = 24.0f;
	return state;
}

static flux_input_t Input(void)
{
	flux_input_t input;
	memset(&input, 0, sizeof(input));
	input.control = 1;
	input.speed_scale = 1.0f;
	return input;
}

static void Run(flux_state_t *state, flux_input_t *input, flux_world_t *world, int frames)
{
	int i;
	flux_result_t result;
	for (i = 0; i < frames; ++i)
	{
		Flux_Step(state, input, world, 8, &result);
		CHECK(isfinite(state->origin[0]) && isfinite(state->origin[1]) && isfinite(state->origin[2]));
		CHECK(isfinite(Flux_Speed(state)));
	}
}

static void TestMovement(void)
{
	test_world_t geometry = { 0 };
	flux_world_t world = World(&geometry);
	flux_input_t input = Input();
	flux_state_t state = Rider();
	float speed;
	input.forward = 1.0f;
	Run(&state, &input, &world, 250);
	CHECK(Flux_Speed(&state) > 400.0f && Flux_Speed(&state) < 630.0f);
	CHECK(fabsf(state.origin[2] - 24.0f) < 0.01f);
	speed = Flux_Speed(&state);
	input.forward = 0.0f;
	Run(&state, &input, &world, 125);
	CHECK(Flux_Speed(&state) < speed);
	input.steer = 1.0f;
	Run(&state, &input, &world, 100);
	CHECK(state.velocity[1] < -30.0f);
	input.forward = -1.0f;
	input.steer = 0.0f;
	Run(&state, &input, &world, 250);
	CHECK(Flux_Speed(&state) < 0.1f);
	state = Rider();
	geometry.wall = 1;
	input.forward = 1.0f;
	Run(&state, &input, &world, 500);
	CHECK(state.origin[0] <= 145.01f);
	geometry.wall = 0;
	geometry.slope = -0.2f;
	state = Rider();
	state.origin[2] = 27.0f;
	input.forward = 0.0f;
	Run(&state, &input, &world, 125);
	CHECK(state.velocity[0] > 100.0f);
	geometry.slope = 0.0f;
	geometry.solid = 1;
	state = Rider();
	state.velocity[0] = 600.0f;
	Run(&state, &input, &world, 20);
	CHECK(Flux_Speed(&state) == 0.0f);
	puts("PASS push, drag, carving, brake, wall collision, slope gravity, solid recovery");
}

int main(void)
{
	TestMovement();
	return 0;
}
