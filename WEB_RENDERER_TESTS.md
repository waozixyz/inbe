# Web Renderer Tests

Inbe has two web renderer paths:

| Target | Renderer | Purpose |
|---|---|---|
| `make web` | raylib/WebGL | Shipping web build with liboqs sync support. |
| `make web-canvas` | Kryon Canvas2D | No-WebGL fallback build using Kryon's Canvas backend. |
| `make web-compare-test` | both | Builds both renderers and runs the browser smoke suite against each. |

The shared smoke test is `scripts/web-smoke-test.mjs`. It is renderer-aware:

- `WEB_SMOKE_RENDERER=raylib` requires a live WebGL context.
- `WEB_SMOKE_RENDERER=canvas` requires a live Canvas2D context and nonblank pixels.
- Both paths require the app runtime-ready hook, a cycling render loop, IDBFS idle/flush behavior, and app settings persistence.
- The raylib path also requires sync-key import to work.
- The Canvas path requires sync-key import to fail explicitly, because `web-canvas` intentionally uses `scripts/ksync-canvas-shim.c` and does not link liboqs.

Current known gap:

- `web-canvas` is local-only. It does not carry liboqs sync-account crypto or the web sync transport. The test locks this in as an explicit capability difference so Canvas regressions are visible without pretending it has full raylib-web parity.
