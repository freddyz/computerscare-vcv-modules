#!/usr/bin/env python3

import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path


USAGE = (
    "Usage: scripts/check-packaged-assets.py "
    "<plugin.vcvplugin|plugin-dir|artifact-dir> [...]"
)

REPO_ROOT = Path(__file__).resolve().parent.parent
SOURCE_EXTENSIONS = {
    ".c",
    ".cc",
    ".cpp",
    ".h",
    ".hpp",
    ".hh",
    ".json",
    ".md",
}


def walk_files(root):
    ignored = {".git", "build", "dist", "node_modules"}
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [name for name in dirnames if name not in ignored]
        for filename in filenames:
            yield Path(dirpath) / filename


def source_files():
    for file_path in walk_files(REPO_ROOT):
        rel = file_path.relative_to(REPO_ROOT)
        if rel.parts and rel.parts[0] == "res":
            continue
        if len(rel.parts) >= 2 and rel.parts[0] == "scripts" and rel.parts[1] == "gen":
            continue
        if file_path.name == "Makefile" or file_path.suffix in SOURCE_EXTENSIONS:
            yield file_path


def add_ref(refs, asset, rel_file):
    refs.setdefault(asset, [])
    if rel_file not in refs[asset]:
        refs[asset].append(rel_file)


def referenced_assets():
    refs = {}
    direct_plugin_asset = re.compile(
        r"asset::plugin\s*\([^;]*?[\"'`]((?:res/)[^\"'`)\s]+)[\"'`][^;]*?\)",
        re.S,
    )
    res_variable = re.compile(
        r"(?:std::string|const\s+char\s*\*|char\s+const\s*\*)\s+"
        r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*"
        r"[\"'`]((?:res/)[^\"'`)\s]+)[\"'`]"
    )

    for file_path in source_files():
        rel_file = file_path.relative_to(REPO_ROOT).as_posix()
        contents = file_path.read_text(encoding="utf-8")

        for match in direct_plugin_asset.finditer(contents):
            add_ref(refs, match.group(1), rel_file)

        for variable, asset in res_variable.findall(contents):
            plugin_variable = re.compile(
                rf"asset::plugin\s*\([^;]*\b{re.escape(variable)}\b[^;]*\)",
                re.S,
            )
            if plugin_variable.search(contents):
                add_ref(refs, asset, rel_file)

    return refs


def run(command):
    subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def extract_package(package_path):
    temp_dir = Path(tempfile.mkdtemp(prefix="vcvplugin-assets-"))
    tar_path = temp_dir / "plugin.tar"
    unpacked = temp_dir / "unpacked"

    try:
        run(["zstd", "-d", str(package_path), "-o", str(tar_path)])
        unpacked.mkdir()
        with tarfile.open(tar_path) as tar:
            tar.extractall(unpacked)
        return {
            "label": str(package_path),
            "root": unpacked,
            "cleanup": lambda: shutil.rmtree(temp_dir, ignore_errors=True),
        }
    except Exception:
        shutil.rmtree(temp_dir, ignore_errors=True)
        raise


def walk_dirs(root):
    for dirpath, dirnames, _filenames in os.walk(root):
        dirnames[:] = [name for name in dirnames if name not in {".git", "node_modules"}]
        for dirname in dirnames:
            yield Path(dirpath) / dirname


def find_packages(root):
    return [file_path for file_path in walk_files(root) if file_path.suffix == ".vcvplugin"]


def is_unpacked_plugin_root(root):
    return (root / "plugin.json").exists() and (root / "res").exists()


def find_unpacked_plugin_roots(root):
    return [candidate for candidate in walk_dirs(root) if is_unpacked_plugin_root(candidate)]


def package_roots(target_path):
    if target_path.is_file():
        if target_path.suffix != ".vcvplugin":
            raise ValueError(f"Expected a .vcvplugin file: {target_path}")
        return [extract_package(target_path)]

    if not target_path.is_dir():
        raise ValueError(f"Expected a file or directory: {target_path}")

    if is_unpacked_plugin_root(target_path):
        return [{"label": str(target_path), "root": target_path, "cleanup": lambda: None}]

    packages = find_packages(target_path)
    if packages:
        return [extract_package(package_path) for package_path in packages]

    unpacked_roots = find_unpacked_plugin_roots(target_path)
    if not unpacked_roots:
        raise ValueError(f"No .vcvplugin files or unpacked plugin root found in: {target_path}")

    return [
        {"label": str(root), "root": root, "cleanup": lambda: None}
        for root in unpacked_roots
    ]


def packaged_paths(root):
    root_entries = [entry for entry in root.iterdir() if entry.is_dir()]
    if len(root_entries) == 1 and (root_entries[0] / "plugin.json").exists():
        plugin_root = root_entries[0]
    else:
        plugin_root = root

    files = []
    for file_path in walk_files(plugin_root):
        try:
            files.append(file_path.relative_to(plugin_root).as_posix())
        except ValueError:
            continue

    return set(files)


def validate_one(target, refs):
    files = packaged_paths(target["root"])
    missing = []

    for asset, locations in refs.items():
        if asset not in files:
            missing.append((asset, locations))

    if not missing:
        print(f"OK: {target['label']}")
        return 0

    print(f"Missing packaged assets in {target['label']}:", file=sys.stderr)
    for asset, locations in missing:
        print(f"  Missing packaged asset: {asset}", file=sys.stderr)
        print(f"    referenced by: {', '.join(locations)}", file=sys.stderr)
    return len(missing)


def main():
    if len(sys.argv) < 2:
        print(USAGE, file=sys.stderr)
        return 2

    refs = referenced_assets()
    if not refs:
        print("No quoted res/... references found.", file=sys.stderr)
        return 2

    roots = []
    failures = 0
    try:
        for arg in sys.argv[1:]:
            roots.extend(package_roots(Path(arg).resolve()))

        for root in roots:
            failures += validate_one(root, refs)
    finally:
        for root in roots:
            root["cleanup"]()

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
