# ks

A remix of [Kisak-Strike](https://github.com/tyabus/Kisak-Strike)/[OpenCSGO](https://github.com/stephen-cusi/OpenCSGO)

```bash
curl -fsSL https://install.determinate.systems/nix | sh -s -- install
nix run .#setup                               # download depot to game/ (one-time, resumable)
nix run                                       # build + install + launch
nix develop; cd src; cmake -B build && cmake --build build  # incremental dev build
```

## License

Mostly Valve's Source engine, governed by the Source 1 SDK license:
see [LICENSE](LICENSE) and `thirdpartylegalnotices.txt`.
Kisak's additions are public domain; OpenCSGO gave no formal license.
No game files ship here. New files I wrote are MIT.
