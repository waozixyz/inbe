import { readFile } from "node:fs/promises";

const baseUrl = "https://developer.amazon.com/api/appstore";

function requireEnv(name) {
  const value = process.env[name];
  if (!value) {
    throw new Error(`${name} is required`);
  }
  return value;
}

async function readResponseText(response) {
  const text = await response.text();
  return text.length > 0 ? text : "(empty response)";
}

async function requestJson(url, options = {}) {
  const response = await fetch(url, options);
  const text = await readResponseText(response);
  if (!response.ok) {
    throw new Error(`${options.method || "GET"} ${url} failed: ${response.status} ${response.statusText}: ${text}`);
  }
  return text === "(empty response)" ? {} : JSON.parse(text);
}

async function requestJsonWithResponse(url, options = {}) {
  const response = await fetch(url, options);
  const text = await readResponseText(response);
  if (!response.ok) {
    throw new Error(`${options.method || "GET"} ${url} failed: ${response.status} ${response.statusText}: ${text}`);
  }
  return {
    data: text === "(empty response)" ? {} : JSON.parse(text),
    response,
  };
}

async function requestOk(url, options = {}) {
  const response = await fetch(url, options);
  const text = await readResponseText(response);
  if (!response.ok) {
    throw new Error(`${options.method || "GET"} ${url} failed: ${response.status} ${response.statusText}: ${text}`);
  }
  return response;
}

const clientId = requireEnv("AMAZON_CLIENT_ID");
const clientSecret = requireEnv("AMAZON_CLIENT_SECRET");
const appId = requireEnv("AMAZON_APP_ID");
const releaseFile = requireEnv("AMAZON_RELEASE_FILE");

console.log("Getting Amazon authentication token");
const tokenParams = new URLSearchParams({
  grant_type: "client_credentials",
  client_id: clientId,
  client_secret: clientSecret,
  scope: "appstore::apps:readwrite",
});
const token = await requestJson("https://api.amazon.com/auth/o2/token", {
  method: "POST",
  headers: { "Content-Type": "application/x-www-form-urlencoded" },
  body: tokenParams,
});
const authHeaders = {
  Authorization: `Bearer ${token.access_token}`,
};
const jsonHeaders = {
  ...authHeaders,
  "Content-Type": "application/json",
};

console.log("Checking for an open Amazon edit");
const { data: activeEdit, response: activeEditResponse } = await requestJsonWithResponse(`${baseUrl}/v1/applications/${appId}/edits`, {
  headers: jsonHeaders,
});
if (activeEdit.id) {
  const activeEditEtag = activeEditResponse.headers.get("etag");
  if (!activeEditEtag) {
    throw new Error(`Amazon did not return an ETag for open edit ${activeEdit.id}`);
  }
  console.log(`Deleting stale open edit ${activeEdit.id}`);
  await requestOk(`${baseUrl}/v1/applications/${appId}/edits/${activeEdit.id}`, {
    method: "DELETE",
    headers: {
      ...jsonHeaders,
      "If-Match": activeEditEtag,
    },
  });
}

console.log("Creating a fresh Amazon edit");
const edit = await requestJson(`${baseUrl}/v1/applications/${appId}/edits`, {
  method: "POST",
  headers: jsonHeaders,
});
if (!edit.id) {
  throw new Error("Amazon did not return an edit id");
}

console.log("Finding APK slot to replace");
const apks = await requestJson(`${baseUrl}/v1/applications/${appId}/edits/${edit.id}/apks`, {
  headers: jsonHeaders,
});
if (!Array.isArray(apks) || apks.length === 0) {
  throw new Error("Amazon edit has no APK slot to replace");
}
const apk = apks[0];
console.log(`Replacing APK ${apk.id} versionCode=${apk.versionCode} name=${apk.name}`);

const apkResponse = await requestOk(`${baseUrl}/v1/applications/${appId}/edits/${edit.id}/apks/${apk.id}`, {
  headers: jsonHeaders,
});
const etag = apkResponse.headers.get("etag");
if (!etag) {
  throw new Error(`Amazon did not return an ETag for APK ${apk.id}`);
}

const apkBytes = await readFile(releaseFile);
await requestOk(`${baseUrl}/v1/applications/${appId}/edits/${edit.id}/apks/${apk.id}/replace`, {
  method: "PUT",
  headers: {
    ...authHeaders,
    "Content-Type": "application/vnd.android.package-archive",
    "If-Match": etag,
  },
  body: apkBytes,
});

console.log("Amazon APK upload completed");
