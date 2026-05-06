import json
import os

def get_wild_encounters_locations(repo_path):
    wild_json_path = os.path.join(repo_path, "src", "data", "wild_encounters.json")
    maps_dir = os.path.join(repo_path, "data", "maps")
    
    with open(wild_json_path, 'r') as f:
        wild_data = json.load(f)
    
    encountered_maps = set()
    for entry in wild_data.get("wild_encounter_groups", []):
        for enc in entry.get("encounters", []):
            encountered_maps.add(enc.get("map"))
            
    # Also check the individual map IDs if they are listed differently
    # In some versions it's wild_encounter_groups -> encounters -> map
    
    mapping = {}
    
    # Iterate through all directories in data/maps
    for map_name in os.listdir(maps_dir):
        map_json_path = os.path.join(maps_dir, map_name, "map.json")
        if os.path.exists(map_json_path):
            try:
                with open(map_json_path, 'r') as f:
                    map_data = json.load(f)
                    map_id = map_data.get("id")
                    map_sec = map_data.get("region_map_section")
                    if map_id in encountered_maps:
                        if map_sec not in mapping:
                            mapping[map_sec] = []
                        mapping[map_sec].append(map_id)
            except:
                pass
                
    return mapping

repo = "c:/Users/disaac1/Desktop/Code/PokemonEmeraldNuzRand/pokeemerald-expansion"
result = get_wild_encounters_locations(repo)

# Sort by MAPSEC name
sorted_keys = sorted(result.keys())

print("| Region Map Section (ID) | Maps Included |")
print("| --- | --- |")
for key in sorted_keys:
    maps = ", ".join(sorted(result[key]))
    print(f"| {key} | {maps} |")
