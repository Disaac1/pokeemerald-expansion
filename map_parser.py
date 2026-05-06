import re
import json
import os

# Configuration: symbols we want to extract
SYMBOLS_TO_FIND = [
    "gBattleTypeFlags",
    "gBattlerPartyIndexes",
    "gEnemyParty",
    "gPlayerParty",
    "gLastUsedMove",
    "gCurrentMove",
    "gSaveBlock1Ptr",
    "gSaveBlock2Ptr"
]

def parse_map(map_file_path, output_json_path):
    symbols = {}
    
    if not os.path.exists(map_file_path):
        print(f"Error: {map_file_path} not found.")
        return

    print(f"Parsing {map_file_path}...")
    
    with open(map_file_path, "r") as f:
        content = f.read()
        
        for symbol in SYMBOLS_TO_FIND:
            # Pattern: 0x020000ac                gBattleTypeFlags
            pattern = rf"(0x[0-9a-fA-F]+)\s+{re.escape(symbol)}\b"
            match = re.search(pattern, content)
            if match:
                addr_str = match.group(1)
                symbols[symbol] = int(addr_str, 16)
                print(f"  Found {symbol}: {addr_str}")
            else:
                print(f"  Warning: {symbol} not found in map file.")

    with open(output_json_path, "w") as f:
        json.dump(symbols, f, indent=4)
        print(f"Successfully wrote {output_json_path}")

if __name__ == "__main__":
    # Default to current directory
    map_path = "pokeemerald.map"
    output_path = "symbols.json"
    parse_map(map_path, output_path)
