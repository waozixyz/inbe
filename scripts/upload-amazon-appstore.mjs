import { readFile } from "node:fs/promises";
import { extname } from "node:path";

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

function releaseKindForFile(file) {
  const ext = extname(file).toLowerCase();
  if (ext === ".apk") {
    return {
      label: "APK",
      resource: "apks",
      contentType: "application/vnd.android.package-archive",
    };
  }
  if (ext === ".aab") {
    return {
      label: "AAB",
      resource: "bundles",
      contentType: "application/octet-stream",
    };
  }
  throw new Error(`Unsupported Amazon release file extension "${ext}". Expected .apk or .aab`);
}

function describeAsset(asset) {
  return `id=${asset.id ?? "(missing)"} versionCode=${asset.versionCode ?? "(unknown)"} name=${asset.name ?? "(unknown)"}`;
}

function assetMatchesReleaseKind(asset, releaseKind) {
  const searchableValues = [
    asset.name,
    asset.fileName,
    asset.filename,
    asset.fileExtension,
    asset.extension,
    asset.type,
    asset.assetType,
  ]
    .filter((value) => typeof value === "string")
    .map((value) => value.toLowerCase());

  if (releaseKind.label === "APK") {
    return !searchableValues.some((value) => value.includes("aab") || value.includes("bundle"));
  }

  return !searchableValues.some((value) => value.includes("apk"));
}

async function findBinaryCollection(editId, releaseKind) {
  const resource = releaseKind.resource;
  const assets = await requestJson(`${baseUrl}/v1/applications/${appId}/edits/${editId}/${resource}`, {
    headers: jsonHeaders,
  });
  if (!Array.isArray(assets)) {
    throw new Error(`Amazon edit returned an unexpected ${resource} response: ${JSON.stringify(assets)}`);
  }
  const matchingAssets = assets.filter((asset) => assetMatchesReleaseKind(asset, releaseKind));
  if (matchingAssets.length === 0) {
    const assetDescriptions = assets.map(describeAsset).join(", ") || "(none)";
    throw new Error(`Amazon edit has no ${releaseKind.label} asset to replace in ${resource}. Found: ${assetDescriptions}`);
  }
  return { resource, assets: matchingAssets };
}

const clientId = requireEnv("AMAZON_CLIENT_ID");
const clientSecret = requireEnv("AMAZON_CLIENT_SECRET");
const appId = requireEnv("AMAZON_APP_ID");
const releaseFile = requireEnv("AMAZON_RELEASE_FILE");
const releaseKind = releaseKindForFile(releaseFile);

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

console.log(`Finding ${releaseKind.label} slot to replace`);
const { resource, assets } = await findBinaryCollection(edit.id, releaseKind);
const asset = assets[0];
if (!asset.id) {
  throw new Error(`Amazon ${releaseKind.label} asset did not include an id: ${JSON.stringify(asset)}`);
}
console.log(`Replacing ${releaseKind.label} ${describeAsset(asset)} via ${resource}`);

const assetResponse = await requestOk(`${baseUrl}/v1/applications/${appId}/edits/${edit.id}/${resource}/${asset.id}`, {
  headers: jsonHeaders,
});
const etag = assetResponse.headers.get("etag");
if (!etag) {
  throw new Error(`Amazon did not return an ETag for ${releaseKind.label} ${asset.id}`);
}

const releaseBytes = await readFile(releaseFile);
await requestOk(`${baseUrl}/v1/applications/${appId}/edits/${edit.id}/${resource}/${asset.id}/replace`, {
  method: "PUT",
  headers: {
    ...authHeaders,
    "Content-Type": releaseKind.contentType,
    "If-Match": etag,
  },
  body: releaseBytes,
});

console.log(`Amazon ${releaseKind.label} upload completed`);
