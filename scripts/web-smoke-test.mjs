#!/usr/bin/env node

import { createServer } from 'node:http';
import { createReadStream, mkdtempSync, rmSync, existsSync, readFileSync, accessSync, constants } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, normalize, resolve, sep } from 'node:path';
import { spawn } from 'node:child_process';

const root = resolve(process.argv[2] || 'build/dist/web');
const browserSetting = process.env.WEB_SMOKE_BROWSER || 'auto';
const browserArgs = (process.env.WEB_SMOKE_BROWSER_ARGS || '').split(/\s+/).filter(Boolean);
const timeoutMs = Number(process.env.WEB_SMOKE_TIMEOUT_MS || 30000);
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
    'chrome'
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

function formatChromeLaunchError(stderrLines) {
  const stderr = stderrLines.join('').trim();
  const suffix = stderr ? `; chrome stderr: ${stderr.slice(-4000)}` : '';
  return new Error(`Chrome exited before DevToolsActivePort was ready${suffix}`);
}

function waitForDevtoolsPort(file, chrome, stderrLines) {
  const start = Date.now();
  return new Promise((resolveWait, reject) => {
    let settled = false;
    const finish = callback => {
      if (settled) return;
      settled = true;
      clearInterval(timer);
      chrome.off('exit', onExit);
      callback();
    };
    const onExit = () => {
      finish(() => reject(formatChromeLaunchError(stderrLines)));
    };
    const timer = setInterval(() => {
      if (existsSync(file)) {
        finish(() => {
          const [port] = readFileSync(file, 'utf8').trim().split('\n');
          resolveWait(port);
        });
        return;
      }
      if (Date.now() - start > 10000) {
        const stderr = stderrLines.join('').trim();
        const suffix = stderr ? `; chrome stderr: ${stderr.slice(-4000)}` : '';
        finish(() => reject(new Error(`timed out waiting for Chrome DevToolsActivePort${suffix}`)));
      }
    }, 50);
    chrome.once('exit', onExit);
  });
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
        if (!canvas) return { ok: false, reason: 'missing canvas' };
        const gl = canvas.getContext('webgl') || canvas.getContext('experimental-webgl');
        if (!gl) return { ok: false, reason: 'missing webgl' };
        const width = gl.drawingBufferWidth;
        const height = gl.drawingBufferHeight;
        return {
          ok: width > 0 && height > 0,
          width,
          height,
          loadingClass: document.querySelector('#loading-screen')?.className || ''
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
  throw new Error(`web app did not become healthy within ${timeoutMs}ms; state=${JSON.stringify(lastState)}; recent=${recent}`);
}

async function waitForStorageIdle(client) {
  const start = Date.now();
  let lastState = null;

  while (Date.now() - start < timeoutMs) {
    const result = await client.send('Runtime.evaluate', {
      expression: `(() => ({
        syncing: !!Module.__inbeStorageSyncing,
        pending: !!Module.__inbeStorageSyncPending,
        timer: !!Module.__inbeStorageSyncTimer
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

async function verifyReloadPersistence(client) {
  const marker = `web-smoke-${Date.now()}`;
  await client.send('Runtime.evaluate', {
    expression: `(() => {
      try { FS.mkdir('/home/inbe'); } catch (e) {}
      FS.writeFile('/home/inbe/web-smoke-persist.txt', ${JSON.stringify(marker)});
      if (typeof Module.__inbeFlushStorageSync !== 'function')
        throw new Error('missing immediate storage flush helper');
      Module.__inbeFlushStorageSync(true);
      return true;
    })()`,
    returnByValue: true
  });
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

let chrome;
let client;
let failure;

try {
  const port = await listen();
  const browser = resolveBrowser();
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

  const devtoolsPort = await waitForDevtoolsPort(join(userDataDir, 'DevToolsActivePort'), chrome, chromeStderr);
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
