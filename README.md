# ks

A nix-first Linux remix of [Kisak-Strike](https://github.com/tyabus/Kisak-Strike)

```bash
curl -fsSL https://install.determinate.systems/nix | sh -s -- install
nix run .#setup                               # download depot to game/ (one-time, resumable)
nix run                                       # build + install + launch
nix develop; cd src; waf {configure,install}  # incremental dev build env
```

## Layout

- `src/rocketui` — RmlUi integration: system, filesystem, d3d9 renderer
- `src/game/client/cstrike15/RocketUI/rkhud_*.cpp` — per-element hud controllers
- `game/csgo/rocketui/*.rml` — hud/menu layouts (html/css)
- `src/game/{client,server,shared}` — gameplay
- `src/engine` — engine
- `src/materialsystem/shaderapidx9` — d3d9 path over dxvk
- `src/tier0`–`tier3`, `src/public` — Source base libs and headers
- `src/thirdparty` — dxvk, jolt, rmlui, protobuf, celt, mojoAL, ...
- `src/wscript` — build, subprojects in `PROJECTS`

## Built on

- [Kisak-Strike (LWSS & tyabus' changes)](https://github.com/SwagSoftware/Kisak-Strike)
- [OpenCSGO (dxvk/waf integration)](https://github.com/stephen-cusi/OpenCSGO)

## License

Mostly Valve's Source engine, governed by the Source 1 SDK license:
see [LICENSE](LICENSE) and `thirdpartylegalnotices.txt`.
Kisak's additions are public domain; OpenCSGO gave no formal license.
No game files ship here. New files I wrote (no Valve header) are MIT.
