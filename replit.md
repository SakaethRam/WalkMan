# WalkMan

WalkMan is a portable C++20 self-healing audio engine that detects damaged PCM WAV regions, evaluates multiple reconstructions, and writes an explainable repaired file.

## Run & Operate

- `pnpm --filter @workspace/api-server run dev` — run the API server (port 5000)
- `pnpm run typecheck` — full typecheck across all packages
- `pnpm run build` — typecheck + build all packages
- `pnpm --filter @workspace/api-spec run codegen` — regenerate API hooks and Zod schemas from the OpenAPI spec
- `pnpm --filter @workspace/db run push` — push DB schema changes (dev only)
- Required env: `DATABASE_URL` — Postgres connection string
- `cmake -S walkman -B walkman/build -DBUILD_TESTING=ON && cmake --build walkman/build --parallel` — build WalkMan
- `walkman/build/walkman_demo` — run the deterministic clean → corrupt → heal demo
- `walkman/build/walkman_cells` — run the 15-cell notebook companion as one executable

## Stack

- pnpm workspaces, Node.js 24, TypeScript 5.9
- API: Express 5
- DB: PostgreSQL + Drizzle ORM
- Validation: Zod (`zod/v4`), `drizzle-zod`
- API codegen: Orval (from OpenAPI spec)
- Build: esbuild (CJS bundle)

## Where things live

- `walkman/include/walkman/` — public C++20 engine interfaces and data structures
- `walkman/src/` — WAV I/O, signal analysis, detection, corruption, repairs, scoring, and orchestration
- `walkman/examples/walkman_demo.cpp` — CLI and synthetic end-to-end demonstration
- `walkman/notebook/walkman_cells.cpp` — 15 executable notebook-style cells
- `walkman/tests/smoke_test.cpp` — compact pipeline regression check

## Architecture decisions

- The first build has no mandatory third-party dependency so it can run in Zerve-like environments without package setup.
- WAV decoding/writing and radix-2 FFT are isolated behind small interfaces, making libsndfile/FFTW replacement straightforward.
- Region indices are audio frames; sample vectors remain interleaved and normalized doubles internally.
- A reference file enables objective scoring; no-reference healing reports local self-consistency instead of claiming ground-truth accuracy.

## Product

WalkMan loads or generates WAV audio, injects deterministic test damage, detects suspicious regions, generates linear, spline, waveform-match, and spectral repairs, selects the best candidate, writes repaired WAV audio, and prints a transparent integrity report.

## User preferences

_Populate as you build — explicit user instructions worth remembering across sessions._

## Gotchas

- WalkMan writes output as 16-bit PCM while preserving input sample rate and channel count.
- `walkman_demo` creates `walkman_output/` only when no input WAV is supplied.
- Repair scores without a clean reference are local continuity scores, not proof that the hidden original was exactly recovered.

## Pointers

- See the `pnpm-workspace` skill for workspace structure, TypeScript setup, and package details
