#!/usr/bin/env python3
"""
ZeroEmbedded Standalone Distribution Packager
Creates a standalone, zero-dependency C SDK distribution bundle and zip archive.
"""

import os
import sys
import shutil
import zipfile
import hashlib
import json

VERSION = "0.7.0"
DIST_NAME = f"ZeroEmbedded-v{VERSION}"

def sha256_file(filepath):
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def create_standalone_cmake(dest_dir):
    cmake_content = """cmake_minimum_required(VERSION 3.16)
project(ZeroEmbedded LANGUAGES C VERSION 0.7.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

file(GLOB_RECURSE ZERO_SOURCES "src/*.c")

add_library(zero_core_c STATIC ${ZERO_SOURCES})
add_library(ZeroEmbedded::core_c ALIAS zero_core_c)

target_include_directories(zero_core_c
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

include(GNUInstallDirs)
install(TARGETS zero_core_c EXPORT ZeroEmbeddedTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
"""
    with open(os.path.join(dest_dir, "CMakeLists.txt"), "w", encoding="utf-8") as f:
        f.write(cmake_content)

def create_dist_library_json(dest_dir):
    pio_config = {
        "name": "ZeroEmbedded",
        "version": VERSION,
        "description": "Deterministic, zero-heap embedded C framework for safety-critical microcontrollers.",
        "keywords": [
            "embedded",
            "zero-allocation",
            "deterministic",
            "stm32",
            "esp32",
            "arm",
            "freertos",
            "cmsis",
            "crypto",
            "dsp",
            "safety-critical"
        ],
        "authors": [
            {
                "name": "ZeroUniverse Team",
                "maintainer": True
            }
        ],
        "license": "MIT",
        "homepage": "https://github.com/kzxl/ZeroEmbedded",
        "repository": {
            "type": "git",
            "url": "https://github.com/kzxl/ZeroEmbedded.git"
        },
        "frameworks": ["*"],
        "platforms": ["*"],
        "build": {
            "includeDir": "include",
            "srcDir": "src"
        }
    }
    with open(os.path.join(dest_dir, "library.json"), "w", encoding="utf-8") as f:
        json.dump(pio_config, f, indent=2)

def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    dist_dir = os.path.join(repo_root, "dist")
    target_dir = os.path.join(dist_dir, DIST_NAME)
    zip_path = os.path.join(dist_dir, f"{DIST_NAME}.zip")

    print(f"[*] Packaging ZeroEmbedded v{VERSION}...")
    print(f"[*] Repository root: {repo_root}")
    print(f"[*] Target directory: {target_dir}")

    # Clean target
    if os.path.exists(target_dir):
        shutil.rmtree(target_dir)
    if os.path.exists(zip_path):
        os.remove(zip_path)

    os.makedirs(target_dir, exist_ok=True)

    # 1. Copy core-c include and src
    src_include = os.path.join(repo_root, "core-c", "include")
    src_code = os.path.join(repo_root, "core-c", "src")
    
    shutil.copytree(src_include, os.path.join(target_dir, "include"))
    shutil.copytree(src_code, os.path.join(target_dir, "src"))

    # 2. Standalone CMakeLists.txt & library.json
    create_standalone_cmake(target_dir)
    create_dist_library_json(target_dir)

    # 3. Copy pdsc, LICENSE, and README
    for filename in ["ZeroEmbedded.pdsc", "LICENSE", "README.md"]:
        src_file = os.path.join(repo_root, filename)
        if os.path.exists(src_file):
            shutil.copy2(src_file, os.path.join(target_dir, filename))

    # 4. Create ZIP archive
    print(f"[*] Compressing into {zip_path}...")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for root, dirs, files in os.walk(target_dir):
            for file in files:
                full_path = os.path.join(root, file)
                rel_path = os.path.relpath(full_path, dist_dir)
                zf.write(full_path, rel_path)

    file_size_kb = os.path.getsize(zip_path) / 1024.0
    sha256 = sha256_file(zip_path)

    print("\n[+] Packaging Complete!")
    print(f"    - Archive: {zip_path}")
    print(f"    - Size:    {file_size_kb:.2f} KB")
    print(f"    - SHA-256: {sha256}")

if __name__ == "__main__":
    main()
