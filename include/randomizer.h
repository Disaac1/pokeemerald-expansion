#ifndef GUARD_RANDOMIZER_H
#define GUARD_RANDOMIZER_H

enum RandomizerMode
{
    RANDOMIZER_MODE_OFF,
    RANDOMIZER_MODE_STATIC,        // Global 1-1 swap (Deterministic)
    RANDOMIZER_MODE_CHAOS,         // True chaos (Every encounter is random)
    RANDOMIZER_MODE_CHAOS_SIMILAR, // Chaos but within BST range
    RANDOMIZER_MODE_AREA_STATIC,   // 1-1 swap but unique per route/area
    RANDOMIZER_MODE_COUNT
};

void ShuffleRandomizerTable(u32 seed);
u16 RandomizeSpecies(u16 species, u16 mapSecId);
u16 RandomizeSpeciesEx(u16 species, u16 mapSecId, bool8 isWild);
u16 RandomizeSpeciesForEditor(u16 species);
u16 RandomizeMove(u16 move, u16 species, u8 slot);
u16 RandomizeAbility(u16 species, u8 slot);
u16 RandomizeItem(u16 itemId);
void RandomizeItemScriptHook(void);
bool8 IsRandomizerEnabled(void);
u32 GetRandomizerSeed(void);
u8 GetRandomizerMode(void);

extern u16 *gRandomizerTablePtr;
extern bool8 gRandomizerFinished;

struct RandomizerTelemetry
{
    u32 seed;
    u8 mode;
    u8 wildcardChance;
    u8 itemRandoEnabled;
    u8 moveRandoEnabled;
    u8 abilityRandoEnabled;
    u16 poolSize;
    const u16 *speciesPool;
};

extern struct RandomizerTelemetry gRandomizerTelemetry;

#endif // GUARD_RANDOMIZER_H
