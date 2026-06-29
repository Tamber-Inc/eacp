#!/usr/bin/env node

import { join } from 'node:path';

import {
  capture,
  cleanDir,
  env,
  log,
  readText,
  repoRoot,
  requireMacOS,
  run,
  sha256File,
  writeJson,
} from './lib/cli.mjs';
import {
  findCatalogAppBundle,
} from './lib/apphub-generated-catalog.mjs';
import {
  normalizeChannel,
  releaseTagForChannel,
  stableChannel,
  stableReleaseTag,
} from './lib/apphub-channel.mjs';
import {
  ensureTamberSigningIdentity,
  notarizeAndStapleApps,
  signPath,
  validateStapledApp,
  verifyCodeSignature,
  verifyGatekeeperApp,
  verifyMachODeploymentTargetAtMost,
} from './lib/macos-signing.mjs';

const version = env('VERSION', '2.0.0');
const channel = normalizeChannel(env('APPHUB_CHANNEL', env('CHANNEL', 'stable')));
const stableTag = env('RELEASE_TAG', stableReleaseTag);
const releaseTag = channel === stableChannel ? stableTag : releaseTagForChannel(channel);
const releaseBaseUrl = channel === stableChannel
  ? env(
    'RELEASE_BASE_URL',
    `https://github.com/Tamber-Inc/eacp/releases/download/${releaseTag}`,
  )
  : `https://github.com/Tamber-Inc/eacp/releases/download/${releaseTag}`;
const productId = envNonEmpty('APPHUB_CATALOG_PRODUCT_ID', 'com.eacp.maze');
const target = envNonEmpty('APPHUB_CATALOG_TARGET', 'Maze');
const outDir = env('OUT_DIR', join(repoRoot, `dist/generated-catalog-${target}-${version}`));
const buildDir = env('BUILD_DIR', join(repoRoot, `build-generated-catalog-${target}-${version}`));
const macOSDeploymentTarget = env('EACP_MACOS_DEPLOYMENT_TARGET', '11.0');
const releaseRepo = env('GITHUB_REPOSITORY', 'Tamber-Inc/eacp');

requireMacOS(`Generated catalog app update publishing for ${productId}`);

log('Download current AppHub catalog');
cleanDir(outDir);
const catalogPath = join(outDir, 'apphub-catalog.json');
if (!downloadCatalog(releaseTag) && (channel === 'stable' || !downloadCatalog(stableReleaseTag))) {
  throw new Error('Cannot publish an independent catalog app update before apphub-catalog.json exists');
}

const catalog = JSON.parse(readText(catalogPath));
const productIndex = catalog.products.findIndex((product) => product.id === productId);
if (productIndex < 0) {
  throw new Error(`Product ${productId} does not exist in ${releaseTag}/apphub-catalog.json`);
}
const currentProduct = catalog.products[productIndex];
if (currentProduct.kind !== 'App') {
  throw new Error(`Product ${productId} is ${currentProduct.kind}, not App`);
}

log('Import Tamber Developer ID signing identity');
ensureTamberSigningIdentity();

log(`Configure ${currentProduct.name} ${version}`);
run('cmake', [
  '-S',
  repoRoot,
  '-B',
  buildDir,
  '-DCMAKE_BUILD_TYPE=Release',
  `-DCMAKE_OSX_DEPLOYMENT_TARGET=${macOSDeploymentTarget}`,
  `-DEACP_CATALOG_DEFAULT_VERSION=${version}`,
  '-DEACP_APPHUB_DISABLE_DEV_CATALOG=ON',
  `-DEACP_APPHUB_CATALOG_URL=${releaseBaseUrl}/apphub-catalog.json`,
]);

log(`Build ${currentProduct.name} ${version}`);
run('cmake', ['--build', buildDir, '--target', target]);

const appBundle = findCatalogAppBundle(buildDir, currentProduct);
const binaryName = appExecutableName(appBundle);

log(`Sign ${currentProduct.name} ${version}`);
signPath(appBundle);
verifyCodeSignature(appBundle);
verifyMachODeploymentTargetAtMost(
  join(appBundle, 'Contents', 'MacOS', binaryName),
  macOSDeploymentTarget,
);

log(`Notarize and staple ${currentProduct.name} ${version}`);
notarizeAndStapleApps([appBundle]);

const zipName = `${productId}-${version}.app.zip`;
const zipPath = join(outDir, zipName);

log(`Package ${currentProduct.name} ${version}`);
run('ditto', ['-c', '-k', '--keepParent', appBundle, zipPath]);

log(`Verify packaged ${currentProduct.name} ${version}`);
const packagedVerifyDir = join(buildDir, 'packaged-app-verify');
cleanDir(packagedVerifyDir);
run('ditto', ['-x', '-k', zipPath, packagedVerifyDir]);
const packagedApp = join(packagedVerifyDir, currentProduct.bundleName);
verifyCodeSignature(packagedApp);
validateStapledApp(packagedApp);
verifyGatekeeperApp(packagedApp);

const productEntry = {
  ...currentProduct,
  channel,
  latestVersion: version,
  artifacts: [
    {
      platform: 'MacOS',
      architecture: 'Universal',
      url: `${releaseBaseUrl}/${zipName}`,
      sha256: sha256File(zipPath),
      signature: '',
    },
  ],
};

const nextProducts = [...catalog.products];
nextProducts[productIndex] = productEntry;
const nextCatalog = {
  ...catalog,
  catalogVersion: Math.max(
    Number(catalog.catalogVersion) || 0,
    Number.parseInt(version.split('.')[0], 10) || 1,
  ),
  products: nextProducts,
};
writeJson(catalogPath, nextCatalog);

log(`Upload ${currentProduct.name} ${version} and updated catalog`);
ensureRelease(releaseTag, `AppHub channel ${channel}`);
run('gh', [
  'release',
  'upload',
  releaseTag,
  zipPath,
  catalogPath,
  '--repo',
  releaseRepo,
  '--clobber',
]);

log(`Published ${currentProduct.name} ${version}`);
console.log(JSON.stringify(productEntry, null, 2));

function downloadCatalog(tag) {
  const result = capture('gh', [
    'release',
    'download',
    tag,
    '--repo',
    releaseRepo,
    '--pattern',
    'apphub-catalog.json',
    '--dir',
    outDir,
    '--clobber',
  ], { check: false });
  return result.status === 0;
}

function ensureRelease(tag, title) {
  const view = capture('gh', [
    'release',
    'view',
    tag,
    '--repo',
    releaseRepo,
  ], { check: false });
  if (view.status === 0) return;

  run('gh', [
    'release',
    'create',
    tag,
    '--repo',
    releaseRepo,
    '--title',
    title,
    '--notes',
    `Catalog channel '${channel}'.`,
  ]);
}

function envNonEmpty(name, fallback) {
  const value = env(name, fallback);
  return value && value.trim() ? value : fallback;
}

function appExecutableName(appBundle) {
  const result = capture('/usr/libexec/PlistBuddy', [
    '-c',
    'Print:CFBundleExecutable',
    join(appBundle, 'Contents', 'Info.plist'),
  ]);
  return result.stdout.trim();
}
