#!/usr/bin/python3
import argparse
import json
import os
import shutil
import math


def load_config(files):
    try:
        config_file = next(filter(lambda f: f.endswith(".json"), files))
    except StopIteration:
        print("No config file found (must be a json file)")
        return None

    with open(os.path.join(args.asset_directory, config_file), "r") as f:
        return json.load(f)

def tileset_has_tile(tileset, tile_id):
    return tile_id >= tileset["firstgid"] and tile_id < tileset["firstgid"] + tileset["tilecount"]

def ext(filename):
    parts = filename.split(".")
    if len(parts) == 1:
        return ""
    return "."+parts[-1]

def convert(args):
    files = os.listdir(args.asset_directory)

    config_contents = load_config(files)
    if config_contents is None:
        return 1

    layers_to_convert = config_contents["layers"]
    if args.layers is not None:
        layer_names = args.layers.split(",")
        layers_to_convert[:] = filter(lambda tileset: tileset["name"] in layer_names, layers_to_convert)
    
    possible_ids = set()
    for layer in layers_to_convert:
        possible_ids.update(layer["data"])

    animations = {}
    for tileset in config_contents["tilesets"]:
        if "tiles" not in tileset: continue
        for tile in tileset["tiles"]:
            animations[tile["id"]] = tile

    anim_shape = tuple(map(int, args.shape.split(",")))
    animation_names = args.animation_names.split(",")

    gathered_assets = {}
    for layer in layers_to_convert:
        for gid in layer["data"]:
            for tileset in config_contents["tilesets"]:
                if not tileset_has_tile(tileset, gid): continue

                asset_name = tileset["name"].lower()
                if args.replace is not None:
                    old, new = args.replace.split(",")
                    asset_name = asset_name.replace(old, new)
                key = (asset_name, tileset["image"])
                gathered_assets.setdefault(key, {
                    "texture": {
                        "filetype": ext(tileset["image"]),
                        "textures": {}
                    },
                    "anim": {}
                })
                if "tiles" not in tileset: continue

                x_step = tileset["tilewidth"] + tileset["spacing"]
                y_step = tileset["tileheight"] + tileset["spacing"]

                animations = {}
                correspondance = {}
                for animation in tileset["tiles"]:
                    interval = None
                    for i, tile in enumerate(animation["animation"]):
                        tile_w = tile["tileid"] * x_step
                        tile_x = tile_w % tileset["imagewidth"]
                        tile_y = tile_w // tileset["imagewidth"] * y_step

                        x_norm = tile_x / x_step
                        y_norm = tile_y / y_step

                        x_index = math.floor(x_norm / anim_shape[0])
                        y_index = math.floor(y_norm / anim_shape[1])

                        if x_index == 0:
                            animations.setdefault(y_index, set())
                            correspondance.setdefault(y_index, {})

                            correspondance[y_index][f"animation{animation['id']}"] = (int(x_norm - x_index * anim_shape[0]), int(y_norm - y_index * anim_shape[1]))
                            animations[y_index].add(f"animation{animation['id']}")

                        gathered_assets[key]["texture"]["textures"][f"animation{animation['id']}-{i}"] = {
                            "width": tileset["tilewidth"],
                            "height": tileset["tileheight"],
                            "offset_x": tile_x,
                            "offset_y": tile_y,
                        }
                        interval = tile["duration"]

                gathered_assets[key]["anim"] = {
                    animation_names[i]: {
                        "interval": interval,
                        "shape": {
                            "width": anim_shape[0],
                            "height": anim_shape[1],
                        },
                        "textures": [{
                            "prefix": prefix,
                            "offset_x": correspondance[y_index][prefix][0],
                            "offset_y": correspondance[y_index][prefix][1],
                        } for prefix in animations[y_index]]
                    } for i, y_index in enumerate(animations.keys())
                }


    os.makedirs(args.output_directory, exist_ok=True)
    project_dir = os.path.dirname(os.path.dirname(__file__))
    relative_path = os.path.relpath(os.path.abspath(args.output_directory), project_dir)
    
    for (asset_name, asset_src), config in gathered_assets.items():
        texture_config = config["texture"]
        anim_config = config["anim"]
        shutil.copyfile(os.path.join(args.asset_directory, asset_src), os.path.join("..", relative_path, asset_name + ext(asset_src)))
        with open(os.path.join("..", relative_path, asset_name + ".asset-config.json"), "w") as f:
            json.dump(texture_config, f, indent=4)
        with open(os.path.join("..", relative_path, asset_name + ".anim-config.json"), "w") as f:
            json.dump(anim_config, f, indent=4)

    config_loc = args.config if args.config is not None else os.path.join("..", project_dir, "assets", "assets.json")
    with open(config_loc, "r") as f:
        assets_config_json = json.load(f)

    for asset_name, _ in gathered_assets.keys():
        assets_config_json[asset_name] = "/".join(os.path.join(relative_path, asset_name).split("/")[1:])

    with open(config_loc, "w") as f:
        json.dump(assets_config_json, f, indent=4)

    return 0

def info(args):
    files = os.listdir(args.asset_directory)

    config_contents = load_config(files)
    if config_contents is None:
        return 1
    
    print("Layers:")
    for layer in config_contents["layers"]:
        print(f"    -> {layer['name']}")

def main(args):
    if args.cmd == "convert":
        return convert(args)
    elif args.cmd == "info":
        return info(args)
    return 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert assets from Tiled to Tayira's own format")
        
    subparsers = parser.add_subparsers(required=True)
    help_parser = subparsers.add_parser("help", help="show this help message and exit")
    help_parser.set_defaults(cmd="help")

    convert_parser = subparsers.add_parser("convert", help="convert assets")
    convert_parser.set_defaults(cmd="convert")
    convert_parser.add_argument("asset_directory", help="the directory containing the asset files and descriptions")
    convert_parser.add_argument("output_directory", help="where to place the converted files")
    convert_parser.add_argument("--layers", "-l", help="a list of comma-separated layers to convert")
    convert_parser.add_argument("--config", "-c", help="path to the base asset configuration file")
    convert_parser.add_argument("--shape", "-s", help="shape of each animation group", default="4,4")
    convert_parser.add_argument("--animation-names", "-a", help="names of each animation group", default="down,left,right,up")
    convert_parser.add_argument("--replace", "-r", help="replace a string in the texture name with another")

    info_parser = subparsers.add_parser("info", help="show information about assets")
    info_parser.set_defaults(cmd="info")
    info_parser.add_argument("asset_directory", help="the directory containing the asset files and descriptions")
    
    args = parser.parse_args()
    if args.cmd == "help":
        parser.print_help()
        raise SystemExit(0)
    
    raise SystemExit(main(args))
