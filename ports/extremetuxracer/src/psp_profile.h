// PSP port, 2026-10-01: scoped sub-section timing for hardware benchmarks.
// Costs one branch outside a benchmark run. See docs/psp-hardware-validation.md.
#ifndef PSP_PROFILE_H
#define PSP_PROFILE_H

enum PspProfileSlot {
	PSP_SUB_QUAD_UPDATE, PSP_SUB_QUAD_TRAVERSE, PSP_SUB_TERRAIN_CLIP, PSP_SUB_TERRAIN_DRAW,
	PSP_SUB_TRACKMARKS, PSP_SUB_TREES, PSP_SUB_ITEMS, PSP_SUB_HUD_GAUGE, PSP_SUB_HUD_TEXT,
	PSP_SUB_PHYSICS_POS, PSP_SUB_VIEW, PSP_SUB_SKY, PSP_SUB_SNOW_UPDATE, PSP_SUB_FLAKES, PSP_SUB_CURTAINS,
	PSP_SUB_OPP_UPDATE, PSP_SUB_OPP_DRAW,
	// Counters from here on: a sum per frame, not microseconds.
	PSP_N_TRI_INSIDE, PSP_N_TRI_BOUNDARY, PSP_N_TRI_CLIPPED, PSP_N_OUTCODES, PSP_N_TRACK_SEEN,
	PSP_N_TRACK_DRAWN, PSP_N_TREES_DRAWN, PSP_N_QUAD_NODES, PSP_SUB_COUNT
};

bool PspProfileActive();
unsigned long long PspProfileNow();
void PspProfileAdd(unsigned slot, unsigned long long us);

struct PspProfileScope {
	unsigned slot;
	unsigned long long start;
	explicit PspProfileScope(unsigned s) : slot(s), start(PspProfileActive() ? PspProfileNow() : 0) {}
	~PspProfileScope() { if (start) PspProfileAdd(slot, PspProfileNow() - start); }
};

#endif
