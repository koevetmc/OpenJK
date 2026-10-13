#ifndef BG_FLUX_H
#define BG_FLUX_H

#define FLUX_GROUNDED 1
#define FLUX_JUMP_HELD 2
#define FLUX_CROUCH_HELD 4
#define FLUX_GRINDING 8
#define FLUX_BOOSTED 16
#define FLUX_LEFT_HELD 32
#define FLUX_RIGHT_HELD 64
#define FLUX_OLLIE 128

#define FLUX_TRICK_NONE 0
#define FLUX_TRICK_OLLIE 1
#define FLUX_TRICK_KICKFLIP 2
#define FLUX_TRICK_HEELFLIP 3
#define FLUX_TRICK_GRIND 4
#define FLUX_TRICK_BOOST 5
#define FLUX_TRICK_BAIL 6

#define FLUX_BOARD_MARKER 197
#define FLUX_USE_HELD 256

typedef struct {
	float fraction;
	float normal[3];
	int solid;
	int entity;
} flux_trace_t;

typedef void (*flux_trace_fn)(void *context, flux_trace_t *hit,
	const float *start, const float *mins, const float *maxs, const float *end);

typedef struct {
	float origin[3];
	float velocity[3];
	float yaw;
	int flags;
	int trick;
	int air_ms;
	int trick_ms;
	int pump_ms;
} flux_state_t;

typedef struct {
	float forward;
	float steer;
	int jump;
	int crouch;
	float speed_scale;
	float boost;
	int control;
} flux_input_t;

typedef struct {
	float mins[3];
	float maxs[3];
	float gravity;
	flux_trace_fn trace;
	void *context;
} flux_world_t;

typedef struct {
	int ground_entity;
	int force_cost;
	int blocked;
} flux_result_t;

void Flux_Step(flux_state_t *state, const flux_input_t *input,
	const flux_world_t *world, int msec, flux_result_t *result);
float Flux_Speed(const flux_state_t *state);
const char *Flux_TrickName(int trick);

#endif
