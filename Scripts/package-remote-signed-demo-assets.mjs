#!/usr/bin/env node

import { join } from 'node:path';

import {
  cleanDir,
  env,
  log,
  repoRoot,
  requireMacOS,
  run,
  sha256File,
  writeJson,
} from './lib/cli.mjs';
import {
  ensureTamberSigningIdentity,
  signPath,
  verifyAppHubDeploymentTarget,
  verifyAppHubPrivilegedHelper,
  verifyCodeSignature,
  verifyMachODeploymentTargetAtMost,
} from './lib/macos-signing.mjs';
import {
  makeCatalog,
} from './lib/apphub-catalog.mjs';

const version = env('VERSION', '1.0.0');
const releaseTag = env('RELEASE_TAG', `remote-demo-v${version}`);
const releaseBaseUrl = env(
  'RELEASE_BASE_URL',
  `https://github.com/Tamber-Inc/eacp/releases/download/${releaseTag}`,
);
const outDir = env('OUT_DIR', join(repoRoot, 'dist', 'remote-signed-demo'));
const buildDir = env('BUILD_DIR', join(repoRoot, 'build-remote-signed-demo'));
const macOSDeploymentTarget = env('EACP_MACOS_DEPLOYMENT_TARGET', '11.0');

const appHubZip = 'AppHub-remote-demo.app.zip';
const demoZip = `TamberLocalUpdateDemo-${version}.app.zip`;
const demoAppName = 'Tamber Local Update Demo.app';
const demoBinaryName = 'Tamber Local Update Demo';
const productId = 'com.tamber.RealUpdateDemo';
const mazeZip = `Maze-${version}.app.zip`;
const teapotZip = `Teapot-${version}.app.zip`;
const runtimeBlob = `shared-onnxruntime-${version}.blob`;
const modelBlob = `shared-clap-${version}.blob`;

requireMacOS('Remote signed demo packaging');

log('Import Tamber Developer ID signing identity');
ensureTamberSigningIdentity();

log('Configure release build');
run('cmake', [
  '-S',
  repoRoot,
  '-B',
  buildDir,
  '-DCMAKE_BUILD_TYPE=Release',
  `-DCMAKE_OSX_DEPLOYMENT_TARGET=${macOSDeploymentTarget}`,
  `-DEACP_APPHUB_VERSION=${version}`,
  `-DEACP_REAL_UPDATE_DEMO_VERSION=${version}`,
]);

log('Build signed-demo targets');
run('cmake', [
  '--build',
  buildDir,
  '--target',
  'AppHub',
  'RealUpdateDemo',
  'Maze',
  'Teapot',
]);

const appHubApp = join(buildDir, 'Apps', 'System', 'AppHub', 'AppHub.app');
const appHubHelper = join(
  appHubApp,
  'Contents',
  'Library',
  'LaunchServices',
  'com.tamber.AppHub.PrivilegedHelper',
);
const demoApp = join(buildDir, 'Apps', 'System', 'RealUpdateDemo', demoAppName);
const mazeApp = join(buildDir, 'Apps', 'GPU', 'Maze', 'Maze.app');
const teapotApp = join(buildDir, 'Apps', 'GPU', 'Teapot', 'Teapot.app');

log('Sign AppHub helper and app');
signPath(appHubHelper);
signPath(appHubApp);
verifyAppHubDeploymentTarget(appHubApp, macOSDeploymentTarget);
verifyAppHubPrivilegedHelper(appHubApp);

log('Sign Demo App');
signPath(demoApp);
verifyMachODeploymentTargetAtMost(
  join(demoApp, 'Contents', 'MacOS', demoBinaryName),
  macOSDeploymentTarget,
);

log('Sign AppHub catalog apps');
signPath(mazeApp);
signPath(teapotApp);
verifyMachODeploymentTargetAtMost(
  join(mazeApp, 'Contents', 'MacOS', 'Maze'),
  macOSDeploymentTarget,
);
verifyMachODeploymentTargetAtMost(
  join(teapotApp, 'Contents', 'MacOS', 'Teapot'),
  macOSDeploymentTarget,
);

log('Verify Demo App version');
run(join(demoApp, 'Contents', 'MacOS', demoBinaryName), ['--version']);

log('Package release assets');
cleanDir(outDir);
run('ditto', ['-c', '-k', '--keepParent', appHubApp, join(outDir, appHubZip)]);
run('ditto', ['-c', '-k', '--keepParent', demoApp, join(outDir, demoZip)]);
run('ditto', ['-c', '-k', '--keepParent', mazeApp, join(outDir, mazeZip)]);
run('ditto', ['-c', '-k', '--keepParent', teapotApp, join(outDir, teapotZip)]);

writeJson(join(outDir, runtimeBlob), {
  name: 'ONNX Runtime placeholder',
  version,
  note: 'Demo shared runtime blob installed once by AppHub.',
});
writeJson(join(outDir, modelBlob), {
  name: 'CLAP Model placeholder',
  version,
  note: 'Demo shared model blob installed once and shared by Maze/Teapot.',
});

log('Verify packaged AppHub artifact');
const packagedVerifyDir = join(buildDir, 'packaged-apphub-verify');
cleanDir(packagedVerifyDir);
run('ditto', ['-x', '-k', join(outDir, appHubZip), packagedVerifyDir]);
const packagedAppHub = join(packagedVerifyDir, 'AppHub.app');
verifyCodeSignature(packagedAppHub);
verifyAppHubDeploymentTarget(packagedAppHub, macOSDeploymentTarget);
verifyAppHubPrivilegedHelper(packagedAppHub);

const demoSha = sha256File(join(outDir, demoZip));
const mazeSha = sha256File(join(outDir, mazeZip));
const teapotSha = sha256File(join(outDir, teapotZip));
const runtimeSha = sha256File(join(outDir, runtimeBlob));
const modelSha = sha256File(join(outDir, modelBlob));
const manifest = {
  productId,
  name: 'Tamber Local Update Demo',
  version,
  bundleName: demoAppName,
  artifact: {
    url: `${releaseBaseUrl}/${demoZip}`,
    sha256: demoSha,
  },
};
writeJson(join(outDir, 'manifest.json'), manifest);

const catalog = makeCatalog({
  version,
  releaseBaseUrl,
  runtimeBlob,
  runtimeSha,
  modelBlob,
  modelSha,
  mazeZip,
  mazeSha,
  teapotZip,
  teapotSha,
});
writeJson(join(outDir, 'apphub-catalog.json'), catalog);

log('Release assets');
run('ls', ['-lh', outDir]);
console.log(JSON.stringify(manifest, null, 2));
console.log(JSON.stringify(catalog, null, 2));
