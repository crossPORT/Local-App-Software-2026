# PWA agent guidance (`apps/web`)

RocketBox App (Vite PWA). UI + hooks + transfer orchestrator only.

## Host app shape

- Peers from announce / `syncSystems` (SDK owns ROCKETBX presence on HW)
- Transfers via `ensureCircuit` + ROCKETBX send/receive (no port switch on the session path; C++ parity)
- Do **not** assume Session ATTACH/LIST on real hardware

## Hard rule

**No USB or wire implementation here.** Use `@rocketbox/sdk`.

Allowed:

- Capture `?simulate=` / `?port=` and pass into `createRocketBoxTransport`
- Call SDK `RocketBoxTransport` APIs, session codecs, port helpers
- UI, identity/settings, `useRocketBox`, `session_orchestrator`

Forbidden in `apps/web/src` (except via `@rocketbox/sdk` imports):

- `navigator.usb`, `claimInterface`, `transferIn` / `transferOut`
- Local USB or wire codecs

## Verify

```bash
cd apps/web && npx tsc --noEmit && npm test
```

See also: [sdks/typescript/AGENTS.md](../../sdks/typescript/AGENTS.md), root [AGENTS.md](../../AGENTS.md).
