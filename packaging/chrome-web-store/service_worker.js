const appUrl = "__WEB_APP_URL__";

chrome.action.onClicked.addListener(() => {
  chrome.tabs.create({ url: appUrl });
});

chrome.runtime.onInstalled.addListener((details) => {
  if (details.reason === "install")
    chrome.tabs.create({ url: appUrl });
});
