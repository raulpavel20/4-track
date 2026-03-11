# 4-Track

A tape-style 4-track recorder. Record on 4 tracks, set loop markers, and print each track through its own pre-tape FX chain. Mix the tracks, send them into 3 post-tape effect buses, and export your session.

![4-Track overview](assets/Overview.png)

## Features

- 4 tape tracks with loop markers
- Pre-tape FX chain per track (EQ, compressor, saturation, delay, reverb, chorus, phaser, and more)
- 3 post-tape send buses
- Mixer with track routing and bouncing
- Export to WAV

## Download

Pre-built binaries: [4track.pulsarmachine.ro](https://4track.pulsarmachine.ro)

- **Windows** — .exe
- **Linux** — AppImage
- **macOS** — .app

## Build

### Prerequisites

- CMake 3.22+
- C++20 compiler
- [JUCE](https://github.com/juce-framework/JUCE) (included as submodule)

### Build steps

```bash
git clone --recursive https://github.com/raulpavel20/4-track.git
cd 4-track
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## License

GNU Affero General Public License v3.0 — see [LICENSE](LICENSE).

---

4-TRACK is open source and free to use. If you enjoy the project, you can support it on [Ko-fi](https://ko-fi.com/raulpavel). Thank you!
