import { mkdir, writeFile } from 'node:fs/promises';
import * as path from 'node:path';
import { createRpcClient, type RpcClient } from './client.ts';

// Mirror of the C++ test.* command surface (see
// Lib/eacp/WebView/Test/TestHarness.cpp). Each method round-trips
// through one HTTP POST → bridge dispatch → evaluateJavaScript →
// window.__test.<op> call in the page.
//
// All methods reject the returned Promise on failure (selector not
// found, JS exception, RPC timeout). Use `exists` / `waitFor` to gate
// optional behaviour instead of catching.

export interface AppDriverOptions
{
    /** Default per-command timeout. C++ side defaults to 5000ms. */
    defaultTimeoutMs?: number;

    /**
     * Directory `snapshot()` writes `<name>.html` + `<name>.png` into.
     * Defaults to `<cwd>/test-results/snapshots`. Created on first
     * write. Names may contain path separators — sub-directories are
     * created as needed.
     */
    snapshotDir?: string;
}

const defaultSnapshotDir = 'test-results/snapshots';

export class AppDriver
{
    private readonly rpc: RpcClient;
    private readonly defaultTimeoutMs: number | undefined;
    private readonly snapshotDir: string;

    constructor(rpcUrl: string, options: AppDriverOptions = {})
    {
        this.rpc = createRpcClient(rpcUrl);
        this.defaultTimeoutMs = options.defaultTimeoutMs;
        this.snapshotDir = options.snapshotDir
            ?? path.resolve(process.cwd(), defaultSnapshotDir);
    }

    get rpcUrl(): string { return this.rpc.baseUrl; }

    /**
     * Calls the underlying RPC bridge directly. Useful for hitting
     * application commands (the ones the app registers itself), not
     * just test.* helpers.
     */
    invoke<T = unknown>(command: string, payload?: unknown): Promise<T>
    {
        return this.rpc.invoke<T>(command, payload);
    }

    click(selector: string, options?: CallOptions): Promise<boolean>
    {
        return this.run('test.click', { selector, ...this.timeout(options) });
    }

    fill(selector: string, value: string, options?: CallOptions): Promise<boolean>
    {
        return this.run('test.fill', { selector, value, ...this.timeout(options) });
    }

    press(selector: string, key: string, options?: CallOptions): Promise<boolean>
    {
        return this.run('test.press', { selector, key, ...this.timeout(options) });
    }

    submit(selector: string, options?: CallOptions): Promise<boolean>
    {
        return this.run('test.submit', { selector, ...this.timeout(options) });
    }

    text(selector: string, options?: CallOptions): Promise<string>
    {
        return this.run('test.text', { selector, ...this.timeout(options) });
    }

    attr(selector: string, name: string, options?: CallOptions): Promise<string | null>
    {
        return this.run('test.attr', { selector, name, ...this.timeout(options) });
    }

    exists(selector: string, options?: CallOptions): Promise<boolean>
    {
        return this.run('test.exists', { selector, ...this.timeout(options) });
    }

    count(selector: string, options?: CallOptions): Promise<number>
    {
        return this.run('test.count', { selector, ...this.timeout(options) });
    }

    waitFor(selector: string, options?: CallOptions): Promise<boolean>
    {
        return this.run('test.waitFor', { selector, ...this.timeout(options) });
    }

    /**
     * Evaluates an arbitrary JS expression in the page. The expression
     * is wrapped in a function body, so it can reference window /
     * document / any in-page globals. The return value must be JSON-
     * serializable.
     */
    evaluate<T = unknown>(expression: string, options?: CallOptions): Promise<T>
    {
        return this.run('test.evaluate', { expression, ...this.timeout(options) });
    }

    /**
     * Returns the serialised HTML for `selector` (its outerHTML), or
     * the full document if no selector is supplied. Useful for
     * inspecting / snapshotting page state after a step.
     */
    dom(selector?: string, options?: CallOptions): Promise<string>
    {
        const payload: { selector?: string; timeoutMs?: number } =
            { ...this.timeout(options) };
        if (selector !== undefined) payload.selector = selector;
        return this.run('test.dom', payload);
    }

    /**
     * Captures a PNG screenshot of the WebView's visible area. The
     * raw PNG bytes are always returned base64-encoded; if `path` is
     * supplied, the bytes are also decoded and written to that path
     * (parent directories are created as needed).
     */
    async screenshot(options?: ScreenshotOptions & CallOptions):
        Promise<ScreenshotResult>
    {
        const { pngBase64 } = await this.run<{ pngBase64: string }>(
            'test.screenshot', { ...this.timeout(options) });

        if (options?.path)
        {
            await mkdir(path.dirname(options.path), { recursive: true });
            await writeFile(options.path, Buffer.from(pngBase64, 'base64'));
            return { pngBase64, path: options.path };
        }
        return { pngBase64 };
    }

    /**
     * Captures both DOM HTML and a screenshot under a single label,
     * writing them to `<snapshotDir>/<name>.html` and `<name>.png`.
     * Returns the captured data plus the on-disk paths.
     */
    async snapshot(name: string,
                   options?: SnapshotOptions & CallOptions): Promise<SnapshotResult>
    {
        const baseName = sanitizeSnapshotName(name);
        const baseDir = options?.dir ?? this.snapshotDir;
        const htmlPath = path.join(baseDir, `${baseName}.html`);
        const pngPath = path.join(baseDir, `${baseName}.png`);

        const [dom, shot] = await Promise.all([
            this.dom(options?.selector, options),
            this.screenshot({ ...options, path: pngPath }),
        ]);

        await mkdir(path.dirname(htmlPath), { recursive: true });
        await writeFile(htmlPath, dom);

        return { name, dom, pngBase64: shot.pngBase64,
                 domPath: htmlPath, screenshotPath: pngPath };
    }

    /**
     * Runs `action`, then captures a `snapshot(name)` and returns the
     * action's result. Use this when you want a per-step audit trail
     * without weaving snapshot calls between each test action.
     */
    async withSnapshot<T>(name: string, action: () => Promise<T>,
                          options?: SnapshotOptions & CallOptions): Promise<T>
    {
        const result = await action();
        await this.snapshot(name, options);
        return result;
    }

    private run<T>(command: string, payload: object): Promise<T>
    {
        return this.rpc.invoke<T>(command, payload);
    }

    private timeout(options?: CallOptions): { timeoutMs?: number }
    {
        const value = options?.timeoutMs ?? this.defaultTimeoutMs;
        return value !== undefined ? { timeoutMs: value } : {};
    }
}

export interface CallOptions
{
    /** Override per-call. C++ side caps at this many ms. */
    timeoutMs?: number;
}

export interface ScreenshotOptions
{
    /** Decode + write the PNG to this path. Parent dirs are created. */
    path?: string;
}

export interface ScreenshotResult
{
    /** Always present — base64-encoded PNG straight from the app. */
    pngBase64: string;
    /** Set only when `options.path` was supplied to `screenshot()`. */
    path?: string;
}

export interface SnapshotOptions
{
    /** Override the AppDriver's configured snapshot directory. */
    dir?: string;
    /** DOM selector to scope the HTML snapshot to. */
    selector?: string;
}

export interface SnapshotResult
{
    name: string;
    dom: string;
    pngBase64: string;
    domPath: string;
    screenshotPath: string;
}

// Snapshot names go straight into file paths. Forward slashes pass
// through (sub-directories are intentional); anything else outside the
// portable set gets replaced so the result is safe on Windows too.
function sanitizeSnapshotName(name: string): string
{
    return name.replace(/[^A-Za-z0-9._/\-]/g, '_');
}
