#!/usr/bin/env node

import { createServer } from 'node:http';
import { createReadStream, mkdtempSync, rmSync, existsSync, accessSync, constants, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, normalize, resolve, sep } from 'node:path';
import { spawn } from 'node:child_process';

const root = resolve(process.argv[2] || 'build/dist/web');
const browserSetting = process.env.WEB_SMOKE_BROWSER || 'auto';
const browserArgs = (process.env.WEB_SMOKE_BROWSER_ARGS || '').split(/\s+/).filter(Boolean);
const timeoutMs = Number(process.env.WEB_SMOKE_TIMEOUT_MS || 30000);
const allowWebglDisabled = process.env.WEB_SMOKE_ALLOW_WEBGL_DISABLED
  ? !/^(0|false|no)$/i.test(process.env.WEB_SMOKE_ALLOW_WEBGL_DISABLED)
  : false;
const useNoSandbox = process.env.WEB_SMOKE_NO_SANDBOX
  ? !/^(0|false|no)$/i.test(process.env.WEB_SMOKE_NO_SANDBOX)
  : !!process.env.CI || (typeof process.getuid === 'function' && process.getuid() === 0);
const userDataDir = mkdtempSync(join(tmpdir(), 'inbe-web-smoke-'));
const mime = new Map([
  ['.html', 'text/html; charset=utf-8'],
  ['.js', 'text/javascript; charset=utf-8'],
  ['.wasm', 'application/wasm'],
  ['.json', 'application/json; charset=utf-8'],
  ['.png', 'image/png'],
  ['.jpg', 'image/jpeg'],
  ['.jpeg', 'image/jpeg'],
  ['.css', 'text/css; charset=utf-8'],
  ['.zip', 'application/zip'],
  ['.ogg', 'audio/ogg']
]);

if (!existsSync(join(root, 'index.html')) || !existsSync(join(root, 'index.js')) || !existsSync(join(root, 'index.wasm'))) {
  console.error(`web smoke: missing web build outputs in ${root}`);
  process.exit(1);
}

function canExecute(path) {
  try {
    accessSync(path, constants.X_OK);
    return true;
  } catch {
    return false;
  }
}

function findOnPath(command) {
  if (command.includes('/') || command.includes('\\'))
    return canExecute(command) ? command : '';

  for (const dir of (process.env.PATH || '').split(':')) {
    if (!dir) continue;
    const path = join(dir, command);
    if (canExecute(path))
      return path;
  }
  return '';
}

function browserCandidates() {
  const candidates = [];
  if (process.env.CHROME) candidates.push(process.env.CHROME);
  if (process.platform === 'freebsd') {
    candidates.push('firefox', 'librewolf');
  }
  candidates.push(
    '/usr/bin/chromium',
    '/usr/bin/chromium-browser',
    '/usr/bin/google-chrome',
    '/usr/bin/google-chrome-stable',
    '/snap/bin/chromium',
    '/opt/google/chrome/chrome',
    '/opt/chromium/chrome',
    'google-chrome',
    'google-chrome-stable',
    'chromium',
    'chromium-browser',
    'chrome',
    'firefox',
    'librewolf'
  );
  return [...new Set(candidates.filter(Boolean))];
}

function resolveBrowser() {
  if (browserSetting && browserSetting !== 'auto') {
    const resolved = findOnPath(browserSetting);
    if (resolved) return resolved;
    throw new Error(`browser not found: ${browserSetting}`);
  }

  const tried = browserCandidates();
  for (const candidate of tried) {
    const resolved = findOnPath(candidate);
    if (resolved) return resolved;
  }
  throw new Error(`no Chrome/Chromium browser found; tried ${tried.join(', ')}`);
}

function isFirefoxBrowser(browser) {
  return /(^|[/\\])(firefox|librewolf)(\.exe)?$/i.test(browser);
}

function contentType(path) {
  const dot = path.lastIndexOf('.');
  return dot >= 0 ? mime.get(path.slice(dot).toLowerCase()) || 'application/octet-stream' : 'application/octet-stream';
}

function isInside(base, path) {
  const rel = normalize(path).startsWith(base.endsWith(sep) ? base : base + sep);
  return path === base || rel;
}

const server = createServer((req, res) => {
  const url = new URL(req.url || '/', 'http://127.0.0.1');
  const leaf = url.pathname === '/' ? '/index.html' : decodeURIComponent(url.pathname);
  const path = resolve(root, '.' + leaf);

  if (!isInside(root, path) || !existsSync(path)) {
    res.writeHead(404, { 'content-type': 'text/plain; charset=utf-8' });
    res.end('not found');
    return;
  }

  res.writeHead(200, {
    'content-type': contentType(path),
    'cache-control': 'no-store'
  });
  createReadStream(path).pipe(res);
});

function listen() {
  return new Promise((resolveListen, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', () => resolveListen(server.address().port));
  });
}

function delay(ms) {
  return new Promise(resolveDelay => setTimeout(resolveDelay, ms));
}

function formatChromeLaunchError(stderrLines) {
  const stderr = stderrLines.join('').trim();
  const suffix = stderr ? `; chrome stderr: ${stderr.slice(-4000)}` : '';
  return new Error(`Chrome exited before DevTools was ready${suffix}`);
}

function readDevtoolsPort() {
  const path = join(userDataDir, 'DevToolsActivePort');

  if (!existsSync(path))
    return 0;

  const port = Number(readFileSync(path, 'utf8').split('\n', 1)[0]);
  return Number.isFinite(port) && port > 0 ? port : 0;
}

async function waitForDevtools(chrome, stderrLines) {
  const start = Date.now();
  let exited = false;
  const onExit = () => {
    exited = true;
  };
  chrome.once('exit', onExit);

  try {
    while (Date.now() - start < timeoutMs) {
      if (exited)
        throw formatChromeLaunchError(stderrLines);

      const port = readDevtoolsPort();
      if (port > 0) {
        try {
          const response = await fetch(`http://127.0.0.1:${port}/json/version`);
          if (response.ok)
            return port;
        } catch {}
      }

      await delay(50);
    }
  } finally {
    chrome.off('exit', onExit);
  }

  const stderr = stderrLines.join('').trim();
  const suffix = stderr ? `; chrome stderr: ${stderr.slice(-4000)}` : '';
  throw new Error(`timed out waiting for Chrome DevToolsActivePort${suffix}`);
}

async function jsonRequest(url, options = {}) {
  const response = await fetch(url, options);
  if (!response.ok)
    throw new Error(`${options.method || 'GET'} ${url} failed: ${response.status}`);
  return response.json();
}

function connect(wsUrl) {
  return new Promise((resolveConnect, reject) => {
    const ws = new WebSocket(wsUrl);
    const pending = new Map();
    const events = [];
    let nextId = 1;

    ws.addEventListener('open', () => {
      resolveConnect({
        events,
        send(method, params = {}) {
          const id = nextId++;
          ws.send(JSON.stringify({ id, method, params }));
          return new Promise((resolveSend, rejectSend) => {
            pending.set(id, { resolve: resolveSend, reject: rejectSend, method });
          });
        },
        close() {
          ws.close();
        }
      });
    });
    ws.addEventListener('message', event => {
      const message = JSON.parse(event.data);
      if (message.id && pending.has(message.id)) {
        const item = pending.get(message.id);
        pending.delete(message.id);
        if (message.error)
          item.reject(new Error(`${item.method}: ${message.error.message}`));
        else
          item.resolve(message.result || {});
      } else if (message.method) {
        events.push(message);
      }
    });
    ws.addEventListener('error', reject);
  });
}

function connectBidi(wsUrl) {
  return new Promise((resolveConnect, reject) => {
    const ws = new WebSocket(wsUrl);
    const pending = new Map();
    const events = [];
    let nextId = 1;

    ws.addEventListener('open', () => {
      resolveConnect({
        events,
        send(method, params = {}) {
          const id = nextId++;
          ws.send(JSON.stringify({ id, method, params }));
          return new Promise((resolveSend, rejectSend) => {
            pending.set(id, { resolve: resolveSend, reject: rejectSend, method });
          });
        },
        close() {
          ws.close();
        }
      });
    });
    ws.addEventListener('message', event => {
      const message = JSON.parse(event.data);
      if (message.id && pending.has(message.id)) {
        const item = pending.get(message.id);
        pending.delete(message.id);
        if (message.error)
          item.reject(new Error(`${item.method}: ${message.message || message.error || JSON.stringify(message)}`));
        else
          item.resolve(message.result || {});
      } else if (message.method) {
        events.push(message);
      }
    });
    ws.addEventListener('error', event => reject(new Error(event.message || `WebSocket error for ${wsUrl}`)));
  });
}

function eventText(event) {
  if (event.method === 'Runtime.consoleAPICalled') {
    return (event.params.args || []).map(arg => arg.value ?? arg.description ?? '').join(' ');
  }
  if (event.method === 'Log.entryAdded')
    return event.params.entry?.text || '';
  if (event.method === 'Runtime.exceptionThrown')
    return [
      event.params.exceptionDetails?.text || event.params.exceptionDetails?.exception?.description || 'exception thrown',
      ...((event.params.exceptionDetails?.stackTrace?.callFrames || []).map(frame => {
        const name = frame.functionName || '(anonymous)';
        return `${name}@${frame.url}:${frame.lineNumber + 1}:${frame.columnNumber + 1}`;
      }))
    ].join(' ');
  if (event.method === 'Page.javascriptDialogOpening')
    return event.params.message || 'javascript dialog opened';
  return '';
}

function fatalEvent(event) {
  const text = eventText(event);
  if (event.method === 'Runtime.exceptionThrown' && text.includes('transaction.oncomplete'))
    return '';
  if (event.method === 'Runtime.exceptionThrown' || event.method === 'Page.javascriptDialogOpening')
    return text;
  if (event.method === 'Runtime.consoleAPICalled' && event.params.type === 'error' &&
      /Aborted|RuntimeError|unreachable|exception thrown/i.test(text))
    return text;
  if (event.method === 'Log.entryAdded' && event.params.entry?.level === 'error' &&
      /Aborted|RuntimeError|unreachable/i.test(text))
    return text;
  return '';
}

function fatalBidiEvent(event) {
  if (event.method !== 'log.entryAdded')
    return '';

  const params = event.params || {};
  const text = params.text || params.args?.map(arg => arg.value ?? arg.text ?? '').join(' ') || '';
  if (text === 'unwind' || text === 'uncaught exception: unwind')
    return '';
  if (params.type === 'javascript' && params.level === 'error')
    return text || 'javascript error';
  if (params.level === 'error' && /Aborted|RuntimeError|unreachable|exception thrown/i.test(text))
    return text;
  return '';
}

async function waitForHealthyPage(client) {
  const start = Date.now();
  let sawRaylib = false;
  let lastState = null;
  let healthySince = 0;

  while (Date.now() - start < timeoutMs) {
    for (const event of client.events) {
      const fatal = fatalEvent(event);
      if (fatal) {
        const recent = client.events.slice(-16).map(eventText).filter(Boolean).join(' | ');
        throw new Error(`${fatal}; recent=${recent}`);
      }
      if (/PLATFORM: WEB: Initialized successfully|INBE: Global app pointer set/.test(eventText(event)))
        sawRaylib = true;
    }

    const result = await client.send('Runtime.evaluate', {
      expression: `(() => {
        const canvas = document.querySelector('canvas');
        const status = document.querySelector('#status')?.textContent || '';
        if (/WebGL is disabled/.test(status)) return { ok: ${allowWebglDisabled ? 'true' : 'false'}, disabledWebgl: true, status };
        if (!canvas) return { ok: false, reason: 'missing canvas', status };
        const gl = canvas.getContext('webgl') || canvas.getContext('experimental-webgl');
        if (!gl) return { ok: false, reason: 'missing webgl', status };
        const width = gl.drawingBufferWidth;
        const height = gl.drawingBufferHeight;
        const M = globalThis.Module;
        return {
          ok: width > 0 && height > 0,
          width,
          height,
          loadingClass: document.querySelector('#loading-screen')?.className || '',
          moduleLoaded: !!M,
          runtimeReady: !!(M && M.__inbeRuntimeReady),
          moduleStatus: (M && M.setStatus && M.setStatus.last && M.setStatus.last.text) || '',
          mainStarted: !!(typeof Module !== 'undefined' && Module._main) || /Global app pointer set/.test((M && M.__inbeLastLog) || ''),
          runDependencies: M && (typeof M.monitorRunDependencies === 'function') ? M.totalDependencies : undefined
        };
      })()`,
      returnByValue: true
    });
    const value = result.result?.value;
    lastState = value;
    if (sawRaylib && value?.ok) {
      if (!healthySince)
        healthySince = Date.now();
      if (Date.now() - healthySince >= 1500)
      return value;
    } else {
      healthySince = 0;
    }
    await new Promise(resolveDelay => setTimeout(resolveDelay, 250));
  }

  const recent = client.events.slice(-12).map(eventText).filter(Boolean).join(' | ');
  const allExceptions = client.events
    .filter(e => e.method === 'Runtime.exceptionThrown')
    .map(eventText)
    .filter(Boolean)
    .join(' | ');
  throw new Error(`web app did not become healthy within ${timeoutMs}ms; state=${JSON.stringify(lastState)}; recent=${recent || '(none)'}; exceptions=${allExceptions || '(none)'}`);
}

async function waitForHealthyBidiPage(client, context) {
  const start = Date.now();
  let lastState = null;
  let healthySince = 0;

  while (Date.now() - start < timeoutMs) {
    for (const event of client.events) {
      const fatal = fatalBidiEvent(event);
      if (fatal)
        throw new Error(fatal);
    }

    const result = await client.send('script.evaluate', {
      target: { context },
      awaitPromise: false,
      resultOwnership: 'none',
      expression: `JSON.stringify((() => {
        const canvas = document.querySelector('canvas');
        const status = document.querySelector('#status')?.textContent || '';
        if (/WebGL is disabled/.test(status)) return { ok: ${allowWebglDisabled ? 'true' : 'false'}, disabledWebgl: true, status };
        if (!canvas) return { ok: false, reason: 'missing canvas', status };
        const gl = canvas.getContext('webgl') || canvas.getContext('experimental-webgl');
        if (!gl) return { ok: false, reason: 'missing webgl', status };
        const width = gl.drawingBufferWidth;
        const height = gl.drawingBufferHeight;
        const boot = Array.from(document.querySelectorAll('script')).some(script => /index\\.js/.test(script.src || ''));
        const M = globalThis.Module;
        return {
          ok: width > 0 && height > 0 && boot && !!(M && M.__inbeRuntimeReady),
          width,
          height,
          boot,
          runtimeReady: !!(M && M.__inbeRuntimeReady),
          hasOnboardingHook: !!(M && M._app_web_test_onboarding_state)
        };
      })())`
    });
    try {
      lastState = JSON.parse(result.result?.value || '{}');
    } catch {
      lastState = null;
    }
    if (lastState?.ok) {
      if (!healthySince)
        healthySince = Date.now();
      if (Date.now() - healthySince >= 1500)
        return lastState;
    } else {
      healthySince = 0;
    }
    await delay(250);
  }

  throw new Error(`Firefox web app did not become healthy within ${timeoutMs}ms; state=${JSON.stringify(lastState)}`);
}

async function waitForFirefoxBidi(browser, stderrLines) {
  const start = Date.now();
  let exited = false;
  const onExit = () => {
    exited = true;
  };
  browser.once('exit', onExit);

  try {
    while (Date.now() - start < timeoutMs) {
      if (exited) {
        const stderr = stderrLines.join('').trim();
        throw new Error(`Firefox exited before BiDi was ready${stderr ? `; firefox stderr: ${stderr.slice(-4000)}` : ''}`);
      }

      const stderr = stderrLines.join('');
      const match = stderr.match(/WebDriver BiDi listening on (ws:\/\/[^\s]+)/);
      if (match)
        return match[1].replace(/\/?$/, '/session');

      await delay(50);
    }
  } finally {
    browser.off('exit', onExit);
  }

  const stderr = stderrLines.join('').trim();
  throw new Error(`timed out waiting for Firefox BiDi${stderr ? `; firefox stderr: ${stderr.slice(-4000)}` : ''}`);
}

function writeFirefoxPrefs() {
  writeFileSync(join(userDataDir, 'user.js'), [
    'user_pref("browser.shell.checkDefaultBrowser", false);',
    'user_pref("browser.tabs.warnOnClose", false);',
    'user_pref("browser.warnOnQuit", false);',
    'user_pref("datareporting.policy.dataSubmissionEnabled", false);',
    'user_pref("dom.webgpu.enabled", false);',
    'user_pref("gfx.webrender.software", true);',
    'user_pref("media.autoplay.default", 0);',
    'user_pref("privacy.fingerprintingProtection", false);',
    'user_pref("privacy.resistFingerprinting", false);',
    'user_pref("webgl.disabled", false);',
    'user_pref("webgl.disable-fail-if-major-performance-caveat", false);',
    'user_pref("webgl.enable-webgl2", true);',
    'user_pref("webgl.force-enabled", true);',
    ''
  ].join('\n'));
}

async function waitForStorageIdle(client) {
  const start = Date.now();
  let lastState = null;

  while (Date.now() - start < timeoutMs) {
    const result = await client.send('Runtime.evaluate', {
      expression: `(() => ({
        syncing: !!Module.__kryonStorageSyncing,
        pending: !!Module.__kryonStorageSyncPending,
        timer: !!Module.__kryonStorageSyncTimer
      }))()`,
      returnByValue: true
    });
    lastState = result.result?.value;
    if (lastState && !lastState.syncing && !lastState.pending && !lastState.timer)
      return;
    await new Promise(resolveDelay => setTimeout(resolveDelay, 50));
  }

  throw new Error(`IDBFS sync did not finish within ${timeoutMs}ms; state=${JSON.stringify(lastState)}`);
}

async function waitForStorageIdleBidi(client, context) {
  const start = Date.now();
  let lastState = null;

  while (Date.now() - start < timeoutMs) {
    const result = await client.send('script.evaluate', {
      target: { context },
      awaitPromise: false,
      resultOwnership: 'none',
      expression: "JSON.stringify((() => ({ syncing: !!Module.__kryonStorageSyncing, pending: !!Module.__kryonStorageSyncPending, timer: !!Module.__kryonStorageSyncTimer }))())"
    });
    try {
      lastState = JSON.parse(result.result?.value || '{}');
    } catch {
      lastState = null;
    }
    if (lastState && !lastState.syncing && !lastState.pending && !lastState.timer)
      return;
    await delay(50);
  }

  throw new Error('Firefox IDBFS sync did not finish within ' + timeoutMs + 'ms; state=' + JSON.stringify(lastState));
}

async function verifyReloadPersistence(client) {
  await waitForStorageIdle(client);
  const marker = `web-smoke-${Date.now()}`;
  const flush = await client.send('Runtime.evaluate', {
    expression: `(async () => {
      try { FS.mkdir('/home/inbe'); } catch (e) {}
      FS.writeFile('/home/inbe/web-smoke-persist.txt', ${JSON.stringify(marker)});
      if (typeof Module.__kryonFlushStorageSync !== 'function')
        throw new Error('missing immediate storage flush helper');
      return await Module.__kryonFlushStorageSync(true);
    })()`,
    awaitPromise: true,
    returnByValue: true
  });
  if (!flush.result?.value)
    throw new Error('IDBFS flush failed before reload persistence check');
  await waitForStorageIdle(client);
  await client.send('Page.reload', { ignoreCache: true });
  await waitForHealthyPage(client);

  const result = await client.send('Runtime.evaluate', {
    expression: `(() => {
      try {
        return FS.readFile('/home/inbe/web-smoke-persist.txt', { encoding: 'utf8' });
      } catch (e) {
        return '';
      }
    })()`,
    returnByValue: true
  });
  if (result.result?.value !== marker)
    throw new Error(`IDBFS reload persistence failed; got=${JSON.stringify(result.result?.value)}`);
}

async function verifyAppSettingsReloadPersistence(client) {
  await waitForStorageIdle(client);
  let result = await client.send('Runtime.evaluate', {
    expression: "(async () => { if (typeof Module._app_web_test_save_onboarding_state !== 'function') throw new Error('missing app settings save test hook'); Module._app_web_test_save_onboarding_state(); await Module.__kryonFlushStorageSync(true); return true; })()",
    awaitPromise: true,
    returnByValue: true
  });
  if (!result.result?.value)
    throw new Error('failed to invoke app settings save test hook');
  await waitForStorageIdle(client);
  await client.send('Page.reload', { ignoreCache: true });
  await waitForHealthyPage(client);

  result = await client.send('Runtime.evaluate', {
    expression: "(() => typeof Module._app_web_test_onboarding_state === 'function' && Module._app_web_test_onboarding_state() === 1)()",
    returnByValue: true
  });
  if (!result.result?.value)
    throw new Error('app settings did not persist across reload');
}

async function verifySyncKeyImport(client) {
  await waitForStorageIdle(client);
  const result = await client.send('Runtime.evaluate', {
    expression: "(() => typeof Module._app_web_test_import_sync_key === 'function' && Module._app_web_test_import_sync_key() === 1)()",
    returnByValue: true
  });
  if (!result.result?.value)
    throw new Error('web sync key import hook failed');
  const flush = await client.send('Runtime.evaluate', {
    expression: "(async () => await Module.__kryonFlushStorageSync(true))()",
    awaitPromise: true,
    returnByValue: true
  });
  if (!flush.result?.value)
    throw new Error('IDBFS flush failed after web sync key import hook');
  await waitForStorageIdle(client);
}

async function verifyAppSettingsReloadPersistenceBidi(client, context) {
  await waitForStorageIdleBidi(client, context);
  let result = await client.send('script.evaluate', {
    target: { context },
    awaitPromise: true,
    resultOwnership: 'none',
    expression: "(async () => JSON.stringify(await (async () => { if (typeof Module._app_web_test_save_onboarding_state !== 'function') return { ok: false, reason: 'missing app settings save test hook' }; Module._app_web_test_save_onboarding_state(); await Module.__kryonFlushStorageSync(true); return { ok: Module._app_web_test_onboarding_state && Module._app_web_test_onboarding_state() === 1 }; })()))()"
  });
  const state = JSON.parse(result.result?.value || '{}');
  if (!state.ok)
    throw new Error(state.reason || 'failed to invoke Firefox app settings save test hook or immediate readback failed');
  await waitForStorageIdleBidi(client, context);
  await client.send('browsingContext.reload', { context, wait: 'complete' });
  await waitForHealthyBidiPage(client, context);

  result = await client.send('script.evaluate', {
    target: { context },
    awaitPromise: false,
    resultOwnership: 'none',
    expression: "JSON.stringify((() => ({ ok: typeof Module._app_web_test_onboarding_state === 'function' && Module._app_web_test_onboarding_state() === 1 }))())"
  });
  state = JSON.parse(result.result?.value || '{}');
  if (!state.ok)
    throw new Error('Firefox app settings did not persist across reload');
}

async function verifyAppSettingsImmediateBidi(client, context) {
  await waitForStorageIdleBidi(client, context);
  const result = await client.send('script.evaluate', {
    target: { context },
    awaitPromise: true,
    resultOwnership: 'none',
    expression: "(async () => JSON.stringify(await (async () => { if (typeof Module._app_web_test_save_onboarding_state !== 'function') return { ok: false, reason: 'missing app settings save test hook' }; Module._app_web_test_save_onboarding_state(); await Module.__kryonFlushStorageSync(true); return { ok: Module._app_web_test_onboarding_state && Module._app_web_test_onboarding_state() === 1 }; })()))()"
  });
  let state = JSON.parse(result.result?.value || '{}');
  if (!state.ok)
    throw new Error(state.reason || 'Firefox app settings immediate save/readback failed');
  await waitForStorageIdleBidi(client, context);
}

async function verifySyncKeyImportBidi(client, context) {
  await waitForStorageIdleBidi(client, context);
  const result = await client.send('script.evaluate', {
    target: { context },
    awaitPromise: false,
    resultOwnership: 'none',
    expression: "JSON.stringify((() => ({ ok: typeof Module._app_web_test_import_sync_key === 'function' && Module._app_web_test_import_sync_key() === 1 }))())"
  });
  let state = JSON.parse(result.result?.value || '{}');
  if (!state.ok)
    throw new Error('Firefox web sync key import hook failed');
  const flush = await client.send('script.evaluate', {
    target: { context },
    awaitPromise: true,
    resultOwnership: 'none',
    expression: "(async () => JSON.stringify({ ok: await Module.__kryonFlushStorageSync(true) }))()"
  });
  state = JSON.parse(flush.result?.value || '{}');
  if (!state.ok)
    throw new Error('Firefox IDBFS flush failed after web sync key import hook');
  await waitForStorageIdleBidi(client, context);
}

let chrome;
let client;
let failure;

try {
  const port = await listen();
  const browser = resolveBrowser();
  if (isFirefoxBrowser(browser)) {
    writeFirefoxPrefs();
    const args = [
      '--headless',
      '--new-instance',
      '--profile',
      userDataDir,
      '--remote-debugging-port',
      '0',
      '--remote-allow-hosts',
      '127.0.0.1',
      '--remote-allow-origins',
      '*',
      ...browserArgs,
      'about:blank'
    ];
    const firefoxStderr = [];
    chrome = spawn(browser, args, { stdio: ['ignore', 'ignore', 'pipe'], detached: true });
    chrome.stderr.setEncoding('utf8');
    chrome.stderr.on('data', chunk => {
      firefoxStderr.push(chunk);
      while (firefoxStderr.join('').length > 8000)
        firefoxStderr.shift();
    });
    chrome.once('error', error => {
      if (!failure)
        failure = error;
    });

    const bidiUrl = await waitForFirefoxBidi(chrome, firefoxStderr);
    client = await connectBidi(bidiUrl);
    await client.send('session.new', { capabilities: {} });
    await client.send('session.subscribe', { events: ['log.entryAdded'] });
    const created = await client.send('browsingContext.create', { type: 'tab' });
    const context = created.context;
    await client.send('browsingContext.navigate', {
      context,
      url: `http://127.0.0.1:${port}/index.html`,
      wait: 'complete'
    });
    await waitForHealthyBidiPage(client, context);
    await verifyAppSettingsImmediateBidi(client, context);
    await verifySyncKeyImportBidi(client, context);
  } else {
    const args = [
      '--headless=new',
      '--remote-debugging-port=0',
      '--remote-debugging-address=127.0.0.1',
      `--user-data-dir=${userDataDir}`,
      '--no-first-run',
      '--no-default-browser-check',
      '--disable-background-networking',
      '--disable-dev-shm-usage',
      '--enable-unsafe-swiftshader',
      '--ignore-gpu-blocklist',
      '--enable-webgl',
      '--use-gl=angle',
      '--use-angle=swiftshader',
      ...browserArgs,
      'about:blank'
    ];
    if (useNoSandbox)
      args.splice(1, 0, '--no-sandbox');

    const chromeStderr = [];
    chrome = spawn(browser, args, { stdio: ['ignore', 'ignore', 'pipe'], detached: true });
    chrome.stderr.setEncoding('utf8');
    chrome.stderr.on('data', chunk => {
      chromeStderr.push(chunk);
      while (chromeStderr.join('').length > 8000)
        chromeStderr.shift();
    });
    chrome.once('error', error => {
      if (!failure)
        failure = error;
    });

    const devtoolsPort = await waitForDevtools(chrome, chromeStderr);
    const pages = await jsonRequest(`http://127.0.0.1:${devtoolsPort}/json/new?about:blank`, {
      method: 'PUT'
    });
    client = await connect(pages.webSocketDebuggerUrl);
    await client.send('Runtime.enable');
    await client.send('Log.enable');
    await client.send('Page.enable');
    await client.send('Page.navigate', { url: `http://127.0.0.1:${port}/index.html` });
    await waitForHealthyPage(client);
    await verifyReloadPersistence(client);
    await verifyAppSettingsReloadPersistence(client);
    await verifySyncKeyImport(client);
  }
  console.log('web smoke: PASS');
} catch (error) {
  failure = error;
} finally {
  if (client)
    client.close();
  if (chrome) {
    const exited = new Promise(resolveExit => chrome.once('exit', resolveExit));
    try {
      process.kill(-chrome.pid, 'SIGTERM');
    } catch {}
    const stopped = await Promise.race([
      exited.then(() => true),
      new Promise(resolveDelay => setTimeout(() => resolveDelay(false), 2000))
    ]);
    if (!stopped) {
      try {
        process.kill(-chrome.pid, 'SIGKILL');
      } catch {}
      await Promise.race([exited, new Promise(resolveDelay => setTimeout(resolveDelay, 1000))]);
    }
  }
  server.close();
  rmSync(userDataDir, { recursive: true, force: true, maxRetries: 5, retryDelay: 100 });
}

if (failure) {
  console.error(`web smoke: FAIL: ${failure.message}`);
  process.exit(1);
}
