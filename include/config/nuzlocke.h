#ifndef GUARD_CONFIG_NUZLOCKE_H
#define GUARD_CONFIG_NUZLOCKE_H

// Nuzlocke Core Settings
#define NUZLOCKE_MODE_ENABLE             TRUE    // Master toggle for all Nuzlocke features

// Permadeath Settings
#define NUZLOCKE_PERMADEATH              TRUE    // Fainted Pokemon are considered dead
#define NUZLOCKE_GRAVEYARD_BOX_START     11      // First box for the graveyard
#define NUZLOCKE_GRAVEYARD_BOX_END       13      // Last box for the graveyard

#define NUZLOCKE_PREVENT_REVIVE          TRUE    // Prevent usage of Revives/Max Revives on dead Pokemon

// Encounter Settings
#define NUZLOCKE_FIRST_ENCOUNTER_ONLY    TRUE    // Only the first encounter on a route is catchable
#define NUZLOCKE_SHINY_CLAUSE            TRUE    // Shiny Pokemon ignore the first encounter rule
#define NUZLOCKE_DUPES_CLAUSE            TRUE    // If the first encounter is a species already caught, try again

// Enforcement Settings
#define NUZLOCKE_FORCE_NICKNAME          TRUE    // Cannot exit the nickname screen without entering a name
#define NUZLOCKE_PREVENT_RUNNING         FALSE   // Optional: Prevent running from wild battles (dangerous!)

// Convenience Settings
#define NUZLOCKE_STARTING_ITEMS          TRUE    // Give utility items by default
#define NUZLOCKE_STARTING_MONEY          TRUE    // Give max money by default

// Randomizer Settings
// -------------------
// RANDOMIZER_SPECIES_MODE:
//   RANDOMIZER_MODE_OFF:          Disable species randomization.
//   RANDOMIZER_MODE_STATIC:       Global 1-1 swap (Deterministic based on seed). Same species always swap to the same species.
//   RANDOMIZER_MODE_CHAOS:        True chaos. Every individual encounter is random.
//   RANDOMIZER_MODE_CHAOS_SIMILAR: Chaos but within a similar Base Stat Total (BST) range.
//   RANDOMIZER_MODE_AREA_STATIC:  1-1 swap unique per area, but stays within similar BST.
//                                 Pidgey on Route 101 might be Hoothoot, but on Route 102 it's Starly.

#define RANDOMIZER_SPECIES_MODE          RANDOMIZER_MODE_AREA_STATIC
#define RANDOMIZER_WILDCARD_CHANCE       5 // 1% chance for a 'True Chaos' encounter regardless of selected mode

// Randomization Toggles (Requires RANDOMIZER_SPECIES_MODE != OFF)
#define RANDOMIZER_MOVE_RANDO            TRUE   // Randomize learnsets and starting moves (Type-themed by default)
#define RANDOMIZER_ABILITY_RANDO         TRUE   // Randomize Pokémon abilities
#define RANDOMIZER_TRAINER_RANDO         TRUE   // Randomize trainer Pokémon species (Follows RANDOMIZER_SPECIES_MODE)

// Item Randomizer
#define RANDOMIZER_ITEM_FULL_RANDO       TRUE   // TRUE: Global Chaos (Anything safe can be anything). FALSE: Smart Rando (Berries to Berries, etc.)


// Memory Note: nuzlockeCaughtSpecies in SaveBlock1 can be removed in the future
// if space is needed. The list could be reconstructed on game load by 
// scanning PC boxes and the player party.

#endif // GUARD_CONFIG_NUZLOCKE_H
