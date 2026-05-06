import re
import json
import os
import struct

# Configuration
ROM_PATH = r"C:\Users\disaac1\Desktop\Code\PokemonEmeraldNuzRand\pokeemerald-expansion\pokeemerald.gba"
MAP_PATH = r"C:\Users\disaac1\Desktop\Code\PokemonEmeraldNuzRand\pokeemerald-expansion\pokeemerald.map"
# Path to your Tracker's data.json (where POIs are stored)
DATA_JSON_PATH = r"c:\Users\disaac1\Downloads\Pokemon\Helper\app\data.json"
# Path to your Tracker's Lua folder (where symbols.json/area_data.json go)
LUA_FOLDER_PATH = r"c:\Users\disaac1\Downloads\Pokemon\Helper\Lua"

# Symbols to find in the .map file
SYMBOLS_TO_FIND = [
    "gBattleTypeFlags", "gBattlerPartyIndexes", "gEnemyParty", "gPlayerParty",
    "gLastUsedMove", "gCurrentMove", "gSaveBlock1Ptr", "gSaveBlock2Ptr",
    "gMapGroups", "gWildMonHeaders", "gRegionMapEntries", "gMapHeader",
    "gMain", "CB2_Overworld", "CB2_LoadMap",
    "gNuzlockeValidEncounterActive", "gNuzlockeDuplicateEncounterActive",
    "gPokemonStoragePtr", "gRandomizerTablePtr", "gRandomizerFinished",
    "gRandomizerTelemetry", "gBattlerAttacker"
]

# Set this based on your ROM config (pokeemerald-expansion default is 4)
TOD_COUNT = 4 
HEADER_SIZE = 4 + (TOD_COUNT * 5 * 4) 

def read_u32(f, addr):
    f.seek(addr & 0x1FFFFFF)
    return struct.unpack("<I", f.read(4))[0]

def read_u8(f, addr):
    f.seek(addr & 0x1FFFFFF)
    return struct.unpack("B", f.read(1))[0]

def read_u16(f, addr):
    f.seek(addr & 0x1FFFFFF)
    return struct.unpack("<H", f.read(2))[0]

def read_string(f, addr):
    f.seek(addr & 0x1FFFFFF)
    s = ""
    while True:
        c = f.read(1)[0]
        if c == 0xFF: break
        if 0xBB <= c <= 0xD4: s += chr(c - 0xBB + ord('A'))
        elif 0xD5 <= c <= 0xEE: s += chr(c - 0xD5 + ord('a'))
        elif 0xA1 <= c <= 0xAA: s += chr(c - 0xA1 + ord('0'))
        elif c == 0x00: s += " "
        elif c == 0xAD: s += "."
        elif c == 0xAE: s += "-"
        else: s += "?"
        if len(s) > 100: break
    return s

def sync():
    symbols = {}

    if not os.path.exists(MAP_PATH):
        print(f"Error: {MAP_PATH} not found.")
        return

    print(f"--- 1. Parsing {MAP_PATH} ---")
    with open(MAP_PATH, "r") as f:
        content = f.read()
        for symbol in SYMBOLS_TO_FIND:
            match = re.search(rf"(0x[0-9a-fA-F]+)\s+{re.escape(symbol)}\b", content)
            if match:
                symbols[symbol] = int(match.group(1), 16)
                print(f"  {symbol}: {match.group(1)}")
    
    with open("symbols.json", "w") as f:
        json.dump(symbols, f, indent=4)

    if not os.path.exists(ROM_PATH):
        print(f"Warning: {ROM_PATH} not found. Skipping ROM parsing.")
        return

    print(f"--- 2. Extracting Area Data from {ROM_PATH} ---")
    area_data = {}
    with open(ROM_PATH, "rb") as f:
        # 1. Extract Names and World Map Coordinates
        names = {}
        g_names = symbols.get("gRegionMapEntries")
        if g_names:
            for i in range(256):
                addr = g_names + (i * 8)
                name_ptr = read_u32(f, addr + 4)
                if 0x08000000 <= name_ptr <= 0x09FFFFFF:
                    names[i] = {
                        "name": read_string(f, name_ptr),
                        "x": read_u8(f, addr),
                        "y": read_u8(f, addr + 1)
                    }

        # 2. Extract Map Groups and Sections
        g_groups = symbols.get("gMapGroups")
        if g_groups:
            for group_idx in range(40):
                group_ptr = read_u32(f, g_groups + (group_idx * 4))
                if not (0x08000000 <= group_ptr <= 0x09FFFFFF): continue
                for map_idx in range(128):
                    header_ptr = read_u32(f, group_ptr + (map_idx * 4))
                    if not (0x08000000 <= header_ptr <= 0x09FFFFFF): break
                    section_id = read_u8(f, header_ptr + 20)
                    entry = names.get(section_id, {"name": f"Area {section_id}", "x":0, "y":0})
                    area_data[f"{group_idx}-{map_idx}"] = {
                        "name": entry["name"],
                        "sectionId": section_id,
                        "romX": entry["x"],
                        "romY": entry["y"],
                        "encounters": []
                    }

        # 3. Extract Wild Encounters
        g_wild = symbols.get("gWildMonHeaders")
        if g_wild:
            i = 0
            while True:
                h = g_wild + (i * HEADER_SIZE)
                g, n = read_u8(f, h), read_u8(f, h + 1)
                if g == 0xFF and n == 0xFF: break
                key = f"{g}-{n}"
                if key in area_data:
                    # Just read the first time of day (Morning/Day) for simplicity
                    types_base = h + 4
                    for tod in range(1): # We only need one TOD to get the species list
                        tod_offset = tod * (5 * 4)
                        for type_idx, count in enumerate([12, 5, 5, 10, 3]): # Land, Water, Rock, Fish, Hidden
                            info_ptr = read_u32(f, types_base + tod_offset + (type_idx * 4))
                            if info_ptr > 0:
                                m_ptr = read_u32(f, info_ptr + 4)
                                if m_ptr > 0:
                                    for m in range(count):
                                        sp = read_u16(f, m_ptr + (m * 4) + 2)
                                        if 0 < sp < 0xFFFF and sp not in area_data[key]["encounters"]:
                                            area_data[key]["encounters"].append(sp)
                i += 1
                if i > 500: break # Safety

    with open("area_data.json", "w") as f:
        json.dump(area_data, f, indent=4)

    # 3. Auto-Binding & Cartography
    if os.path.exists(DATA_JSON_PATH):
        print(f"--- 3. Cartography & POI Sync ---")
        with open(DATA_JSON_PATH, "r") as f:
            data = json.load(f)
        
        routes = data.get("routes", [])
        bindings = data.get("mapIdToName", {})
        
        def rom_to_ui(rx, ry):
            # Scale ROM grid (~28x18) to UI % (0-100)
            return round((rx / 28) * 100, 2), round((ry / 18) * 100, 2)

        new_pois = 0
        bound_ids = 0

        for map_id, info in area_data.items():
            rom_name = info["name"]
            has_wild = len(info["encounters"]) > 0
            
            # Match existing POI
            found_poi = None
            for r in routes:
                if r["name"].lower() == rom_name.lower():
                    r["sectionId"] = info["sectionId"]
                    found_poi = r["name"]
                    break
            
            # If map has wild encounters but no POI, create it at center (50, 50)
            if has_wild and not found_poi:
                ux, uy = 50.0, 50.0 # User wants manual placement from center
                routes.append({"name": rom_name, "x": ux, "y": uy, "sectionId": info["sectionId"]})
                found_poi = rom_name
                print(f"  [POI] Created {rom_name} at Center (50%, 50%)")
                new_pois += 1
            
            # Bind the Map ID
            if found_poi:
                if map_id not in bindings:
                    bindings[map_id] = found_poi
                    bound_ids += 1
            else:
                # Interior Auto-Naming
                parent = rom_name
                # Some ROM names are generic, try to make them unique
                full_name = f"{parent} (Interior {map_id})"
                bindings[map_id] = {"name": full_name, "parent": parent}
                bound_ids += 1
        
        data["routes"] = routes
        data["mapIdToName"] = bindings
        with open(DATA_JSON_PATH, "w") as f:
            json.dump(data, f, indent=2)
        print(f"  Cartography Complete: {new_pois} new POIs, {bound_ids} maps bound.")

    # 4. Auto-Deploy to Lua Folder
    if os.path.exists(LUA_FOLDER_PATH):
        import shutil
        print(f"--- 4. Deploying to {LUA_FOLDER_PATH} ---")
        shutil.copy("symbols.json", os.path.join(LUA_FOLDER_PATH, "symbols.json"))
        shutil.copy("area_data.json", os.path.join(LUA_FOLDER_PATH, "area_data.json"))
        print("  Deployment successful!")

if __name__ == "__main__":
    sync()
