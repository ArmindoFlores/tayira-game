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

def main(args):
    if args.name == None:
        args.name = os.path.basename(args.tileset).replace(" ", "_").lower()
        if "." in args.name:
            args.name = ".".join(args.name.split(".")[:-1])

    with open(args.tileset, "r") as f:
        tileset_contents = json.load(f)

    columns = tileset_contents["columns"]
    rows = tileset_contents["tilecount"] // tileset_contents["columns"]

    textures = {
        "columns": columns,
        "rows": rows,
        "texture_width": tileset_contents["tilewidth"],
        "texture_height": tileset_contents["tileheight"]
    }

    os.makedirs(args.output_directory, exist_ok=True)
    tileset_dir = os.path.dirname(args.tileset)
    tileset_image = os.path.join(tileset_dir, tileset_contents["image"])
    image_ext = ext(tileset_contents["image"])
    project_dir = os.path.dirname(os.path.dirname(__file__))
    relative_path = os.path.relpath(os.path.abspath(args.output_directory), project_dir)
    
    shutil.copyfile(tileset_image, os.path.join("..", relative_path, args.name + image_ext))
    
    with open(os.path.join("..", relative_path, args.name + ".asset-config.json"), "w") as f:
        json.dump({
            "filetype": image_ext,
            "regular_texture_info": textures
        }, f, indent=4)

    config_loc = args.config if args.config is not None else os.path.join("..", project_dir, "assets", "assets.json")
    with open(config_loc, "r") as f:
        assets_config_json = json.load(f)

    assets_config_json[args.name] = "/".join(os.path.join(relative_path, args.name).split("/")[1:])

    with open(config_loc, "w") as f:
        json.dump(assets_config_json, f, indent=4)

    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert tileset files from Tiled to game assets")
    
    parser.add_argument("tileset", help="the Tiled tileset file to convert")
    parser.add_argument("output_directory", help="path to a directory to place the converted files")
    parser.add_argument("--config", "-c", help="path to the base asset configuration file")
    parser.add_argument("--name", "-n", help="name of the tileset")
    
    args = parser.parse_args()    
    raise SystemExit(main(args))
