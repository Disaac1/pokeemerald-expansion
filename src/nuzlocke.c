#include "global.h"
#include "nuzlocke.h"
#include "config/nuzlocke.h"
#include "event_data.h"
#include "pokemon.h"
#include "battle.h"
#include "save.h"
#include "constants/region_map_sections.h"
#include "constants/species.h"
#include "constants/items.h"
#include "pokemon_storage_system.h"
#include "pokedex.h"
#include "sound.h"
#include "constants/songs.h"

#include "main.h"
#include "new_game.h"
#include "pokedex.h"
#include "battle.h"
#include "random.h"

#if NUZLOCKE_MODE_ENABLE

EWRAM_DATA bool8 gNuzlockeValidEncounterActive = FALSE;
EWRAM_DATA bool8 gNuzlockeDuplicateEncounterActive = FALSE;


static u16 GetRootSpecies(u16 species, u8 depth)
{
    u16 i, j;
    const struct Evolution *evolutions;

    if (depth > 10 || species >= NUM_SPECIES || species == SPECIES_NONE)
        return species;

    // Search for a pre-evolution
    for (i = 1; i < NUM_SPECIES; i++)
    {
        evolutions = gSpeciesInfo[i].evolutions;
        if (evolutions == NULL)
            continue;

        for (j = 0; evolutions[j].method != EVO_NONE; j++)
        {
            if (evolutions[j].targetSpecies == species)
            {
                if (i == species) return species; // Avoid direct self-loop
                return GetRootSpecies(i, depth + 1);
            }
        }
    }

    return species;
}

static bool8 IsAnyMemberOfFamilyCaught(u16 species, u8 depth)
{
    const struct Evolution *evolutions;
    u32 i;

    if (depth > 10 || species >= NUM_SPECIES || species == SPECIES_NONE)
        return FALSE;

    // Check Pokedex
    if (GetSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_GET_CAUGHT))
        return TRUE;

    // Check our custom early-game tracking
    for (i = 0; i < gSaveBlock1Ptr->nuzlockeNumCaught; i++)
    {
        if (gSaveBlock1Ptr->nuzlockeCaughtSpecies[i] == species)
            return TRUE;
    }

    evolutions = gSpeciesInfo[species].evolutions;
    if (evolutions != NULL)
    {
        for (i = 0; evolutions[i].method != EVO_NONE; i++)
        {
            if (IsAnyMemberOfFamilyCaught(evolutions[i].targetSpecies, depth + 1))
                return TRUE;
        }
    }

    return FALSE;
}

bool8 Nuzlocke_GetEncounterFlag(u16 mapSecId)
{
    if (mapSecId >= MAPSEC_COUNT)
        return FALSE;

    u8 byte = mapSecId / 8;
    u8 bit = mapSecId % 8;

    return (gSaveBlock2Ptr->nuzlockeData[byte] >> bit) & 1;
}

void Nuzlocke_SetEncounterFlag(u16 mapSecId)
{
    if (mapSecId >= MAPSEC_COUNT)
        return;

    u8 byte = mapSecId / 8;
    u8 bit = mapSecId % 8;

    gSaveBlock2Ptr->nuzlockeData[byte] |= (1 << bit);
}

void Nuzlocke_ClearEncounterFlag(u16 mapSecId)
{
    if (mapSecId >= MAPSEC_COUNT)
        return;

    u8 byte = mapSecId / 8;
    u8 bit = mapSecId % 8;

    gSaveBlock2Ptr->nuzlockeData[byte] &= ~(1 << bit);
}

bool8 Nuzlocke_IsSpeciesCaught(u16 species)
{
    if (species >= NUM_SPECIES || species == SPECIES_NONE)
        return FALSE;

    u16 root = GetRootSpecies(species, 0);
    return IsAnyMemberOfFamilyCaught(root, 0);
}

bool8 Nuzlocke_CanCatchEncounter(u16 species)
{
    u16 mapSecId = gMapHeader.regionMapSectionId;
    bool8 isShiny = IsMonShiny(GetBattlerMon(gBattlerTarget));

    if (!NUZLOCKE_MODE_ENABLE)
        return TRUE;

    if (FlagGet(FLAG_NUZLOCKE_RUN_LOST))
        return FALSE;

    if (NUZLOCKE_SHINY_CLAUSE && isShiny)
        return TRUE;

    if (!NUZLOCKE_FIRST_ENCOUNTER_ONLY)
        return TRUE;

    // Check if we already have an encounter flag for this route
    if (Nuzlocke_GetEncounterFlag(mapSecId))
        return FALSE;

    // Dupes Clause logic:
    // If it's a dupe, the player is ALLOWED to catch it, but if they skip it,
    // the route flag isn't set (handled in Init/End hooks).
    return TRUE;
}

void Nuzlocke_InitWildBattle(u16 species)
{
    u16 mapSecId = gMapHeader.regionMapSectionId;

    // Tutorial battles don't count
    if (gBattleTypeFlags & (BATTLE_TYPE_FIRST_BATTLE | BATTLE_TYPE_CATCH_TUTORIAL))
    {
        gNuzlockeValidEncounterActive = FALSE;
        gNuzlockeDuplicateEncounterActive = FALSE;
        return;
    }

    // If we already caught/failed something here, this isn't a valid "new" encounter
    if (Nuzlocke_GetEncounterFlag(mapSecId))
    {
        gNuzlockeValidEncounterActive = FALSE;
        gNuzlockeDuplicateEncounterActive = FALSE;
        return;
    }

    // Check Dupes Clause
    if (NUZLOCKE_DUPES_CLAUSE && Nuzlocke_IsSpeciesCaught(species))
    {
        // It's a dupe, so it doesn't count as the route encounter (can be skipped)
        gNuzlockeValidEncounterActive = FALSE;
        gNuzlockeDuplicateEncounterActive = TRUE;
        return;
    }

    // This is a valid first encounter!
    gNuzlockeValidEncounterActive = TRUE;
    gNuzlockeDuplicateEncounterActive = FALSE;
}


void Nuzlocke_EndWildBattle(u8 outcome)
{
    if (gNuzlockeValidEncounterActive)
    {
        if (outcome != B_OUTCOME_CAUGHT)
        {
            // Missed encounter! Add to graveyard as a memorial.
            struct Pokemon *mon = &gEnemyParty[0];
            u16 species = GetMonData(mon, MON_DATA_SPECIES);
            
            // Re-verify species in case it was cleared
            if (species == SPECIES_NONE && gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
                mon = &gEnemyParty[1], species = GetMonData(mon, MON_DATA_SPECIES);

            if (species != SPECIES_NONE && species != SPECIES_EGG)
            {
                u16 mapSecId = gMapHeader.regionMapSectionId;
                u32 trainerId = GetTrainerId(gSaveBlock2Ptr->playerTrainerId);
                
                // Give the memorial mon the player's info so it shows up correctly
                SetMonData(mon, MON_DATA_OT_NAME, gSaveBlock2Ptr->playerName);
                SetMonData(mon, MON_DATA_OT_GENDER, &gSaveBlock2Ptr->playerGender);
                SetMonData(mon, MON_DATA_OT_ID, &trainerId);
                SetMonData(mon, MON_DATA_MET_LOCATION, &mapSecId);
                
                Nuzlocke_MoveToGraveyard(mon);
            }
        }
        // Set the encounter flag regardless of outcome
        Nuzlocke_SetEncounterFlag(gMapHeader.regionMapSectionId);
    }
    gNuzlockeValidEncounterActive = FALSE;
    gNuzlockeDuplicateEncounterActive = FALSE;
}


bool32 IsItemInfinite(u16 itemId)
{
    return (itemId == ITEM_PORTA_HEAL ||
            itemId == ITEM_INFINITE_REPEL ||
            itemId == ITEM_INFINITE_RARE_CANDY ||
            itemId == ITEM_CAP_CANDY);
}

void Nuzlocke_OnCatch(struct Pokemon *mon)
{
    u16 mapSecId = gMapHeader.regionMapSectionId;
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    Nuzlocke_SetEncounterFlag(mapSecId);

    // Record the catch for the Dupes Clause
    if (species != SPECIES_NONE && species != SPECIES_EGG && gSaveBlock1Ptr->nuzlockeNumCaught < 100)
    {
        // Don't add if already tracked
        u32 i;
        bool8 alreadyTracked = FALSE;
        for (i = 0; i < gSaveBlock1Ptr->nuzlockeNumCaught; i++)
        {
            if (gSaveBlock1Ptr->nuzlockeCaughtSpecies[i] == species)
            {
                alreadyTracked = TRUE;
                break;
            }
        }

        if (!alreadyTracked)
        {
            gSaveBlock1Ptr->nuzlockeCaughtSpecies[gSaveBlock1Ptr->nuzlockeNumCaught++] = species;
        }
    }
}

void Nuzlocke_MoveToGraveyard(struct Pokemon *mon)
{
    s32 boxNo;
    s32 boxPos;

    // Search through all configured graveyard boxes
    for (boxNo = NUZLOCKE_GRAVEYARD_BOX_START; boxNo <= NUZLOCKE_GRAVEYARD_BOX_END; boxNo++)
    {
        for (boxPos = 0; boxPos < IN_BOX_COUNT; boxPos++)
        {
            struct BoxPokemon *checkingMon = GetBoxedMonPtr(boxNo, boxPos);
            // Check species directly to bypass checksum-guarded GetBoxMonData
            // Species is the first 2 bytes of the first encrypted substruct,
            // but in an empty mon, the whole thing is zeros.
            if (checkingMon->secure.substructs[0].type0.species == SPECIES_NONE)
            {
                // Extract original identity data
                u16 species = GetMonData(mon, MON_DATA_SPECIES);
                u8 level = GetMonData(mon, MON_DATA_LEVEL);
                u32 personality = GetMonData(mon, MON_DATA_PERSONALITY);
                u32 otId = GetMonData(mon, MON_DATA_OT_ID);
                struct OriginalTrainerId trainerId = {OT_ID_PRESET, otId};
                u8 buffer[32];

                // Create a valid, encrypted box mon shell with the same identity
                CreateBoxMon(checkingMon, species, level, personality, trainerId);
                
                // Copy over the personal memorial data using safe functions
                GetMonData(mon, MON_DATA_NICKNAME, buffer);
                SetBoxMonData(checkingMon, MON_DATA_NICKNAME, buffer);

                GetMonData(mon, MON_DATA_OT_NAME, buffer);
                SetBoxMonData(checkingMon, MON_DATA_OT_NAME, buffer);

                u32 metLoc = GetMonData(mon, MON_DATA_MET_LOCATION);
                SetBoxMonData(checkingMon, MON_DATA_MET_LOCATION, &metLoc);

                u32 metLevel = GetMonData(mon, MON_DATA_MET_LEVEL);
                SetBoxMonData(checkingMon, MON_DATA_MET_LEVEL, &metLevel);

                u32 metGame = GetMonData(mon, MON_DATA_MET_GAME);
                SetBoxMonData(checkingMon, MON_DATA_MET_GAME, &metGame);

                u32 otGender = GetMonData(mon, MON_DATA_OT_GENDER);
                SetBoxMonData(checkingMon, MON_DATA_OT_GENDER, &otGender);
                
                ZeroMonData(mon);
                return;
            }
        }
    }

    // If all graveyard boxes are full, we still need to clear the fainted mon
    ZeroMonData(mon);
}


void Nuzlocke_HandlePostBattleCleanup(void)
{
    u32 i;
    bool8 killedAny = FALSE;
    if (!NUZLOCKE_PERMADEATH || !FlagGet(FLAG_SYS_POKEDEX_GET))
        return;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gPlayerParty[i];
        if (GetMonData(mon, MON_DATA_SPECIES) != SPECIES_NONE 
            && !GetMonData(mon, MON_DATA_IS_EGG)
            && GetMonData(mon, MON_DATA_HP) == 0)
        {
            FlagSet(FLAG_TEMP_1); // Debug Flag: Hook is firing!
            Nuzlocke_MoveToGraveyard(mon);
            killedAny = TRUE;
        }
    }

    if (killedAny)
    {
        CompactPartySlots();
        CalculatePlayerPartyCount();
    }
}

void Nuzlocke_HandleWhiteOut(void)
{
    u32 i;
    bool8 killedAny = FALSE;
    static const u8 sMemorialName[] = _("MEMORIAL");

    if (!NUZLOCKE_PERMADEATH || !FlagGet(FLAG_SYS_POKEDEX_GET))
        return;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gPlayerParty[i];
        if (GetMonData(mon, MON_DATA_SPECIES) != SPECIES_NONE && !GetMonData(mon, MON_DATA_IS_EGG))
        {
            Nuzlocke_MoveToGraveyard(mon);
            killedAny = TRUE;
        }
    }

    if (killedAny)
    {
        CompactPartySlots();
        CalculatePlayerPartyCount();
    }

    if (gPlayerPartyCount == 0)
    {
        FlagSet(FLAG_NUZLOCKE_RUN_LOST);
        // Create the Memorial Magikarp
        CreateMon(&gPlayerParty[0], SPECIES_MAGIKARP, 5, Random32(), OTID_STRUCT_PLAYER_ID);
        SetMonData(&gPlayerParty[0], MON_DATA_NICKNAME, sMemorialName);
        CalculatePlayerPartyCount();
    }
}

#endif // NUZLOCKE_MODE_ENABLE
