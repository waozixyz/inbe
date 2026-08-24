# Web Renderer Tests

Inbe has two web renderer paths:

| Target | Renderer | Purpose |
|---|---|---|
| `make web` | raylib/WebGL | Shipping web build with liboqs sync support. |
| `make web-canvas` | Kryon Canvas2D | No-WebGL fallback build using Kryon's Canvas backend. The app still uses Emscripten/WASM for C code and support libraries. |
| `make web-compare-test` | both | Builds both renderers and runs the browser smoke suite against each. |
| `make web-side-by-side-test` | both | Builds both renderers, runs both smoke tests, then serves a split-page manual comparison view. |

The shared smoke test is `scripts/web-smoke-test.mjs`. It is renderer-aware:

- `WEB_SMOKE_RENDERER=raylib` requires a live WebGL context.
- `WEB_SMOKE_RENDERER=canvas` requires a live Canvas2D context and nonblank pixels.
- Both paths require the app runtime-ready hook, a cycling render loop, IDBFS idle/flush behavior, and app settings persistence.
- Both paths require sync-key import to work. Canvas links the same ksync/liboqs account crypto as raylib-web; only the renderer differs.

Current known gap:

- The shared smoke test verifies account crypto/import parity, rendering, storage, and app settings persistence. It does not yet run a real local sync server transaction for either renderer.

For manual visual comparison, run:

```bash
make web-side-by-side-test
```

The script prints a local URL with raylib/WebGL on the left and Canvas2D on the
right. Use `scripts/web-side-by-side-test.sh --no-serve` for CI-style
build-and-smoke validation without starting the comparison server.
