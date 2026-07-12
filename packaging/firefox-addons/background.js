const extensionApi = globalThis.browser || globalThis.chrome;
const appUrl = extensionApi.runtime.getURL("index.html");

const practiceUrls = {
  whm: `${appUrl}?inbe_launch=start-practice&practice=whm`,
  meditation: `${appUrl}?inbe_launch=start-practice&practice=meditation`,
  sun_salutation: `${appUrl}?inbe_launch=start-practice&practice=sun_salutation`,
};

function openApp(url = appUrl) {
  extensionApi.tabs.create({ url });
}

function createContextMenus(contexts) {
  extensionApi.contextMenus.create({
    id: "open",
    title: "Open Inner Breeze",
    contexts,
  });
  extensionApi.contextMenus.create({
    id: "start-practice",
    title: "Start Practice",
    contexts,
  });
  extensionApi.contextMenus.create({
    id: "start-whm",
    parentId: "start-practice",
    title: "Wim Hof",
    contexts,
  });
  extensionApi.contextMenus.create({
    id: "start-meditation",
    parentId: "start-practice",
    title: "Meditation",
    contexts,
  });
  extensionApi.contextMenus.create({
    id: "start-sun-salutation",
    parentId: "start-practice",
    title: "Sun Salutation",
    contexts,
  });
}

function rebuildContextMenus() {
  const removed = extensionApi.contextMenus.removeAll();

  if (removed && typeof removed.then === "function")
    removed.then(() => createContextMenus(["action"]));
  else
    createContextMenus(["action"]);
}

extensionApi.action.onClicked.addListener(() => {
  openApp();
});

extensionApi.runtime.onInstalled.addListener((details) => {
  rebuildContextMenus();
  if (details.reason === "install")
    openApp();
});

extensionApi.runtime.onStartup.addListener(() => {
  rebuildContextMenus();
});

extensionApi.contextMenus.onClicked.addListener((info) => {
  switch (info.menuItemId) {
    case "open":
      openApp();
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
