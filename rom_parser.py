import json
import os
import struct

# Configuration: We need these addresses from symbols.json
# If symbols.json doesn't exist, we'll use fallbacks (user should run map_parser.py first)
SYMBOLS_PATH = "symbols.json"
ROM_PATH = "pokeemerald.gba"
OUTPUT_PATH = "area_data.json"

def read_u32(f, addr):
    f.seek(addr & 0x1FFFFFF)
    return struct.unpack("<I", f.read(4))[0]

def read_u16(f, addr):
    f.seek(addr & 0x1FFFFFF)
    return struct.unpack("<H", f.read(2))[0]

def read_u8(f, addr):
    f.seek(addr & 0x1FFFFFF)
    return struct.unpack("B", f.read(1))[0]

def read_string(f, addr):
    f.seek(addr & 0x1FFFFFF)
    s = ""
    # Simplified Gen 3 character decoding (A-Z, a-z, 0-9)
    while True:
        c = f.read(1)[0]
        if c == 0xFF: break
        if 0xBB <= c <= 0xD4: s += chr(c - 0xBB + ord('A'))
        elif 0xD5 <= c <= 0xEE: s += chr(c - 0xD5 + ord('a'))
        elif 0xA1 <= c <= 0xAA: s += chr(c - 0xA1 + ord('0'))
        elif c == 0x00: s += " "
        else: s += "?"
        if len(s) > 100: break
    return s

def parse_rom():
    if not os.path.exists(SYMBOLS_PATH):
        print("Error: symbols.json not found. Run map_parser.py first!")
        return

    with open(SYMBOLS_PATH, "r") as f:
        symbols = json.load(f)

    # We need a few more symbols for the names and groups
    # (I'll assume these were added to symbols.json or find them)
    G_MAP_GROUPS = symbols.get("gMapGroups", 0x08DF5318)
    G_REGION_MAP_ENTRIES = symbols.get("gRegionMapEntries", 0x08BFC908)
    G_WILD_MON_HEADERS = symbols.get("gWildMonHeaders", 0x08D2FD20)

    if not os.path.exists(ROM_PATH):
        print(f"Error: {ROM_PATH} not found.")
        return

    area_data = {}

    with open(ROM_PATH, "rb") as f:
        # 1. Map Names
        # In pokeemerald-expansion, gRegionMapEntries is an array of RegionMapEntry
        # struct RegionMapEntry { u8 x, y, width, height; const u8 *name; ... }
        # Name pointer is at offset 4
        print("Parsing Map Names...")
        map_names = {}
        for i in range(256): # Max 256 map sections
            entry_addr = G_REGION_MAP_ENTRIES + (i * 8)
            name_ptr = read_u32(f, entry_addr + 4)
            if name_ptr < 0x08000000 or name_ptr > 0x09FFFFFF: continue
            map_names[i] = read_string(f, name_ptr)

        # 2. Map Groups & Headers
        print("Parsing Map Groups...")
        # gMapGroups is a table of pointers to group tables
        for group_idx in range(40): # Typical max groups
            group_ptr = read_u32(f, G_MAP_GROUPS + (group_idx * 4))
            if group_ptr < 0x08000000 or group_ptr > 0x09FFFFFF: continue
            
            for map_idx in range(128): # Max maps per group
                header_ptr = read_u32(f, group_ptr + (map_idx * 4))
                if header_ptr < 0x08000000 or header_ptr > 0x09FFFFFF: break # End of group
                
                # struct MapHeader { ... u8 regionMapSectionId at offset 20 (0x14) ... }
                section_id = read_u8(f, header_ptr + 20)
                name = map_names.get(section_id, f"Unknown Area {section_id}")
                
                key = f"{group_idx}-{map_idx}"
                area_data[key] = {
                    "name": name,
                    "bank": group_idx,
                    "num": map_idx,
                    "encounters": []
                }

        # 3. Wild Encounters
        print("Parsing Wild Encounters...")
        # struct WildMonHeader { u8 mapGroup, mapNum; const struct WildMonInfo *landMons, *waterMons, ... }
        # Table ends with 0xFF mapGroup
        i = 0
        while True:
            header_addr = G_WILD_MON_HEADERS + (i * 20)
            group = read_u8(f, header_addr)
            if group == 0xFF: break
            num = read_u8(f, header_addr + 1)
            
            key = f"{group}-{num}"
            if key in area_data:
                # Land Mons Info pointer at +4
                land_info_ptr = read_u32(f, header_addr + 4)
                if land_info_ptr > 0:
                    # struct WildMonInfo { u8 ratio; const struct WildMon *mons; }
                    mons_ptr = read_u32(f, land_info_ptr + 4)
                    if mons_ptr > 0:
                        # struct WildMon { u8 minLevel, maxLevel; u16 species; }
                        for m in range(12): # Land table has 12 entries
                            species = read_u16(f, mons_ptr + (m * 4) + 2)
                            if species > 0:
                                area_data[key]["encounters"].append(species)
            i += 1

    with open(OUTPUT_PATH, "w") as f:
        json.dump(area_data, f, indent=4)
        print(f"Successfully wrote {OUTPUT_PATH}")

if __name__ == "__main__":
    parse_rom()
