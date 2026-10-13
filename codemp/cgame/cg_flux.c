#include "cg_local.h"
#include "game/bg_flux.h"

static void CG_FluxPoly(qhandle_t shader, const vec3_t origin, vec3_t *axes,
	const vec3_t *points, int count, const byte *color)
{
	polyVert_t vertices[8];
	int i, j;
	for (i = 0; i < count; ++i)
	{
		VectorCopy(origin, vertices[i].xyz);
		for (j = 0; j < 3; ++j)
			VectorMA(vertices[i].xyz, points[i][j], axes[j], vertices[i].xyz);
		vertices[i].st[0] = vertices[i].st[1] = 0.5f;
		memcpy(vertices[i].modulate, color, 4);
	}
	trap->R_AddPolysToScene(shader, count, vertices, 1);
}

void CG_FluxBoard(centity_t *cent)
{
	static const float outline[8][2] = {
		{ -17.0f, -3.0f }, { -14.0f, -6.0f }, { 14.0f, -6.0f }, { 17.0f, -3.0f },
		{ 17.0f, 3.0f }, { 14.0f, 6.0f }, { -14.0f, 6.0f }, { -17.0f, 3.0f }
	};
	static const byte grip[4] = { 30, 38, 48, 255 };
	static const byte deck[4] = { 38, 200, 166, 255 };
	static const byte tire[4] = { 230, 180, 64, 255 };
	vec3_t origin, angles, axes[3], top[8], bottom[8], points[8];
	int owner = cent->currentState.otherEntityNum;
	int trick = cent->currentState.time;
	int remaining = cent->currentState.time2 - (cg.time-cent->currentState.apos.trTime);
	qhandle_t shader;
	int i, wheel, segment;
	if (cent->currentState.eFlags & EF_NODRAW)
		return;
	VectorCopy(cent->lerpOrigin, origin);
	VectorCopy(cent->lerpAngles, angles);
	if (owner == cg.predictedPlayerState.clientNum
		&& cg.predictedPlayerState.stats[STAT_FLUX_BOARD] == cent->currentState.number+1)
	{
		VectorCopy(cg.predictedPlayerState.origin, origin);
		origin[2] += MINS_Z-6.0f;
		angles[YAW] = (float)cg.predictedPlayerState.stats[STAT_FLUX_YAW]/64.0f;
		trick = cg.predictedPlayerState.stats[STAT_FLUX_TRICK];
		remaining = cg.predictedPlayerState.stats[STAT_FLUX_TRICK_MS];
	}
	else if (owner >= 0 && owner < MAX_CLIENTS && cg_entities[owner].currentValid)
	{
		VectorCopy(cg_entities[owner].lerpOrigin, origin);
		origin[2] += MINS_Z-6.0f;
	}
	if ((trick == FLUX_TRICK_KICKFLIP || trick == FLUX_TRICK_HEELFLIP) && remaining > 1050)
		angles[ROLL] = (float)(1400-remaining)*(360.0f/350.0f)*(trick == FLUX_TRICK_KICKFLIP ? 1.0f : -1.0f);
	AnglesToAxis(angles, axes);
	shader = trap->R_RegisterShader("flux/deck");
	for (i = 0; i < 8; ++i)
	{
		VectorSet(top[i], outline[i][0], outline[i][1], 6.0f);
		VectorSet(bottom[7-i], outline[i][0], outline[i][1], 4.0f);
	}
	CG_FluxPoly(shader, origin, axes, top, 8, grip);
	CG_FluxPoly(shader, origin, axes, bottom, 8, deck);
	for (i = 0; i < 8; ++i)
	{
		int next = (i+1)%8;
		VectorCopy(top[i], points[0]);
		VectorCopy(top[next], points[1]);
		VectorCopy(top[next], points[2]);
		VectorCopy(top[i], points[3]);
		points[2][2] = points[3][2] = 4.0f;
		CG_FluxPoly(shader, origin, axes, points, 4, deck);
	}
	for (wheel = 0; wheel < 4; ++wheel)
	{
		float x = (wheel & 1) ? 10.0f : -10.0f;
		float y = (wheel & 2) ? 6.0f : -6.0f;
		for (i = 0; i < 8; ++i)
		{
			float angle = (float)i*0.7853981634f;
			VectorSet(top[i], x+2.0f*cosf(angle), y-1.5f, 2.0f+2.0f*sinf(angle));
			VectorSet(bottom[i], top[i][0], y+1.5f, top[i][2]);
		}
		CG_FluxPoly(shader, origin, axes, top, 8, tire);
		CG_FluxPoly(shader, origin, axes, bottom, 8, tire);
		for (segment = 0; segment < 8; ++segment)
		{
			int next = (segment+1)%8;
			VectorCopy(top[segment], points[0]);
			VectorCopy(top[next], points[1]);
			VectorCopy(bottom[next], points[2]);
			VectorCopy(bottom[segment], points[3]);
			CG_FluxPoly(shader, origin, axes, points, 4, tire);
		}
	}
}

void CG_FluxHUD(void)
{
	const playerState_t *ps = &cg.predictedPlayerState;
	const vec4_t background = { 0.025f, 0.04f, 0.06f, 0.85f };
	const vec4_t accent = { 0.15f, 0.78f, 0.65f, 1.0f };
	char text[96];
	float speed;
	if (!BG_FluxRiding(ps) || cg.showScores || ps->persistant[PERS_TEAM] == TEAM_SPECTATOR)
		return;
	speed = sqrtf(ps->velocity[0]*ps->velocity[0]+ps->velocity[1]*ps->velocity[1]);
	Com_sprintf(text, sizeof(text), "Fluxboard  %d u/s", (int)speed);
	CG_FillRect(18.0f, 310.0f, 248.0f, 58.0f, background);
	CG_FillRect(18.0f, 310.0f, 3.0f, 58.0f, accent);
	CG_DrawSmallString(28, 320, text, 1.0f);
	CG_DrawSmallString(28, 342, Flux_TrickName(ps->stats[STAT_FLUX_TRICK]), 1.0f);
}
