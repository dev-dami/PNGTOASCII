# Release Checklist

Before creating a release:

1. Ensure `make build`, `make test`, and `make memcheck` all pass.
2. Confirm `README.md` and CLI docs reflect current flags.
3. Verify deterministic fixture outputs in `tests/expected` remain stable.
4. Update `CHANGELOG.md` with release highlights.
5. Tag and publish after final review.
