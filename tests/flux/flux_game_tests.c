#include "game/g_local.h"
#include "game/bg_local.h"
#define main FluxPhysicsMain
#include "flux_tests.c"
#undef main

static gameImport_t imports;
static gclient_t clients[2];
static test_world_t geometry;
static int links;
static int blocked;
static int include_target;
static animation_t animations[MAX_TOTALANIMATIONS];
static char configstrings[MAX_CONFIGSTRINGS][MAX_STRING_CHARS];

static void EngineTrace(trace_t *out, const vec3_t start, const vec3_t mins,
	const vec3_t maxs, const vec3_t end, int pass, int mask, int capsule, int flags, int lod)
{
	flux_trace_t hit = { 0 };
	vec3_t zero = { 0.0f, 0.0f, 0.0f };
	int axis;
	(void)pass; (void)mask; (void)capsule; (void)flags; (void)lod;
	memset(out, 0, sizeof(*out));
	TestTrace(&geometry, &hit, start, mins ? mins : zero, maxs ? maxs : zero, end);
	out->fraction = blocked ? 0.0f : hit.fraction;
	out->startsolid = out->allsolid = hit.solid ? qtrue : qfalse;
	out->entityNum = hit.entity < 0 ? ENTITYNUM_NONE : ENTITYNUM_WORLD;
	VectorCopy(hit.normal, out->plane.normal);
	for (axis = 0; axis < 3; ++axis)
		out->endpos[axis] = start[axis] + (end[axis]-start[axis])*out->fraction;
}

static void MoveTrace(trace_t *out, const vec3_t start, const vec3_t mins,
	const vec3_t maxs, const vec3_t end, int pass, int mask)
{
	EngineTrace(out, start, mins, maxs, end, pass, mask, 0, 0, 0);
}

static void Link(sharedEntity_t *ent)
{
	int axis;
	++links;
	for (axis = 0; axis < 3; ++axis)
	{
		ent->r.absmin[axis] = ent->r.currentOrigin[axis]+ent->r.mins[axis];
		ent->r.absmax[axis] = ent->r.currentOrigin[axis]+ent->r.maxs[axis];
	}
}

static void FreeScript(sharedEntity_t *ent)
{
	(void)ent;
}

static int Contents(const vec3_t point, int pass)
{
	(void)point; (void)pass;
	return 0;
}

static void ConfigGet(int index, char *buffer, int size)
{
	CHECK(index >= 0 && index < MAX_CONFIGSTRINGS);
	snprintf(buffer, (size_t)size, "%s", configstrings[index]);
}

static void ConfigSet(int index, const char *value)
{
	CHECK(index >= 0 && index < MAX_CONFIGSTRINGS);
	snprintf(configstrings[index], MAX_STRING_CHARS, "%s", value);
}

static int Entities(const vec3_t low, const vec3_t high, int *list, int size)
{
	(void)low; (void)high;
	if (include_target && size > 0)
	{
		list[0] = 1;
		return 1;
	}
	return 0;
}

static qboolean Visible(const vec3_t a, const vec3_t b)
{
	(void)a; (void)b;
	return qtrue;
}

static void Locate(sharedEntity_t *entities, int count, int stride, playerState_t *states, int clientstride)
{
	(void)entities; (void)count; (void)stride; (void)states; (void)clientstride;
}

static void QDECL Fail(int code, const char *format, ...)
{
	(void)code;
	fprintf(stderr, "engine error: %s\n", format);
	exit(1);
}

static void QDECL Print(const char *format, ...)
{
	(void)format;
}

static void Setup(void)
{
	int i;
	memset(&imports, 0, sizeof(imports));
	memset(g_entities, 0, sizeof(g_entities));
	memset(clients, 0, sizeof(clients));
	memset(&level, 0, sizeof(level));
	memset(&geometry, 0, sizeof(geometry));
	memset(configstrings, 0, sizeof(configstrings));
	blocked = 0;
	include_target = 0;
	for (i = 0; i < MAX_TOTALANIMATIONS; ++i)
	{
		animations[i].numFrames = 10;
		animations[i].frameLerp = 50;
	}
	bgAllAnims[0].anims = animations;
	imports.InPVS = Visible;
	imports.Trace = EngineTrace;
	imports.LinkEntity = Link;
	imports.ICARUS_FreeEnt = FreeScript;
	imports.PointContents = Contents;
	imports.GetConfigstring = ConfigGet;
	imports.SetConfigstring = ConfigSet;
	imports.EntitiesInBox = Entities;
	imports.LocateGameData = Locate;
	imports.Error = Fail;
	imports.Print = Print;
	trap = &imports;
	level.time = 1000;
	level.num_entities = MAX_CLIENTS+1;
	level.maxclients = 2;
	level.clients = clients;
	g_gravity.value = 800.0f;
	for (i = 0; i < 2; ++i)
	{
		gentity_t *rider = &g_entities[i];
		playerState_t *ps = &clients[i].ps;
		rider->s.number = i;
		rider->s.eType = ET_PLAYER;
		rider->classname = "player";
		rider->inuse = qtrue;
		rider->health = 100;
		rider->client = &clients[i];
		rider->playerState = ps;
		clients[i].pers.connected = CON_CONNECTED;
		ps->clientNum = i;
		ps->pm_type = PM_NORMAL;
		ps->stats[STAT_HEALTH] = 100;
		ps->stats[STAT_MAX_HEALTH] = 100;
		ps->stats[STAT_WEAPONS] = 1 << WP_SABER;
		ps->weapon = WP_SABER;
		ps->weaponstate = WEAPON_READY;
		ps->saberMove = LS_READY;
		ps->fd.saberAnimLevel = SS_FAST;
		ps->gravity = 800;
		ps->speed = 250;
		ps->standheight = 32;
		ps->crouchheight = 16;
		ps->viewheight = 26;
		ps->groundEntityNum = ENTITYNUM_WORLD;
		ps->origin[2] = 24.0f;
		ps->fd.forcePower = 100;
		ps->fd.forcePowerMax = 100;
		ps->fd.forcePowerLevel[FP_PUSH] = 3;
		ps->fd.forcePowerLevel[FP_PULL] = 3;
		ps->fd.forcePowerLevel[FP_SPEED] = 3;
		ps->fd.forcePowerLevel[FP_LEVITATION] = 3;
		ps->fd.forcePowersKnown = (1 << FP_PUSH) | (1 << FP_PULL) | (1 << FP_SPEED) | (1 << FP_LEVITATION);
		VectorSet(rider->r.mins, -15.0f, -15.0f, -24.0f);
		VectorSet(rider->r.maxs, 15.0f, 15.0f, 32.0f);
	}
	g_entities[MAX_CLIENTS].s.number = MAX_CLIENTS;
	g_entities[MAX_CLIENTS].inuse = qtrue;
	g_entities[MAX_CLIENTS].classname = "misc_fluxboard";
	g_entities[MAX_CLIENTS].s.origin[0] = 40.0f;
	SP_misc_fluxboard(&g_entities[MAX_CLIENTS]);
}

static void TestMount(void)
{
	gentity_t *rider = &g_entities[0], *board = &g_entities[MAX_CLIENTS];
	usercmd_t cmd = { 0 };
	Setup();
	cmd.buttons = BUTTON_USE;
	G_FluxInput(rider, &cmd);
	CHECK(BG_FluxRiding(&rider->client->ps));
	CHECK(board->s.otherEntityNum == 0);
	CHECK(rider->client->ps.m_iVehicleNum == 0);
	G_FluxInput(rider, &cmd);
	CHECK(BG_FluxRiding(&rider->client->ps));
	G_FluxInput(&g_entities[1], &cmd);
	CHECK(!BG_FluxRiding(&clients[1].ps));
	cmd.buttons = 0;
	G_FluxInput(rider, &cmd);
	cmd.buttons = BUTTON_USE;
	G_FluxInput(rider, &cmd);
	CHECK(!BG_FluxRiding(&clients[0].ps) && board->s.otherEntityNum == ENTITYNUM_NONE);
	G_FluxInput(rider, &cmd);
	CHECK(!BG_FluxRiding(&clients[0].ps));
	Setup();
	blocked = 1;
	G_FluxInput(rider, &cmd);
	CHECK(!BG_FluxRiding(&clients[0].ps));
	Setup();
	G_FluxInput(rider, &cmd);
	rider->health = 0;
	G_FluxUpdate(rider);
	CHECK(!clients[0].ps.stats[STAT_FLUX_BOARD] && board->s.otherEntityNum == ENTITYNUM_NONE);
	Setup();
	G_FluxInput(rider, &cmd);
	clients[0].pers.connected = CON_DISCONNECTED;
	level.time += 50;
	board->think(board);
	CHECK(!clients[0].ps.stats[STAT_FLUX_BOARD] && board->s.otherEntityNum == ENTITYNUM_NONE);
	Setup();
	G_FluxInput(rider, &cmd);
	board->use(board, NULL, NULL);
	CHECK(!clients[0].ps.stats[STAT_FLUX_BOARD] && board->genericValue1);
	CHECK(links > 0);
	puts("PASS actual board spawn/use, ownership, debounce, blocked mount, death, disconnect, disable");
}

static pmove_t Move(void)
{
	pmove_t move;
	memset(&move, 0, sizeof(move));
	move.ps = &clients[0].ps;
	move.cmd.serverTime = level.time;
	move.cmd.weapon = WP_SABER;
	move.cmd.forcesel = move.cmd.invensel = (byte)-1;
	move.animations = animations;
	move.trace = MoveTrace;
	move.pointcontents = Contents;
	move.tracemask = MASK_PLAYERSOLID;
	move.baseEnt = (bgEntity_t *)g_entities;
	move.entSize = sizeof(gentity_t);
	VectorSet(move.mins, -15.0f, -15.0f, -30.0f);
	VectorSet(move.maxs, 15.0f, 15.0f, 32.0f);
	return move;
}

static void TestForceCompatibility(void)
{
	usercmd_t cmd = { 0 };
	pmove_t move;
	float before;
	int power;
	Setup();
	cmd.buttons = BUTTON_USE;
	G_FluxInput(&g_entities[0], &cmd);
	move = Move();
	move.cmd.forwardmove = 127;
	BG_FluxMove(&move, 200);
	before = clients[0].ps.origin[0];
	power = clients[0].ps.fd.forcePower;
	include_target = 1;
	clients[1].ps.origin[0] = 400.0f;
	clients[1].ps.fd.forcePowerLevel[FP_PUSH] = 0;
	clients[1].ps.fd.forcePowerLevel[FP_PULL] = 0;
	VectorCopy(clients[1].ps.origin, g_entities[1].r.currentOrigin);
	Link((sharedEntity_t *)&g_entities[1]);
	ForceThrow(&g_entities[0], qfalse);
	CHECK(clients[1].ps.velocity[0] > 100.0f);
	CHECK(clients[0].ps.forceHandExtend == HANDEXTEND_FORCEPUSH);
	CHECK(clients[0].ps.fd.forcePower < power);
	move.ps->commandTime = level.time;
	move.cmd.serverTime = level.time+200;
	Pmove(&move);
	CHECK(clients[0].ps.origin[0] > before && BG_FluxRiding(&clients[0].ps));
	clients[0].ps.forceHandExtend = HANDEXTEND_NONE;
	clients[0].ps.powerups[PW_DISINT_4] = 0;
	ForceThrow(&g_entities[0], qtrue);
	CHECK(clients[0].ps.forceHandExtend == HANDEXTEND_FORCEPULL);
	CHECK(clients[1].ps.velocity[0] < -100.0f);
	clients[0].ps.forceHandExtend = HANDEXTEND_NONE;
	clients[0].ps.fd.forcePower = 100;
	ForceSpeed(&g_entities[0], 0);
	CHECK(clients[0].ps.fd.forcePowersActive & (1 << FP_SPEED));
	move.cmd.buttons = BUTTON_ATTACK;
	move.cmd.generic_cmd = GENCMD_FORCE_THROW;
	BG_FluxMove(&move, 8);
	CHECK(move.cmd.buttons & BUTTON_ATTACK);
	CHECK(move.cmd.generic_cmd == GENCMD_FORCE_THROW);
	CHECK(clients[0].ps.weapon == WP_SABER && clients[0].ps.m_iVehicleNum == 0);
	puts("PASS actual ForceThrow push/pull activation and ForceSpeed while mounted; combat inputs preserved");
}

static void TestPmoveScript(void)
{
	pmove_t move;
	usercmd_t cmd = { 0 };
	int frame, attacks = 0, tricks = 0;
	Setup();
	cmd.buttons = BUTTON_USE;
	G_FluxInput(&g_entities[0], &cmd);
	move = Move();
	move.ps->commandTime = level.time;
	move.ps->legsAnim = move.ps->torsoAnim = BOTH_STAND1;
	for (frame = 0; frame < 37500; ++frame)
	{
		int phase = (frame*8) % 4000;
		level.time += 8;
		move.cmd.serverTime = level.time;
		move.cmd.forwardmove = 127;
		move.cmd.rightmove = phase > 1000 && phase < 2500 ? 64 : 0;
		move.cmd.upmove = phase < 120 ? 127 : (phase > 2800 && phase < 3050 ? -127 : 0);
		move.cmd.buttons = phase > 1400 && phase < 1900 ? BUTTON_ATTACK : 0;
		if (phase >= 64 && phase < 96)
			move.cmd.rightmove = -127;
		Pmove(&move);
		G_FluxUpdate(&g_entities[0]);
		CHECK(BG_FluxRiding(move.ps));
		CHECK(isfinite(move.ps->origin[0]) && isfinite(move.ps->origin[1]) && isfinite(move.ps->origin[2]));
		CHECK(move.ps->origin[2] >= 29.9f && VectorLength(move.ps->velocity) < 2000.0f);
		if (BG_SaberInAttack(move.ps->saberMove)) ++attacks;
		if (move.ps->stats[STAT_FLUX_TRICK] == FLUX_TRICK_KICKFLIP) ++tricks;
	}
	CHECK(attacks > 0 && tricks > 0);
	printf("PASS 300000 ms actual Pmove script: 37500 commands, %d attack frames, %d flip frames\n", attacks, tricks);
}

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	TestMount();
	TestForceCompatibility();
	TestPmoveScript();
	return 0;
}
