# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import os
import json

# Cache for domain value to avoid re-reading config file
_cached_domain = None

def get_config_path():
    file_name = "realsense-config.json"
    if os.name == "nt":  # windows
        base_dir = os.environ.get("APPDATA")
    else:  # linux / macos / other unix-like
        file_name = "." + file_name # Hidden on unix like
        base_dir = os.environ.get("HOME")

    config_path = os.path.join(base_dir, file_name)
    return config_path


def get_config_file():
    config_path = get_config_path()

    try:
        with open(config_path, "r", encoding="utf-8") as f:
            config = json.load(f)
    except FileNotFoundError:
        raise FileNotFoundError(f"config file not found: {config_path}")
    except json.JSONDecodeError as e:
        raise ValueError(f"invalid json in {config_path}: {e}")
    
    return config
    
    
def get_domain_from_config_file_or_default():
    global _cached_domain
    
    # Return cached value if already read
    if _cached_domain is not None:
        return _cached_domain
    
    try:
        # Read from file and cache the result
        config_file = get_config_file()
        domain = config_file["context"]["dds"]["domain"]

        if domain is None:
            raise KeyError("Missing required config key: context.dds.domain")

        # Cache the domain value for future calls
        _cached_domain = domain
    
    except FileNotFoundError:
        # Fallback to default domain if config file is missing
        _cached_domain = 0
    
    finally:
        return _cached_domain
