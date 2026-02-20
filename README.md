# Open CS:GO

```bash
curl -fsSL https://install.determinate.systems/nix | sh -s -- install
nix run .#setup                               # download depot to game/ (one-time, resumable)
nix run                                       # build + install + launch
nix develop; cd src; waf {configure,install}  # incremental dev build env
```

## Projects Used:

- [csgo-src](https://github.com/SourceSDK2013Ports/csgo-src)
- [Kisak-Strike](https://github.com/SwagSoftware/Kisak-Strike)
- [VPhysics-Jolt](https://github.com/Joshua-Ashton/VPhysics-Jolt)
- [protobuf](https://github.com/protocolbuffers/protobuf)
- [mojoAL](https://icculus.org/mojoAL)
- [DirectXMath](https://github.com/microsoft/DirectXMath)
- [DXVK Native](https://github.com/doitsujin/dxvk)
- [libpng](http://www.libpng.org/pub/png/libpng.html)
- [jpeglib](https://ijg.org)
