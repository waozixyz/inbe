import assert from "node:assert/strict";
import fs from "node:fs";
import vm from "node:vm";

const workerSource = fs.readFileSync("packaging/chrome-web-store/service_worker.js", "utf8");
const storage = {};
const events = {};
const notifications = [];
const tabs = [];
const alarms = [];
let idleState = "active";

function event(name) {
  events[name] = [];
  return {
    addListener(listener) {
      events[name].push(listener);
    },
  };
}

const chrome = {
  runtime: {
    getURL(path) {
      return `chrome-extension://inbe/${path}`;
    },
    onInstalled: event("runtime.onInstalled"),
    onStartup: event("runtime.onStartup"),
    onMessage: event("runtime.onMessage"),
  },
  storage: {
    local: {
      async get(keys) {
        const out = {};
        for (const key of keys)
          out[key] = storage[key];
        return out;
      },
      async set(values) {
        Object.assign(storage, values);
      },
    },
  },
  alarms: {
    async create(name, spec) {
      alarms.push({ name, spec });
    },
    async clear(name) {
      alarms.push({ name, clear: true });
    },
    onAlarm: event("alarms.onAlarm"),
  },
  idle: {
    async queryState() {
      return idleState;
    },
  },
  notifications: {
    async create(id, options) {
      notifications.push({ id, options });
      return id;
    },
    async clear(id) {
      notifications.push({ id, clear: true });
      return true;
    },
    onClicked: event("notifications.onClicked"),
    onButtonClicked: event("notifications.onButtonClicked"),
  },
  action: {
    onClicked: event("action.onClicked"),
    async setBadgeText(value) {
      chrome.action.badgeText = value.text;
    },
    async setBadgeBackgroundColor(value) {
      chrome.action.badgeColor = value.color;
    },
  },
  contextMenus: {
    create(item) {
      chrome.contextMenus.items.push(item);
    },
    removeAll(callback) {
      chrome.contextMenus.items = [];
      if (callback) callback();
    },
    items: [],
    onClicked: event("contextMenus.onClicked"),
  },
  tabs: {
    create(tab) {
      tabs.push(tab);
    },
  },
};

const context = {
  chrome,
  console,
  Date,
  globalThis: {},
  setTimeout,
  clearTimeout,
};
context.globalThis = context;

vm.runInNewContext(workerSource, context, {
  filename: "packaging/chrome-web-store/service_worker.js",
});

const api = context.__inbeExtensionBreaks;
assert.ok(api, "service worker exposes test API");

const normalized = api.normalizeConfig({
  enabled: true,
  timers: [{ limitS: 10.4, durationS: 2, showSkip: false }],
});
assert.equal(normalized.enabled, true);
assert.equal(normalized.timers[0].limitS, 10);
assert.equal(normalized.timers[0].showSkip, false);
assert.equal(normalized.timers[1].limitS, 2700);

events["runtime.onInstalled"][0]({ reason: "install" });
await new Promise((resolve) => setImmediate(resolve));
assert.ok(chrome.contextMenus.items.some((item) => item.id === "break-rest-now"));
assert.ok(tabs.some((tab) => tab.url === "chrome-extension://inbe/index.html"));

await api.requestBreakNow(1);
assert.equal(storage.inbeBreakConfig.enabled, true);
assert.ok(notifications.some((entry) => entry.id === "inbe-break-1"));
assert.equal(chrome.action.badgeText, "BR");

storage.inbeBreakConfig = api.normalizeConfig({
  enabled: true,
  timers: [
    { enabled: true, limitS: 60, durationS: 30, postponeS: 120, maxPrompts: 3, showSkip: true, showPostpone: true },
    { enabled: false },
    { enabled: false },
  ],
});
storage.inbeBreakState = api.normalizeState({ lastTickMs: 1000 });
idleState = "active";
await api.tickBreaks(61000);
assert.ok(notifications.some((entry) => entry.id === "inbe-break-0" && entry.options.buttons.length === 2));

await api.applyNotificationAction("inbe-break-0", 0);
assert.ok(storage.inbeBreakState.timers[0].snoozeUntilMs > Date.now(), "postpone stores snooze deadline");

storage.inbeBreakState = api.normalizeState({
  lastTickMs: 1000,
  timers: [{ activeS: 80, idleS: 0 }],
});
idleState = "idle";
await api.tickBreaks(61000);
assert.equal(storage.inbeBreakState.timers[0].activeS, 0, "natural idle break resets timer");

events["notifications.onClicked"][0]("inbe-break-1");
assert.ok(tabs.some((tab) => tab.url.endsWith("?inbe_launch=break-settings")));

events["contextMenus.onClicked"][0]({ menuItemId: "habits" });
assert.ok(tabs.some((tab) => tab.url.endsWith("?inbe_launch=habits")));

console.log("Chrome extension break service worker tests passed");
