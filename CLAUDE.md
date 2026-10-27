# CoA64

## Conventions

- **Inline Comments**: Keep inline comments terse (1-2 lines) stating only "why". Long-form rationale and measurement history belong in a dated `docs/` page. Enforced by `Tools/comment_budget.mjs` (ratchet baseline `Tools/comment_budget.json`, PostToolUse hook in `.claude/settings.json`, guards in `Tools/__tests__`, run with `node --test "Tools/__tests__/*.test.mjs"`). `ThirdParty/` is verbatim upstream and exempt.
