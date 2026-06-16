#!/usr/bin/env node

const fs = require("fs");
const os = require("os");
const path = require("path");
const childProcess = require("child_process");

const usage = `Usage: scripts/check-packaged-assets.js <plugin.vcvplugin|plugin-dir|artifact-dir> [...]`;

const repoRoot = path.resolve(__dirname, "..");
const sourceExtensions = new Set([
  ".c",
  ".cc",
  ".cpp",
  ".h",
  ".hpp",
  ".hh",
  ".json",
  ".md",
]);

function walk(dir, files = []) {
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    if (
      entry.name === ".git" ||
      entry.name === "build" ||
      entry.name === "dist" ||
      entry.name === "node_modules"
    ) {
      continue;
    }

    const fullPath = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      walk(fullPath, files);
    } else {
      files.push(fullPath);
    }
  }
  return files;
}

function sourceFiles() {
  return walk(repoRoot).filter((file) => {
    const rel = path.relative(repoRoot, file);
    if (rel.startsWith(`res${path.sep}`)) return false;
    if (rel.startsWith(`scripts${path.sep}gen${path.sep}`)) return false;
    if (path.basename(file) === "Makefile") return true;
    return sourceExtensions.has(path.extname(file));
  });
}

function referencedAssets() {
  const refs = new Map();
  const directPluginAsset =
    /asset::plugin\s*\([^;]*?["'`]((?:res\/)[^"'`)\s]+)["'`][^;]*?\)/gs;
  const resVariable =
    /(?:std::string|const\s+char\s*\*|char\s+const\s*\*)\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*["'`]((?:res\/)[^"'`)\s]+)["'`]/g;

  for (const file of sourceFiles()) {
    const relFile = path.relative(repoRoot, file);
    const contents = fs.readFileSync(file, "utf8");
    let match;

    while ((match = directPluginAsset.exec(contents))) {
      const asset = match[1];
      if (!refs.has(asset)) refs.set(asset, []);
      refs.get(asset).push(relFile);
    }

    while ((match = resVariable.exec(contents))) {
      const [, variable, asset] = match;
      const pluginVariable = new RegExp(
        `asset::plugin\\s*\\([^;]*\\b${variable}\\b[^;]*\\)`,
        "s",
      );
      if (!pluginVariable.test(contents)) continue;

      if (!refs.has(asset)) refs.set(asset, []);
      refs.get(asset).push(relFile);
    }
  }

  return refs;
}

function run(command, args, options = {}) {
  return childProcess.execFileSync(command, args, {
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
    ...options,
  });
}

function extractPackage(packagePath) {
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), "vcvplugin-assets-"));
  const tarPath = path.join(tmpDir, "plugin.tar");

  try {
    run("zstd", ["-d", packagePath, "-o", tarPath]);
    fs.mkdirSync(path.join(tmpDir, "unpacked"));
    run("tar", ["-xf", tarPath, "-C", path.join(tmpDir, "unpacked")]);
    return { root: path.join(tmpDir, "unpacked"), cleanup: () => rm(tmpDir) };
  } catch (error) {
    rm(tmpDir);
    throw error;
  }
}

function rm(target) {
  fs.rmSync(target, { recursive: true, force: true });
}

function findPackages(dir) {
  return walk(dir).filter((file) => file.endsWith(".vcvplugin"));
}

function findUnpackedPluginRoots(dir) {
  return walkDirs(dir).filter(
    (candidate) =>
      fs.existsSync(path.join(candidate, "plugin.json")) &&
      fs.existsSync(path.join(candidate, "res")),
  );
}

function walkDirs(dir, dirs = []) {
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    if (entry.name === ".git" || entry.name === "node_modules") continue;

    const fullPath = path.join(dir, entry.name);
    if (!entry.isDirectory()) continue;

    dirs.push(fullPath);
    walkDirs(fullPath, dirs);
  }
  return dirs;
}

function packageRoots(targetPath) {
  const stat = fs.statSync(targetPath);
  if (stat.isFile()) {
    if (!targetPath.endsWith(".vcvplugin")) {
      throw new Error(`Expected a .vcvplugin file: ${targetPath}`);
    }
    return [{ label: targetPath, ...extractPackage(targetPath) }];
  }

  if (!stat.isDirectory()) {
    throw new Error(`Expected a file or directory: ${targetPath}`);
  }

  if (
    fs.existsSync(path.join(targetPath, "plugin.json")) &&
    fs.existsSync(path.join(targetPath, "res"))
  ) {
    return [{ label: targetPath, root: targetPath, cleanup: () => {} }];
  }

  const packages = findPackages(targetPath);
  if (packages.length > 0) {
    return packages.map((packagePath) => ({
      label: packagePath,
      ...extractPackage(packagePath),
    }));
  }

  const unpackedRoots = findUnpackedPluginRoots(targetPath);
  if (unpackedRoots.length === 0) {
    throw new Error(
      `No .vcvplugin files or unpacked plugin root found in: ${targetPath}`,
    );
  }

  return unpackedRoots.map((root) => ({
    label: root,
    root,
    cleanup: () => {},
  }));
}

function packagedPaths(root) {
  const files = walk(root);
  const rootEntries = fs
    .readdirSync(root, { withFileTypes: true })
    .filter((entry) => entry.isDirectory());

  const pluginRoot =
    rootEntries.length === 1 &&
    fs.existsSync(path.join(root, rootEntries[0].name, "plugin.json"))
      ? path.join(root, rootEntries[0].name)
      : root;

  return new Set(
    files.map((file) => path.relative(pluginRoot, file).split(path.sep).join("/")),
  );
}

function validateOne(target, refs) {
  const files = packagedPaths(target.root);
  const missing = [];

  for (const [asset, locations] of refs) {
    if (!files.has(asset)) {
      missing.push({ asset, locations });
    }
  }

  if (missing.length === 0) {
    console.log(`OK: ${target.label}`);
    return 0;
  }

  console.error(`Missing packaged assets in ${target.label}:`);
  for (const miss of missing) {
    console.error(`  Missing packaged asset: ${miss.asset}`);
    console.error(`    referenced by: ${miss.locations.join(", ")}`);
  }
  return missing.length;
}

function main() {
  const args = process.argv.slice(2);
  if (args.length === 0) {
    console.error(usage);
    process.exit(2);
  }

  const refs = referencedAssets();
  if (refs.size === 0) {
    console.error("No quoted res/... references found.");
    process.exit(2);
  }

  let failures = 0;
  const roots = [];

  try {
    for (const arg of args) {
      roots.push(...packageRoots(path.resolve(arg)));
    }

    for (const root of roots) {
      failures += validateOne(root, refs);
    }
  } finally {
    for (const root of roots) {
      root.cleanup();
    }
  }

  if (failures > 0) process.exit(1);
}

main();
