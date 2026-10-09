# Contributing

Contributions are welcome. Read `CLAUDE.md` first: its architecture rules apply to everyone, human or AI.

- **License:** by submitting a contribution you agree it is licensed under the same terms as the project: PolyForm Noncommercial 1.0.0 for code (`LICENSE.md`) and CC BY-NC-SA 4.0 for assets, data and docs (`LICENSE-ASSETS.txt`).
- **Art:** change the generators in `tools/artgen/`, never the PNGs.
- **Content:** buildings, goods and missions are data in `data/`. Mods use the same format in `mods/`.
- Run `cmake --build --preset dev && ctest --preset dev` before opening a PR.
