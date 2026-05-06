#include "global.h"
#include "random.h"
#include "nuzlocke.h"
#include "pokemon.h"
#include "move.h"
#include "battle.h"
#include "randomizer.h"
#include "item.h"
#include "event_data.h"
#include "constants/items.h"
#include "constants/species.h"
#include "constants/pokemon.h"
#include "constants/moves.h"
#include "constants/abilities.h"
#include "data/randomizer_data.h"

#define ITEM_BALL_RNG_MODIFIER 0x5EED

// The randomizer will shuffle species in this range
#define RANDOMIZER_MAX_SPECIES (NUM_SPECIES - 1)

// We use a specific magic value to detect if the table is truly initialized
#define RANDOMIZER_MAGIC 0xbeef

EWRAM_DATA u16 sRandomizerTable[NUM_SPECIES];
EWRAM_DATA u16 sRandomizerCheck = 0;

static EWRAM_DATA u16 sMovePool[MOVES_COUNT_ALL];
static EWRAM_DATA u16 sMovePoolByType[NUMBER_OF_MON_TYPES][200];
static EWRAM_DATA u16 sMoveCountByType[NUMBER_OF_MON_TYPES];
static EWRAM_DATA u16 sAbilityPool[ABILITIES_COUNT];
static u32 sMovePoolSize = 0;
static u32 sAbilityPoolSize = 0;

EWRAM_DATA u16 *gRandomizerTablePtr = NULL;
EWRAM_DATA bool8 gRandomizerFinished = FALSE;
EWRAM_DATA struct RandomizerTelemetry gRandomizerTelemetry = {0};

// Simple deterministic RNG for shuffling based on the seed
static u32 sRandomizerRng;

static void SeedRandomizerRng(u32 seed)
{
    sRandomizerRng = seed;
}

static u32 RandomizerNext(void)
{
    sRandomizerRng = 1103515245 * sRandomizerRng + 12345;
    return (sRandomizerRng >> 16) & 0x7FFF;
}

static void InitAbilityPool(void)
{
    u32 i;
    sAbilityPoolSize = 0;
    for (i = 1; i < ABILITIES_COUNT; i++)
    {
        if (i != ABILITY_NONE)
            sAbilityPool[sAbilityPoolSize++] = i;
    }
}

static void InitMovePools(void)
{
    u32 i;
    sMovePoolSize = 0;
    for (i = 0; i < NUMBER_OF_MON_TYPES; i++)
        sMoveCountByType[i] = 0;

    for (i = 1; i < MOVES_COUNT_ALL; i++)
    {
        const struct MoveInfo *move = &gMovesInfo[i];
        if (i > 0 && i < MOVE_MAX_GUARD && (move->power > 0 || move->category == DAMAGE_CATEGORY_STATUS))
        {
            sMovePool[sMovePoolSize++] = i;
            if (move->type < NUMBER_OF_MON_TYPES && sMoveCountByType[move->type] < 200)
            {
                sMovePoolByType[move->type][sMoveCountByType[move->type]++] = i;
            }
        }
    }
}

void ShuffleRandomizerTable(u32 seed)
{
    u32 i, j, k;
    if (seed == 0) return;
    
    gRandomizerFinished = FALSE;
    SeedRandomizerRng(seed);
    
    // 1. Initialize table to identity
    for (i = 0; i < NUM_SPECIES; i++)
    {
        sRandomizerTable[i] = i;
    }
    
    // 2. Build a local copy of the sorted pool to shuffle
    static EWRAM_DATA u16 shufflePool[NUM_VALID_SPECIES];
    for (i = 0; i < NUM_VALID_SPECIES; i++)
    {
        shufflePool[i] = gSortedSpeciesPool[i];
    }

    // 3. Sliding Window Fisher-Yates (Perfect Shuffle within BST range)
    u32 blockSize = 60;
    for (i = 0; i < NUM_VALID_SPECIES; i += (blockSize / 2))
    {
        u32 end = i + blockSize;
        if (end > NUM_VALID_SPECIES) end = NUM_VALID_SPECIES;
        
        for (j = end - 1; j > i; j--)
        {
            k = i + (RandomizerNext() % (j - i + 1));
            u16 temp = shufflePool[j];
            shufflePool[j] = shufflePool[k];
            shufflePool[k] = temp;
        }
    }

    // 4. Commit the shuffled pool to the global mapping table
    for (i = 0; i < NUM_VALID_SPECIES; i++)
    {
        sRandomizerTable[gSortedSpeciesPool[i]] = shufflePool[i];
    }

    InitMovePools();
    InitAbilityPool();
    
    sRandomizerCheck = RANDOMIZER_MAGIC;
    gRandomizerTablePtr = sRandomizerTable;
    gRandomizerFinished = TRUE;

    // Update Telemetry
    gRandomizerTelemetry.seed = seed;
    gRandomizerTelemetry.mode = GetRandomizerMode();
    gRandomizerTelemetry.wildcardChance = RANDOMIZER_WILDCARD_CHANCE;
    gRandomizerTelemetry.itemRandoEnabled = RANDOMIZER_ITEM_FULL_RANDO;
    gRandomizerTelemetry.moveRandoEnabled = RANDOMIZER_MOVE_RANDO;
    gRandomizerTelemetry.abilityRandoEnabled = RANDOMIZER_ABILITY_RANDO;
    gRandomizerTelemetry.poolSize = NUM_VALID_SPECIES;
    gRandomizerTelemetry.speciesPool = gSortedSpeciesPool;
}

u8 GetRandomizerMode(void)
{
    // In the future, this could check a variable/flag. For now, use the config.
    return RANDOMIZER_SPECIES_MODE;
}

u16 RandomizeSpecies(u16 species, u16 mapSecId)
{
    return RandomizeSpeciesEx(species, mapSecId, FALSE);
}

u16 RandomizeSpeciesEx(u16 species, u16 mapSecId, bool8 isWild)
{
    u8 mode = GetRandomizerMode();

    // Safety: Ensure telemetry is synced if the seed is active but telemetry is blank
    if (gSaveBlock2Ptr->randomizerSeed != 0 && gRandomizerTelemetry.seed == 0)
    {
        gRandomizerTelemetry.seed = gSaveBlock2Ptr->randomizerSeed;
        gRandomizerTelemetry.mode = mode;
        gRandomizerTelemetry.wildcardChance = RANDOMIZER_WILDCARD_CHANCE;
        gRandomizerTelemetry.itemRandoEnabled = RANDOMIZER_ITEM_FULL_RANDO;
        gRandomizerTelemetry.moveRandoEnabled = RANDOMIZER_MOVE_RANDO;
        gRandomizerTelemetry.abilityRandoEnabled = RANDOMIZER_ABILITY_RANDO;
        gRandomizerTelemetry.poolSize = NUM_VALID_SPECIES;
        gRandomizerTelemetry.speciesPool = gSortedSpeciesPool;
    }

    if (gSaveBlock2Ptr->randomizerSeed == 0 || species == SPECIES_NONE || species >= NUM_SPECIES || mode == RANDOMIZER_MODE_OFF)
        return species;

    // Wildcard: Small chance to ignore the mode and go full chaos (applies to all modes)
    // For non-wild encounters (Starters/Gifts), we make the wildcard deterministic
    if (RANDOMIZER_WILDCARD_CHANCE > 0)
    {
        u32 roll;
        if (isWild)
            roll = Random() % 100;
        else
        {
            // Deterministic roll for Starters/Gifts
            u32 seed = gSaveBlock2Ptr->randomizerSeed + species + (mapSecId * 7);
            roll = (1103515245 * seed + 12345) >> 16;
            roll %= 100;
        }

        if (roll < RANDOMIZER_WILDCARD_CHANCE)
        {
            u16 randomized;
            if (isWild)
            {
                randomized = gSortedSpeciesPool[Random() % NUM_VALID_SPECIES];
                if (randomized == SPECIES_NONE || randomized >= NUM_SPECIES || gSpeciesInfo[randomized].speciesName[0] == '?')
                    return species;
            }
            else
            {
                u32 seed = gSaveBlock2Ptr->randomizerSeed + species + (mapSecId * 13);
                randomized = gSortedSpeciesPool[((1103515245 * seed + 12345) >> 16) % NUM_VALID_SPECIES];
            }
            return (randomized == SPECIES_NONE || randomized >= NUM_SPECIES) ? species : randomized;
        }
    }

    if (mode == RANDOMIZER_MODE_CHAOS)
    {
        if (isWild)
        {
            u16 randomized = gSortedSpeciesPool[Random() % NUM_VALID_SPECIES];
            if (randomized == SPECIES_NONE || randomized >= NUM_SPECIES || gSpeciesInfo[randomized].speciesName[0] == '?')
                return species;
            return randomized;
        }
        else
        {
            // Deterministic Chaos for Starters/Gifts
            u32 seed = gSaveBlock2Ptr->randomizerSeed + species + (mapSecId * 31);
            u16 randomized = gSortedSpeciesPool[((1103515245 * seed + 12345) >> 16) % NUM_VALID_SPECIES];
            if (randomized == SPECIES_NONE || randomized >= NUM_SPECIES || gSpeciesInfo[randomized].speciesName[0] == '?')
                return species;
            return randomized;
        }
    }

    if (mode == RANDOMIZER_MODE_CHAOS_SIMILAR)
    {
        s32 idx = -1;
        u32 i;

        // Find the index of the current species in the sorted pool
        for (i = 0; i < NUM_VALID_SPECIES; i++)
        {
            if (gSortedSpeciesPool[i] == species)
            {
                idx = i;
                break;
            }
        }

        if (idx != -1)
        {
            s32 offset;
            if (isWild)
                offset = (Random() % 51) - 25;
            else
            {
                // Deterministic neighbor selection
                u32 seed = gSaveBlock2Ptr->randomizerSeed + species + (mapSecId * 43);
                offset = (((1103515245 * seed + 12345) >> 16) % 51) - 25;
            }

            s32 newIdx = idx + offset;
            if (newIdx < 0) newIdx = 0;
            if (newIdx >= NUM_VALID_SPECIES) newIdx = NUM_VALID_SPECIES - 1;
            return gSortedSpeciesPool[newIdx];
        }
        return species;
    }

    if (mode == RANDOMIZER_MODE_AREA_STATIC)
    {
        s32 idx = -1;
        u32 i;

        // Find the index of the current species in the sorted pool
        for (i = 0; i < NUM_VALID_SPECIES; i++)
        {
            if (gSortedSpeciesPool[i] == species)
            {
                idx = i;
                break;
            }
        }

        if (idx != -1)
        {
            // Deterministic neighbor selection unique per area
            u32 seed = gSaveBlock2Ptr->randomizerSeed + species + (mapSecId * 1337);
            seed = 1103515245 * seed + 12345;
            s32 offset = ((seed >> 16) % 51) - 25; // +/- 25 slots
            
            s32 newIdx = idx + offset;
            if (newIdx < 0) newIdx = 0;
            if (newIdx >= NUM_VALID_SPECIES) newIdx = NUM_VALID_SPECIES - 1;
            
            u16 randomized = gSortedSpeciesPool[newIdx];
            if (randomized == SPECIES_NONE || randomized >= NUM_SPECIES || gSpeciesInfo[randomized].speciesName[0] == '?')
                return species;
            return randomized;
        }
        return species;
    }

    // Default to Static mode behavior (Global 1-1 swap) (Already deterministic)
    if (sRandomizerCheck != RANDOMIZER_MAGIC)
        ShuffleRandomizerTable(gSaveBlock2Ptr->randomizerSeed);

    u16 randomized = sRandomizerTable[species];
    if (randomized == SPECIES_NONE || randomized >= NUM_SPECIES || gSpeciesInfo[randomized].speciesName[0] == '?')
        return species;
    return randomized;
}

// Version used for Dex and UI - always uses STATIC mode so entries are consistent
u16 RandomizeSpeciesForEditor(u16 species)
{
    if (gSaveBlock2Ptr->randomizerSeed == 0 || species == SPECIES_NONE || species >= NUM_SPECIES || GetRandomizerMode() == RANDOMIZER_MODE_OFF)
        return species;

    if (sRandomizerCheck != RANDOMIZER_MAGIC)
        ShuffleRandomizerTable(gSaveBlock2Ptr->randomizerSeed);

    u16 randomized = sRandomizerTable[species];
    if (randomized == SPECIES_NONE || randomized >= NUM_SPECIES || gSpeciesInfo[randomized].speciesName[0] == '?')
        return species;
    return randomized;
}

u16 RandomizeMove(u16 move, u16 species, u8 slot)
{
    if (gSaveBlock2Ptr->randomizerSeed == 0 || move == MOVE_NONE || species >= NUM_SPECIES || !RANDOMIZER_MOVE_RANDO)
        return move;

    // Use a magic check to see if the table was wiped from memory
    if (sRandomizerCheck != RANDOMIZER_MAGIC)
        ShuffleRandomizerTable(gSaveBlock2Ptr->randomizerSeed);

    // Use a local RNG state for moves so it doesn't interfere with the table shuffle
    u32 localRng = gSaveBlock2Ptr->randomizerSeed + species + (slot * 73); 
    
    u8 t1 = gSpeciesInfo[species].types[0];
    u8 t2 = gSpeciesInfo[species].types[1];
    u8 targetType = TYPE_NONE;
    
    localRng = 1103515245 * localRng + 12345;
    u8 roll = (localRng >> 16) % 100;

    if (t2 == TYPE_NONE || t1 == t2)
    {
        if (roll < 40) targetType = t1;
    }
    else if (t1 == TYPE_NORMAL || t2 == TYPE_NORMAL)
    {
        u8 otherType = (t1 == TYPE_NORMAL) ? t2 : t1;
        if (roll < 10) targetType = TYPE_NORMAL;
        else if (roll < 40) targetType = otherType;
    }
    else
    {
        if (roll < 20) targetType = t1;
        else if (roll < 40) targetType = t2;
    }

    if (targetType != TYPE_NONE && sMoveCountByType[targetType] > 0)
    {
        localRng = 1103515245 * localRng + 12345;
        return sMovePoolByType[targetType][(localRng >> 16) % sMoveCountByType[targetType]];
    }

    localRng = 1103515245 * localRng + 12345;
    return sMovePool[(localRng >> 16) % sMovePoolSize];
}

u16 RandomizeAbility(u16 species, u8 slot)
{
    if (gSaveBlock2Ptr->randomizerSeed == 0 || species >= NUM_SPECIES || !RANDOMIZER_ABILITY_RANDO)
        return gSpeciesInfo[species].abilities[slot];

    u32 localRng = gSaveBlock2Ptr->randomizerSeed + species + (slot * 127);
    if (sAbilityPoolSize == 0) InitAbilityPool();

    localRng = 1103515245 * localRng + 12345;
    u16 ability = sAbilityPool[(localRng >> 16) % sAbilityPoolSize];
    if (ability == ABILITY_WONDER_GUARD && species != SPECIES_SHEDINJA)
    {
        localRng = 1103515245 * localRng + 12345;
        ability = sAbilityPool[(localRng >> 16) % sAbilityPoolSize];
    }

    return ability;
}

u16 RandomizeItem(u16 itemId)
{
    if (gSaveBlock2Ptr->randomizerSeed == 0 || itemId == ITEM_NONE || itemId >= ITEMS_COUNT)
        return itemId;

    u8 pocket = GetItemPocket(itemId);
    
    // Safety: Never randomize Key Items or HMs
    if (pocket == POCKET_KEY_ITEMS || (itemId >= ITEM_HM01 && itemId <= ITEM_HM08))
        return itemId;

    // Use a deterministic seed for this specific item location/type
    u32 localRng = gSaveBlock2Ptr->randomizerSeed + itemId + ITEM_BALL_RNG_MODIFIER;
    localRng = 1103515245 * localRng + 12345;
    u16 roll = (localRng >> 16);

    if (RANDOMIZER_ITEM_FULL_RANDO)
    {
        // Chaos Mode: Anything (safe) can be anything
        u16 result = 1 + (roll % (ITEMS_COUNT - 1));
        
        // Safety Loop: Re-roll if we hit a Key Item, HM, or invalid entry
        // We limit to 100 tries to prevent any theoretical infinite loops, though it should find one immediately
        u32 safety = 0;
        while (safety < 100 && (GetItemPocket(result) == POCKET_KEY_ITEMS || (result >= ITEM_HM01 && result <= ITEM_HM08) || result >= ITEMS_COUNT))
        {
            localRng = 1103515245 * localRng + 12345;
            roll = (localRng >> 16);
            result = 1 + (roll % (ITEMS_COUNT - 1));
            safety++;
        }
        return (safety < 100) ? result : itemId;
    }

    // Smart Mode: Randomize within categories
    if (pocket == POCKET_BERRIES)
    {
        // Randomize into another Berry
        return FIRST_BERRY_INDEX + (roll % (LAST_BERRY_INDEX - FIRST_BERRY_INDEX + 1));
    }
    else if (pocket == POCKET_TM_HM)
    {
        // Randomize into another TM (HMs are already excluded above)
        return ITEM_TM01 + (roll % (ITEM_TM54 - ITEM_TM01 + 1));
    }
    else
    {
        // Randomize into a general pool item (Medicine, Balls, Battle Items, etc.)
        // We pick a range that covers most "standard" items and exclude Key Items
        u16 result = 1 + (roll % (ITEM_ORANGE_MAIL - 1));
        
        // If we accidentally rolled a Key Item or invalid pocket, just try one more time with a simple shift
        if (GetItemPocket(result) == POCKET_KEY_ITEMS)
            result = 1 + ((roll + 0x77) % (ITEM_ORANGE_MAIL - 1));
            
        return result;
    }
}

// This function will be called from scripts via callnative
void RandomizeItemScriptHook(void)
{
    u16 itemId = VarGet(VAR_0x8000);
    VarSet(VAR_0x8000, RandomizeItem(itemId));
}

bool8 IsRandomizerEnabled(void)
{
    return gSaveBlock2Ptr->randomizerSeed != 0;
}

u32 GetRandomizerSeed(void)
{
    return gSaveBlock2Ptr->randomizerSeed;
}
