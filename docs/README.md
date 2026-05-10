# libsed (cats) Documentation

libsed is a C++17 library for TCG SED (Self-Encrypting Drive) evaluation and
control. This folder contains everything you need to learn the library and
the underlying TCG protocol.

---

## Who are you?

Three audiences read these docs. Pick the row that matches you and start
from the listed entry points — every doc also carries an **Audience** banner
at its top so you can confirm at a glance.

| Audience | You are... | Start here |
|----------|-----------|------------|
| **TC Developer** | Writing tests / tools *on top of* libsed. Don't usually edit `src/`. | `sed_drive_guide.md` → `cookbook.md` → `examples.md` → **`tc_authoring_guide.md`** |
| **Library Maintainer** | Modifying libsed itself (encoders, transports, composite logic). | `internal/hammurabi_code.md` → **`internal/postmortem_sedutil_compat.md`** → `internal/work_history.md` → `internal/architecture_rationale.md` |
| **Common (everyone)** | Onboarding, protocol learning, wire-level reference. | `tcg_sed_primer.md` → `rosetta_stone.md` → this README |

`internal/` files are mostly Korean (changelog / postmortem / design
discussion); public docs are English. Both audiences share `tcg_sed_primer.md`
and `rosetta_stone.md`.

---

## Pick your path

Different documents target different readers. Start here:

### I'm a TC application developer — I just want to unlock a drive

You need the high-level facade (`SedDrive`, `SedSession`).

1. [`sed_drive_guide.md`](sed_drive_guide.md) — Quick start with `SedDrive`.
2. [`cookbook.md`](cookbook.md) — Copy-paste recipes for the 11 most common
   tasks (discovery, take ownership, range lock, MBR, DataStore, etc.).
3. [`examples.md`](examples.md) — 20 progressive example programs.
4. [`tc_authoring_guide.md`](tc_authoring_guide.md) — How to write a *new*
   TC: Mock vs Sim vs Real, the `scenarioIntent()` pattern, common
   anti-patterns, and a debugging playbook.
5. If you hit an error code or an auth failure, jump to
   [`examples.md`](examples.md) example 14 (Error Handling).

### I'm new to TCG SED — I need to understand the protocol first

Start from zero. SED, Opal, ComIDs, Sessions — what do any of those mean?

1. [`tcg_sed_primer.md`](tcg_sed_primer.md) — 15-chapter tutorial on the TCG
   SED specification. Each chapter points at a runnable example.
2. [`examples.md`](examples.md) — Run examples 01-06 alongside chapters 1-6
   of the primer.
3. When you want the wire-level truth, open
   [`rosetta_stone.md`](rosetta_stone.md).

### I'm building an evaluation platform — I need wire-level control

You need the low-level `EvalApi` with byte-level inspection and fault
injection.

1. [`eval_platform_guide.md`](eval_platform_guide.md) — Architecture,
   multi-threading rules, NVMe DI pattern, SedContext, fault injection.
2. [`examples.md`](examples.md) — Focus on examples 15 (wire inspection),
   16 (step-by-step EvalApi), 17 (composite patterns), 18 (fault
   injection), 19 (multi-session), 20 (custom transport).
3. [`rosetta_stone.md`](rosetta_stone.md) — Byte-exact encoding for every
   TCG command type.
4. [`test_scenarios.md`](test_scenarios.md) — 104-scenario test catalog
   spanning Levels 1–6 (L1=unit, L5=stress, L6=SSC-specific).

### I'm evaluating / debugging a drive from the shell

[`cats_cli_guide.md`](cats_cli_guide.md) — the `cats-cli` tool with
`<Resource> <Action>` subcommand tree, `--json` output, `--pw-env/file/
stdin` password paths, `--sim` for hardware-free logic checks, `--force`
gating on every destructive op, and `eval transaction <script.json>`
for multi-op scenarios. The evaluator-facing counterpart to `sedutil-cli`.

Transaction script schema: [`cats_cli_transaction_schema.md`](cats_cli_transaction_schema.md).

### I'm verifying compatibility or tracing a wire bug

The tools in [`../tools/`](../tools/) have the byte-level answers.

- `tools/cats-cli/` — full evaluation CLI (see `cats_cli_guide.md`).
- `tools/packet_decode.cpp` — decode a captured hex stream into a
  ComPacket header + token tree (the recommended tool for analysing
  user-captured wire dumps).
- `tools/pwhash.cpp` — produce SHA-256 / PBKDF2-HMAC-SHA1 hashes of a
  password (for cross-tool PIN comparison; see LAW 21).
- `tools/token_dump.cpp` — decode a hex stream into TCG tokens (lighter
  than `packet_decode`).
- `tools/sed_discover.cpp` — quick one-shot discovery CLI.
- `tools/sed_manage.cpp` — production-style admin CLI (ownership, lock,
  revert, user management).
- For decisive wire validation, run `./build/tests/golden_validator`
  against `tests/fixtures/golden/*.bin` (real-hardware capture).
  `sed_compare` was retired in 2026-05 — see
  `internal/postmortem_sedutil_compat.md` Theme 7.

### I'm contributing to libsed

See [`internal/`](internal/) — those files are for contributors, not library
users.

- [`internal/hammurabi_code.md`](internal/hammurabi_code.md) — 21 immutable
  laws derived from past bugs. Violate none.
- [`internal/postmortem_sedutil_compat.md`](internal/postmortem_sedutil_compat.md) —
  Narrative postmortem of the 2026-04~05 sedutil wire-compatibility effort.
  7 themes, root cause → lesson, written for fresh maintainers (no
  commit-by-commit detail).
- [`internal/architecture_rationale.md`](internal/architecture_rationale.md) —
  Why the transport layer is split between `ITransport` and `INvmeDevice`.
- [`internal/work_history.md`](internal/work_history.md) — Session-by-session
  changelog. Use the postmortem first; drill down here for details.

---

## Document map

| Document | Audience | Purpose |
|----------|----------|---------|
| [`sed_drive_guide.md`](sed_drive_guide.md) | TC Developer | Quick start using the `SedDrive` facade |
| [`cookbook.md`](cookbook.md) | TC Developer | 11 copy-paste recipes |
| [`examples.md`](examples.md) | TC Developer | Guide to the 20 example programs |
| [`tc_authoring_guide.md`](tc_authoring_guide.md) | TC Developer | How to write a new TC: env choice, intent pattern, debugging playbook |
| [`eval_platform_guide.md`](eval_platform_guide.md) | TC Developer (wire-level) | `EvalApi`, threading, NVMe DI, fault injection |
| [`test_scenarios.md`](test_scenarios.md) | TC Developer | 104-scenario test catalog |
| [`cats_cli_guide.md`](cats_cli_guide.md) | TC Developer | `cats-cli` shell reference |
| [`cats_cli_transaction_schema.md`](cats_cli_transaction_schema.md) | TC Developer | JSON transaction script schema |
| [`tcg_sed_primer.md`](tcg_sed_primer.md) | Common | 15-chapter TCG protocol tutorial |
| [`rosetta_stone.md`](rosetta_stone.md) | Common | Byte-exact encoding reference (TC + Maintainer) |
| [`internal/hammurabi_code.md`](internal/hammurabi_code.md) | Maintainer | 21 immutable encoding rules from past bugs |
| [`internal/postmortem_sedutil_compat.md`](internal/postmortem_sedutil_compat.md) | Maintainer | Narrative postmortem of 2026-04~05 sedutil compat work |
| [`internal/architecture_rationale.md`](internal/architecture_rationale.md) | Maintainer | Why transport is split `ITransport` + `INvmeDevice` |
| [`internal/work_history.md`](internal/work_history.md) | Maintainer | Session-by-session changelog |
| [`internal/future_api_ideas.md`](internal/future_api_ideas.md) | Maintainer | Backlog ideas |
| [`internal/cats_cli_review.md`](internal/cats_cli_review.md) | Maintainer | Design critique of `cats-cli` |

---

## Reading order for self-study

If you're going end-to-end on your own:

1. `tcg_sed_primer.md` chapters 0-3 + run examples 01-04.
2. `sed_drive_guide.md` + run examples 05-08.
3. `cookbook.md` — try the recipes that match your goal.
4. `tcg_sed_primer.md` chapters 4-15 + run examples 09-14.
5. `eval_platform_guide.md` + examples 15-20 — only if you need wire-level
   control.
6. `rosetta_stone.md` — consult when a packet doesn't look right.

The `examples.md` guide also ties every chapter to a runnable program, so
you can flip between "why" (primer) and "how" (example) chapter by chapter.
