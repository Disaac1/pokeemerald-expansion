#ifndef GUARD_NUZLOCKE_H
#define GUARD_NUZLOCKE_H

#include "global.h"
#include "config/nuzlocke.h"

// Core Nuzlocke Logic
bool8 Nuzlocke_CanCatchEncounter(u16 species);
void Nuzlocke_OnCatch(struct Pokemon *mon);
bool8 Nuzlocke_IsSpeciesCaught(u16 species);
void Nuzlocke_InitWildBattle(u16 species);
void Nuzlocke_EndWildBattle(u8 outcome);
#define FLAG_NUZLOCKE_RUN_LOST 0x20 // Unused Flag

void Nuzlocke_HandlePostBattleCleanup(void);
void Nuzlocke_HandleWhiteOut(void);
bool32 IsItemInfinite(u16 itemId);

// Helper for PC box movement
void Nuzlocke_MoveToGraveyard(struct Pokemon *mon);

// Save data access
bool8 Nuzlocke_GetEncounterFlag(u16 mapSecId);
void Nuzlocke_SetEncounterFlag(u16 mapSecId);
void Nuzlocke_ClearEncounterFlag(u16 mapSecId);

extern bool8 gNuzlockeValidEncounterActive;
extern bool8 gNuzlockeDuplicateEncounterActive;


#endif // GUARD_NUZLOCKE_H
