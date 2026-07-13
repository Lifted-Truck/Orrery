# CLAUDE.md — coupled-rings/ (engine territory — SPEC-GAPPED)

Sub-charter. Read root `../../CLAUDE.md` and contract
`../../sequencer-studio-architecture.md` first.

## ⚠ Status: prototype only — implementation BLOCKED
Only `coupled-rings.html` exists. There is **no spec yet**, so this engine is
not ready to build (root DECISIONS #4, ROADMAP O4). The other engines earned
validated specs before implementation; this one must too.

## §Domain (what is known from the contract + prototype)
Ring B's particles feel potential wells created by ring A's positions →
interlocking parts. `sourceId` = (ring, particle id) packed (contract §1.2).
The strongest post-v1 feature of the elastic model (elastic spec §9); the
contract's slot/port model (§7) exists so multi-ring coupling is not a rewrite.

## Next task (O4a, gated): write the spec
Produce `coupled-rings-spec.md` in this dir, in the shape of the sibling specs
(model / state / forces or coupling law / clocking + latch / determinism /
acceptance tests / parameters), derived from `coupled-rings.html` and the
coupling notes in the contract §7 and elastic spec §9. Human-reviewed before
any C++. Until then, do not implement.
