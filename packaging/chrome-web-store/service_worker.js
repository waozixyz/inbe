const appUrl = chrome.runtime.getURL("index.html");

const practiceUrls = {
  whm: `${appUrl}?inbe_launch=start-practice&practice=whm`,
  meditation: `${appUrl}?inbe_launch=start-practice&practice=meditation`,
  sun_salutation: `${appUrl}?inbe_launch=start-practice&practice=sun_salutation`,
};

function openApp(url = appUrl) {
  chrome.tabs.create({ url });
}

function createContextMenus(contexts) {
  chrome.contextMenus.create({
    id: "open",
    title: "Open Inner Breeze",
    contexts,
  });
  chrome.contextMenus.create({
    id: "start-practice",
    title: "Start Practice",
    contexts,
  });
  chrome.contextMenus.create({
    id: "start-whm",
    parentId: "start-practice",
    title: "Wim Hof",
    contexts,
  });
  chrome.contextMenus.create({
    id: "start-meditation",
    parentId: "start-practice",
    title: "Meditation",
    contexts,
  });
  chrome.contextMenus.create({
    id: "start-sun-salutation",
    parentId: "start-practice",
    title: "Sun Salutation",
    contexts,
  });
}

function rebuildContextMenus() {
  chrome.contextMenus.removeAll(() => createContextMenus(["action"]));
}

chrome.action.onClicked.addListener(() => {
  openApp();
});

chrome.runtime.onInstalled.addListener((details) => {
  rebuildContextMenus();
  if (details.reason === "install")
    openApp();
});

chrome.runtime.onStartup.addListener(() => {
  rebuildContextMenus();
});

chrome.contextMenus.onClicked.addListener((info) => {
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
