#!/usr/bin/python3
import argparse
import json
import os
import shlex
import shutil

import cv2

def format_name(name):
    return "font-" + name.lower().replace(" ", "-")

def ext(filename):
    parts = filename.split(".")
    if len(parts) == 1:
        return ""
    return "."+parts[-1]

def parse_attributes(line):
    result = {}
    tokens = shlex.split(line)
    for token in tokens:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        if value.isdigit():
            value = int(value)
        else:
            try:
                value = float(value)
            except ValueError:
                pass
        result[key] = value
    return result

def main(args):
    with open(args.file, "r") as f:
        info_txt, _, page_txt, _, *instructions = f.readlines()

    info = parse_attributes(info_txt)
    page = parse_attributes(page_txt)

    font_name = info["face"]
    font_img_file = page["file"]

    font_name = format_name(font_name)
    if args.font_name:
        font_name = args.font_name

    img_file_path = os.path.join(
        os.path.dirname(args.file),
        font_img_file
    )    

    img = cv2.imread(img_file_path, flags=cv2.IMREAD_UNCHANGED)
    b, g, r, a = cv2.split(img)
    mask = (a == 0)
    b[mask] = 0
    g[mask] = 0
    r[mask] = 0
    img = cv2.merge([b, g, r])

    font_chars = {}
    for instruction in instructions:
        _, *values, _, _, _, _, _ = instruction.split()
        for i in range(len(values)):
            values[i] = int(values[i].split("=")[-1])
        char, x, y, w, h = values
        
        if w <= 0 or h <= 0:
            continue

        font_chars[chr(char)] = {
            "width": w,
            "height": h,
            "offset_x": x,
            "offset_y": y,
        }

    font_img_file_ext = ext(font_img_file)
    project_dir = os.path.dirname(os.path.dirname(__file__))
    relative_path = os.path.relpath(os.path.abspath(args.output_directory), project_dir)
    no_asset_path = "/".join(relative_path.split("/")[1:])
    os.makedirs(args.output_directory, exist_ok=True)

    shutil.copyfile(img_file_path, os.path.join("..", relative_path, font_name + font_img_file_ext))

    with open(os.path.join(args.output_directory, font_name + ".asset-config.json"), "w") as f:
        json.dump({
            "filetype": font_img_file_ext,
            "textures": font_chars
        }, f, indent=4)
    
    font_config_filename = os.path.join(args.output_directory, font_name + ".font-config.json")
    if not os.path.isfile(font_config_filename):
        with open(font_config_filename, "w") as f:
            # TODO: fill this file automatically
            f.write("{}\n")

    config_loc = args.config if args.config is not None else os.path.join("..", project_dir, "assets", "assets.json")
    with open(config_loc, "r") as f:
        assets_config_json = json.load(f)
    
    assets_config_json[font_name] = f"{no_asset_path}/{font_name}"

    with open(config_loc, "w") as f:
        json.dump(assets_config_json, f, indent=4)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert font (.fnt) files to game assets")
    
    parser.add_argument("file", help="the .fnt file to convert")
    parser.add_argument("output_directory", help="path to a directory to place the converted files")
    parser.add_argument("--font-name", "-n", help="the name to give this font, ignoring its original name")
    parser.add_argument("--config", "-c", help="path to the base asset configuration file")
    
    args = parser.parse_args()    
    main(args)
