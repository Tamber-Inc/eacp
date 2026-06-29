#!/usr/bin/env node

import { createHash } from 'node:crypto';
import {
  cpSync,
  createReadStream,
  mkdirSync,
  mkdtempSync,
  rmSync,
  writeFileSync,
} from 'node:fs';
import { basename, dirname, join } from 'node:path';
import { tmpdir } from 'node:os';
import { spawnSync } from 'node:child_process';

// When packaging a plain build directory (non-macOS), keep the runtime payload
// but drop the build-system droppings so the bundle is just the app.
function isBuildArtifact(path) {
  const name = basename(path);
  if (name === 'CMakeFiles') return true;
  return /\.(pdb|ilk|obj|exp|cmake|dd)$/i.test(name);
}

// The catalog the hub consumes locally must advertise an artifact for the
// platform it is generated on, so the build host's target is the default.
function hostTarget() {
  if (process.platform === 'darwin') {
    return { platform: 'MacOS', architecture: 'Universal' };
  }
  const architecture = process.arch === 'arm64' ? 'Arm64' : 'X64';
  if (process.platform === 'win32') {
    return { platform: 'Windows', architecture };
  }
  return { platform: 'Linux', architecture };
}

function parseArgs(argv) {
  const host = hostTarget();
  const args = {
    products: [],
    catalogVersion: '1',
    channel: 'stable',
    urlBase: '',
    platform: host.platform,
    architecture: host.architecture,
  };

  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    const value = argv[i + 1];
    if (arg === '--catalog') {
      args.catalog = value;
      i += 1;
    } else if (arg === '--artifact-dir') {
      args.artifactDir = value;
      i += 1;
    } else if (arg === '--catalog-version') {
      args.catalogVersion = value;
      i += 1;
    } else if (arg === '--channel') {
      args.channel = value;
      i += 1;
    } else if (arg === '--url-base') {
      args.urlBase = value.replace(/\/+$/, '');
      i += 1;
    } else if (arg === '--platform') {
      args.platform = value;
      i += 1;
    } else if (arg === '--arch') {
      args.architecture = value;
      i += 1;
    } else if (arg === '--product') {
      args.products.push(parseProduct(value));
      i += 1;
    } else {
      throw new Error(`Unknown argument: ${arg}`);
    }
  }

  if (!args.catalog) throw new Error('--catalog is required');
  if (!args.artifactDir) throw new Error('--artifact-dir is required');
  return args;
}

function parseProduct(raw) {
  const parts = raw.split('|');
  if (parts.length !== 8) {
    throw new Error(`Invalid product descriptor: ${raw}`);
  }

  const [target, id, name, bundleName, version, kind, deps, bundleDir] = parts;
  return {
    target,
    id,
    name,
    bundleName,
    version,
    kind,
    dependencies: deps ? deps.split(',').filter(Boolean) : [],
    bundleDir,
  };
}

function run(command, args) {
  const result = spawnSync(command, args, { stdio: 'inherit' });
  if (result.status !== 0) {
    throw new Error(`${command} ${args.join(' ')} failed`);
  }
}

function zipBundle(product, artifactDir) {
  const zipName = `${product.id}-${product.version}.app.zip`;
  const zipPath = join(artifactDir, zipName);

  if (process.platform === 'darwin') {
    // ditto --keepParent makes the .app the top-level entry of the zip.
    run('ditto', ['-c', '-k', '--keepParent', product.bundleDir, zipPath]);
    return { zipName, zipPath };
  }

  // Elsewhere the built app is a plain directory whose name differs from the
  // catalog bundle name, so stage it under the bundle name first, then archive
  // with bsdtar (tar.exe on Windows) so the zip's top entry matches what the
  // privileged helper expects to unpack.
  const staging = mkdtempSync(join(tmpdir(), 'eacp-bundle-'));
  try {
    const staged = join(staging, product.bundleName);
    cpSync(product.bundleDir, staged, {
      recursive: true,
      filter: (source) => !isBuildArtifact(source),
    });
    run('tar', ['-a', '-c', '-f', zipPath, '-C', staging, product.bundleName]);
  } finally {
    rmSync(staging, { recursive: true, force: true });
  }
  return { zipName, zipPath };
}

function sha256(path) {
  return new Promise((resolve, reject) => {
    const hash = createHash('sha256');
    const input = createReadStream(path);
    input.on('error', reject);
    input.on('data', (chunk) => hash.update(chunk));
    input.on('end', () => resolve(hash.digest('hex')));
  });
}

const args = parseArgs(process.argv.slice(2));
mkdirSync(args.artifactDir, { recursive: true });
mkdirSync(dirname(args.catalog), { recursive: true });

const products = [];
for (const product of args.products) {
  const { zipName, zipPath } = zipBundle(product, args.artifactDir);
  products.push({
    id: product.id,
    name: product.name,
    kind: product.kind,
    bundleName: product.bundleName,
    channel: args.channel,
    latestVersion: product.version,
    dependencies: product.dependencies,
    artifacts: [
      {
        platform: args.platform,
        architecture: args.architecture,
        url: args.urlBase ? `${args.urlBase}/${zipName}` : `file://${zipPath}`,
        sha256: await sha256(zipPath),
        signature: 'dev-signature-placeholder',
      },
    ],
  });
}

products.sort((left, right) => left.id.localeCompare(right.id));

writeFileSync(
  args.catalog,
  `${JSON.stringify({
    catalogVersion: Number.parseInt(args.catalogVersion, 10) || 1,
    products,
    signature: 'dev-catalog-signature-placeholder',
  }, null, 2)}\n`,
);
