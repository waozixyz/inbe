const appUrl = chrome.runtime.getURL("index.html");

const BREAK_MICRO = 0;
const BREAK_REST = 1;
const BREAK_DAILY = 2;
const BREAK_TYPES = [BREAK_MICRO, BREAK_REST, BREAK_DAILY];
const TICK_ALARM = "inbe-break-tick";
const STORAGE_CONFIG = "inbeBreakConfig";
const STORAGE_STATE = "inbeBreakState";
const MAX_ELAPSED_S = 300;

const practiceUrls = {
  whm: `${appUrl}?inbe_launch=start-practice&practice=whm`,
  meditation: `${appUrl}?inbe_launch=start-practice&practice=meditation`,
  sun_salutation: `${appUrl}?inbe_launch=start-practice&practice=sun_salutation`,
};

const defaultTimers = [
  {
    enabled: true,
    limitS: 180,
    durationS: 30,
    postponeS: 150,
    maxPrompts: 3,
    showSkip: true,
    showPostpone: true,
  },
  {
    enabled: true,
    limitS: 2700,
    durationS: 600,
    postponeS: 300,
    maxPrompts: 3,
    showSkip: true,
    showPostpone: true,
  },
  {
    enabled: true,
    limitS: 14400,
    durationS: 0,
    postponeS: 1800,
    maxPrompts: 0,
    showSkip: true,
    showPostpone: false,
  },
];

function clampInt(value, fallback, min, max) {
  const n = Number(value);
  if (!Number.isFinite(n)) return fallback;
  return Math.max(min, Math.min(max, Math.round(n)));
}

function normalizeTimer(timer, index) {
  const fallback = defaultTimers[index] || defaultTimers[0];
  return {
    enabled: timer && typeof timer.enabled === "boolean" ? timer.enabled : fallback.enabled,
    limitS: clampInt(timer && timer.limitS, fallback.limitS, 0, 24 * 60 * 60),
    durationS: clampInt(timer && timer.durationS, fallback.durationS, 0, 24 * 60 * 60),
    postponeS: clampInt(timer && timer.postponeS, fallback.postponeS, 0, 24 * 60 * 60),
    maxPrompts: clampInt(timer && timer.maxPrompts, fallback.maxPrompts, 0, 20),
    showSkip: timer && typeof timer.showSkip === "boolean" ? timer.showSkip : fallback.showSkip,
    showPostpone: timer && typeof timer.showPostpone === "boolean" ? timer.showPostpone : fallback.showPostpone,
  };
}

function normalizeConfig(config) {
  const sourceTimers = config && Array.isArray(config.timers) ? config.timers : [];
  return {
    enabled: !!(config && config.enabled),
    timers: BREAK_TYPES.map((type) => normalizeTimer(sourceTimers[type], type)),
  };
}

function defaultTimerState() {
  return {
    activeS: 0,
    idleS: 0,
    promptS: 0,
    promptCount: 0,
    snoozeUntilMs: 0,
    notificationId: "",
    buttonActions: [],
  };
}

function normalizeState(state) {
  const sourceTimers = state && Array.isArray(state.timers) ? state.timers : [];
  return {
    lastTickMs: clampInt(state && state.lastTickMs, 0, 0, Number.MAX_SAFE_INTEGER),
    timers: BREAK_TYPES.map((type) => ({
      ...defaultTimerState(),
      ...(sourceTimers[type] || {}),
    })),
  };
}

function breakTitle(type) {
  if (type === BREAK_REST) return "Rest break";
  if (type === BREAK_DAILY) return "Daily limit";
  return "Microbreak";
}

function breakMessage(type) {
  if (type === BREAK_DAILY)
    return "You have reached your daily practice-work limit. Open Inner Breeze to wind down.";
  if (type === BREAK_REST)
    return "Step away for a longer rest, or open Inner Breeze for a practice.";
  return "Take a short pause, relax your eyes, or start a quick practice.";
}

function storageGet(keys) {
  return chrome.storage.local.get(keys);
}

function storageSet(values) {
  return chrome.storage.local.set(values);
}

function openApp(url = appUrl) {
  chrome.tabs.create({ url });
}

function openBreakSettings() {
  openApp(`${appUrl}?inbe_launch=break-settings`);
}

function openHabits() {
  openApp(`${appUrl}?inbe_launch=habits`);
}

function notificationIdFor(type) {
  return `inbe-break-${type}`;
}

async function showBreakNotification(type, state, config, forced) {
  const timer = config.timers[type];
  const timerState = state.timers[type];
  const buttons = [];
  const buttonActions = [];
  const id = notificationIdFor(type);

  if (timer.showPostpone && timer.postponeS > 0) {
    buttons.push({ title: "Postpone" });
    buttonActions.push("postpone");
  }
  if (timer.showSkip) {
    buttons.push({ title: "Skip" });
    buttonActions.push("skip");
  }

  timerState.notificationId = id;
  timerState.buttonActions = buttonActions;
  await storageSet({ [STORAGE_STATE]: state });
  await chrome.notifications.create(id, {
    type: "basic",
    iconUrl: "icons/icon-128.png",
    title: forced ? `${breakTitle(type)} now` : breakTitle(type),
    message: breakMessage(type),
    priority: 2,
    requireInteraction: true,
    buttons,
  });
}

async function scheduleBreakAlarm(config) {
  if (!config.enabled) {
    await chrome.alarms.clear(TICK_ALARM);
    await chrome.action.setBadgeText({ text: "" });
    return;
  }
  await chrome.alarms.create(TICK_ALARM, { periodInMinutes: 1 });
  await chrome.action.setBadgeText({ text: "BR" });
  await chrome.action.setBadgeBackgroundColor({ color: "#2c7388" });
}

async function saveConfig(config) {
  const normalized = normalizeConfig(config);
  await storageSet({ [STORAGE_CONFIG]: normalized });
  await scheduleBreakAlarm(normalized);
  return normalized;
}

function stepTimer(timer, timerState, elapsedS, active, nowMs) {
  if (!timer.enabled || timer.limitS <= 0) {
    Object.assign(timerState, defaultTimerState());
    return false;
  }
  if (timerState.snoozeUntilMs && nowMs < timerState.snoozeUntilMs)
    return false;
  if (timerState.snoozeUntilMs && nowMs >= timerState.snoozeUntilMs)
    timerState.snoozeUntilMs = 0;

  if (active) {
    timerState.activeS += elapsedS;
    timerState.idleS = 0;
  } else {
    timerState.idleS += elapsedS;
    if (timer.durationS > 0 && timerState.activeS > 0 && timerState.idleS >= timer.durationS) {
      Object.assign(timerState, defaultTimerState());
      return false;
    }
  }

  if (timerState.activeS < timer.limitS)
    return false;

  timerState.promptS += elapsedS;
  if (timerState.promptCount === 0 || timerState.promptS >= 30) {
    timerState.promptS = 0;
    timerState.promptCount += 1;
    return true;
  }
  return false;
}

async function tickBreaks(nowMs = Date.now()) {
  const data = await storageGet([STORAGE_CONFIG, STORAGE_STATE]);
  const config = normalizeConfig(data[STORAGE_CONFIG]);
  const state = normalizeState(data[STORAGE_STATE]);

  if (!config.enabled) {
    await scheduleBreakAlarm(config);
    return state;
  }

  const elapsedS = state.lastTickMs
    ? clampInt((nowMs - state.lastTickMs) / 1000, 60, 1, MAX_ELAPSED_S)
    : 60;
  const idleState = await chrome.idle.queryState(Math.max(60, Math.ceil(elapsedS)));
  const active = idleState === "active";
  state.lastTickMs = nowMs;

  for (const type of BREAK_TYPES) {
    if (stepTimer(config.timers[type], state.timers[type], elapsedS, active, nowMs))
      await showBreakNotification(type, state, config, false);
  }
  await storageSet({ [STORAGE_STATE]: state });
  return state;
}

async function requestBreakNow(type) {
  const data = await storageGet([STORAGE_CONFIG, STORAGE_STATE]);
  const config = normalizeConfig(data[STORAGE_CONFIG]);
  const state = normalizeState(data[STORAGE_STATE]);
  const safeType = BREAK_TYPES.includes(type) ? type : BREAK_REST;

  if (!config.enabled)
    config.enabled = true;
  await showBreakNotification(safeType, state, config, true);
  await storageSet({ [STORAGE_CONFIG]: config, [STORAGE_STATE]: state });
  await scheduleBreakAlarm(config);
}

async function applyNotificationAction(notificationId, buttonIndex) {
  const data = await storageGet([STORAGE_CONFIG, STORAGE_STATE]);
  const config = normalizeConfig(data[STORAGE_CONFIG]);
  const state = normalizeState(data[STORAGE_STATE]);
  const type = BREAK_TYPES.find((candidate) => notificationId === notificationIdFor(candidate));
  if (type === undefined) return;

  const timer = config.timers[type];
  const timerState = state.timers[type];
  const action = timerState.buttonActions && timerState.buttonActions[buttonIndex];
  if (action === "postpone") {
    Object.assign(timerState, defaultTimerState(), {
      snoozeUntilMs: Date.now() + Math.max(1, timer.postponeS) * 1000,
    });
  } else if (action === "skip") {
    Object.assign(timerState, defaultTimerState());
  }
  await chrome.notifications.clear(notificationId);
  await storageSet({ [STORAGE_STATE]: state });
}

function createContextMenus(contexts) {
  chrome.contextMenus.create({ id: "open", title: "Open Inner Breeze", contexts });
  chrome.contextMenus.create({ id: "breaks", title: "Breaks", contexts });
  chrome.contextMenus.create({ id: "break-settings", parentId: "breaks", title: "Break settings", contexts });
  chrome.contextMenus.create({ id: "break-rest-now", parentId: "breaks", title: "Rest now", contexts });
  chrome.contextMenus.create({ id: "habits", parentId: "breaks", title: "Habits", contexts });
  chrome.contextMenus.create({ id: "start-practice", title: "Start Practice", contexts });
  chrome.contextMenus.create({ id: "start-whm", parentId: "start-practice", title: "Wim Hof", contexts });
  chrome.contextMenus.create({ id: "start-meditation", parentId: "start-practice", title: "Meditation", contexts });
  chrome.contextMenus.create({ id: "start-sun-salutation", parentId: "start-practice", title: "Sun Salutation", contexts });
}

function rebuildContextMenus() {
  chrome.contextMenus.removeAll(() => createContextMenus(["action"]));
}

chrome.action.onClicked.addListener(() => {
  openApp();
});

chrome.runtime.onInstalled.addListener(async (details) => {
  rebuildContextMenus();
  const data = await storageGet([STORAGE_CONFIG]);
  await saveConfig(normalizeConfig(data[STORAGE_CONFIG]));
  if (details.reason === "install")
    openApp();
});

chrome.runtime.onStartup.addListener(async () => {
  rebuildContextMenus();
  const data = await storageGet([STORAGE_CONFIG]);
  await scheduleBreakAlarm(normalizeConfig(data[STORAGE_CONFIG]));
});

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  if (!message || typeof message.type !== "string")
    return false;

  if (message.type === "inbe.breakConfig") {
    saveConfig(message.config).then((config) => sendResponse({ ok: true, config }));
    return true;
  }
  if (message.type === "inbe.breakNow") {
    requestBreakNow(clampInt(message.breakType, BREAK_REST, BREAK_MICRO, BREAK_DAILY))
      .then(() => sendResponse({ ok: true }));
    return true;
  }
  return false;
});

chrome.alarms.onAlarm.addListener((alarm) => {
  if (alarm && alarm.name === TICK_ALARM)
    tickBreaks().catch((error) => console.error("Inner Breeze break tick failed:", error));
});

chrome.notifications.onClicked.addListener((notificationId) => {
  if (notificationId && notificationId.startsWith("inbe-break-"))
    openBreakSettings();
});

chrome.notifications.onButtonClicked.addListener((notificationId, buttonIndex) => {
  applyNotificationAction(notificationId, buttonIndex)
    .catch((error) => console.error("Inner Breeze notification action failed:", error));
});

chrome.contextMenus.onClicked.addListener((info) => {
  switch (info.menuItemId) {
    case "open":
      openApp();
      break;
    case "break-settings":
      openBreakSettings();
      break;
    case "break-rest-now":
      requestBreakNow(BREAK_REST).catch((error) => console.error("Inner Breeze rest-now failed:", error));
      break;
    case "habits":
      openHabits();
      break;
    case "start-whm":
      openApp(practiceUrls.whm);
      break;
    case "start-meditation":
      openApp(practiceUrls.meditation);
      break;
    case "start-sun-salutation":
      openApp(practiceUrls.sun_salutation);
      break;
    default:
      break;
  }
});

globalThis.__inbeExtensionBreaks = {
  normalizeConfig,
  normalizeState,
  stepTimer,
  tickBreaks,
  requestBreakNow,
  applyNotificationAction,
};
