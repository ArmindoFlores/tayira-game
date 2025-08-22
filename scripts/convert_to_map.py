#!/usr/bin/python3
import argparse
import json
import os
import shutil


def ext(filename):
    parts = filename.split(".")
    if len(parts) == 1:
        return ""
    return "."+parts[-1]

def get_layer_map(layer, binary):
    layer_map = []
    for y in range(layer["height"]):
        start = y * layer["width"]
        layer_map.append([t if t == 0 or not binary else 1 for t in layer["data"][start:start+layer["width"]]])
    return layer_map

def get_custom_property(obj, property_name):
    properties = obj.get("properties", [])
    for property_obj in properties:
        if property_obj["name"] == property_name:
            return property_obj["value"]
    return None

def tileset_has_tile(tileset, tile_id):
    return tile_id >= tileset["firstgid"] and tile_id < tileset["firstgid"] + tileset["tilecount"]

def main(args):
    if args.name == None:
        args.name = os.path.basename(args.map).replace(" ", "_").lower()
        if "." in args.name:
            args.name = ".".join(args.name.split(".")[:-1])

    with open(args.map, "r") as f:
        map_contents = json.load(f)

    player_layer = get_custom_property(map_contents, "player_layer")

    layers = {}
    assets = {}
    used_ids = set()
    for i, layer in enumerate(map_contents["layers"]):
        transparent = get_custom_property(layer, "transparent") is True
        is_collisions = get_custom_property(layer, "collisions") is True
        is_vision = get_custom_property(layer, "vision") is True
        is_binary = is_vision or is_collisions
        layer_map = get_layer_map(layer, is_binary)
        if not is_binary:
            for row in layer_map:
                used_ids.update(row)
        
        layer_info = {
            "map": layer_map,
            "layer": i,
            "collisions": is_collisions,
            "vision": is_vision,
            "entities": get_custom_property(layer, "objects") is True,
            "transparent": transparent
        }
        layers[layer["name"]] = layer_info

    for tileset in map_contents["tilesets"]:
        assets[tileset["name"]] = {
            "min_id": tileset["firstgid"],
            "max_id": tileset["firstgid"] + tileset["tilecount"],
        }

    os.makedirs(args.output_directory, exist_ok=True)
    project_dir = os.path.dirname(os.path.dirname(__file__))
    relative_path = os.path.relpath(os.path.abspath(args.output_directory), project_dir)

    map_info = {
        "width": map_contents["width"],
        "height": map_contents["height"],
        "tilewidth": map_contents["tilewidth"],
        "tileheight": map_contents["tileheight"],
        "layers": layers,
        "assets": assets,
    }
    if player_layer is not None:
        map_info["player_layer"] = player_layer
    
    with open(os.path.join("..", relative_path, args.name + ".map-config.json"), "w") as f:
        json.dump(map_info, f)

    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert map files from Tiled to game assets")
    
    parser.add_argument("map", help="the tiled map file to convert")
    parser.add_argument("output_directory", help="path to a directory to place the converted files")
    parser.add_argument("--config", "-c", help="path to the base asset configuration file")
    parser.add_argument("--name", "-n", help="name of the map")
    
    args = parser.parse_args()    
    raise SystemExit(main(args))
