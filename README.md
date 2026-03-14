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

## Codebase Notes

The internal structure of this project is not as clean as it could be. 4-track started as a small experiment and gradually grew in scope. Because of this, the codebase and some parts of the architecture are not as well organized as they would be in a project designed with the current feature set from the beginning. It could use a good refactoring, but for now I prefer to focus my time on building new tools and experiments. 
If you explore the code, expect rough edges.

## License

GNU Affero General Public License v3.0 — see [LICENSE](LICENSE).

---

4-TRACK is open source and free to use. If you enjoy the project, you can support it on [Ko-fi](https://ko-fi.com/raulpavel). Thank you!
