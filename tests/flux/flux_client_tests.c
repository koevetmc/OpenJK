#include "cgame/cg_local.h"
#include "game/bg_flux.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(test) do { if (!(test)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); exit(1); } } while (0)

static cgameImport_t imports;
static int polys, glyphs;
static float low_z, high_z;
static char speed_text[64], trick_text[64];

static qhandle_t Shader(const char *name)
{
	CHECK(!strcmp(name, "flux/deck"));
	return 1;
}

static void Polys(qhandle_t shader, int count, const polyVert_t *vertices, int number)
{
	int i, axis;
	CHECK(shader == 1 && number == 1 && count >= 3 && count <= 8);
	++polys;
	for (i = 0; i < count; ++i)
	{
		for (axis = 0; axis < 3; ++axis)
			CHECK(isfinite(vertices[i].xyz[axis]));
		CHECK(vertices[i].modulate[3] == 255);
		if (vertices[i].xyz[2] < low_z) low_z = vertices[i].xyz[2];
		if (vertices[i].xyz[2] > high_z) high_z = vertices[i].xyz[2];
	}
}

static void Color(const float *color)
{
	(void)color;
}

static qboolean Asian(void)
{
	return qfalse;
}

static void Picture(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t shader)
{
	(void)s2; (void)t2; (void)shader;
	CHECK(isfinite(x) && isfinite(y) && w > 0.0f && h > 0.0f);
	if (y == 320.0f || y == 342.0f)
	{
		int index = (int)((x-28.0f)/(float)SMALLCHAR_WIDTH);
		char *text = y == 320.0f ? speed_text : trick_text;
		CHECK(index >= 0 && index < 63);
		text[index] = (char)((int)(t1*16.0f)*16 + (int)(s1*16.0f));
	}
	++glyphs;
}

int main(void)
{
	centity_t board;
	playerState_t *ps = &cg.predictedPlayerState;
	memset(&board, 0, sizeof(board));
	memset(&cg, 0, sizeof(cg));
	memset(&cgs, 0, sizeof(cgs));
	imports.R_RegisterShader = Shader;
	imports.R_AddPolysToScene = Polys;
	imports.R_SetColor = Color;
	imports.R_DrawStretchPic = Picture;
	imports.R_Language_IsAsian = Asian;
	trap = &imports;
	cgs.screenXScale = cgs.screenYScale = 1.0f;
	board.currentState.otherEntityNum = ENTITYNUM_NONE;
	board.currentState.number = MAX_CLIENTS;
	CG_FluxBoard(&board);
	CHECK(polys == 50 && low_z >= 0.0f && high_z <= 6.001f);
	ps->stats[STAT_FLUX_BOARD] = MAX_CLIENTS+1;
	ps->stats[STAT_HEALTH] = 100;
	ps->origin[2] = 30.0f;
	ps->stats[STAT_FLUX_TRICK] = FLUX_TRICK_KICKFLIP;
	ps->stats[STAT_FLUX_TRICK_MS] = 1312;
	ps->velocity[0] = 300.0f;
	ps->velocity[1] = 400.0f;
	board.currentState.otherEntityNum = 0;
	CG_FluxBoard(&board);
	CHECK(polys == 100 && low_z < -5.0f && high_z > 5.0f);
	memset(speed_text, ' ', sizeof(speed_text)-1);
	memset(trick_text, ' ', sizeof(trick_text)-1);
	CG_FluxHUD();
	CHECK(glyphs > 20);
	CHECK(strstr(speed_text, "500 u/s") != NULL);
	CHECK(strstr(trick_text, "Kickflip") != NULL);
	glyphs = 0;
	cg.showScores = qtrue;
	CG_FluxHUD();
	CHECK(glyphs == 0);
	board.currentState.eFlags = EF_NODRAW;
	CG_FluxBoard(&board);
	CHECK(polys == 100);
	CHECK(!strcmp(Flux_TrickName(-1), "Cruising"));
	CHECK(!strcmp(Flux_TrickName(FLUX_TRICK_KICKFLIP), "Kickflip"));
	puts("PASS actual client procedural board submission, predicted trick rotation, HUD drawing and visibility");
	return 0;
}
