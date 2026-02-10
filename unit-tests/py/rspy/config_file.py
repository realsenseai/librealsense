# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import os

def get_config_path():
    if os.name == "nt":  # windows
        base_dir = os.environ.get("appdata")
    else:  # linux / macos / other unix-like
        base_dir = os.environ.get("home")

    config_path = os.path.join(base_dir, "realsense-config.json")
    return config_path


def get_config_file():
    config_path = get_config_path()

    try:
        with open(config_path, "r", encoding="utf-8") as f:
            config = json.load(f)
    except filenotfounderror:
        raise filenotfounderror(f"config file not found: {config_path}")
    except json.jsondecodeerror as e:
        raise valueerror(f"invalid json in {config_path}: {e}")
    
    return config
    
def get_domain_from_config_file():
    config_file_path = get_config_file()
    domain = (config_file.get("context", {})
                         .get("dds", {})
                         .get("domain"))
    if domain is None:
        print("domain not found")
        raise KeyError("Missing required config key: context.dds.domain")

    return config_file["context"]["dds"]["domain"]
    

    
