import { defineConfig } from '@playwright/test';

// Spawn mode (EACP_APP_BINARY set) gives each worker its own app
// instance — fully parallel is safe. Attach mode shares one running
// app across every test, so parallel workers race on shared state;
// force a single worker so mutating tests stay deterministic.
const isSpawnMode = !!process.env['EACP_APP_BINARY'];

export default defineConfig({
    testDir: '.',
    timeout: 30_000,
    fullyParallel: isSpawnMode,
    workers: isSpawnMode ? undefined : 1,
    forbidOnly: !!process.env['CI'],
    retries: process.env['CI'] ? 1 : 0,
    reporter: process.env['CI'] ? 'github' : 'list',
});
