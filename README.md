# tactmon - Simple realtime beat detector using [aubio](https://aubio.org/)

---

## Features
- Supports two input modes:  
  - `pipewire` — captures audio from a PipeWire sink  
  - ~~`stdin` — reads raw 32-bit float mono audio from standard input~~ (buggy)
- BPM Detection
- Json output
- Minimal dependencies: only PipeWire and aubio

## Usage

From pipewire sink
```sh
tactmon
```
tactmon will create pipewire sink which need to connect to source of music!
Also see `tactmon --help` for more options
## Installation

Currently, only Arch Linux and Arch-based distributions are supported.

### Arch-based distros

`tactmon` provides a `PKGBUILD` for building the package from source:

```sh
git clone https://github.com/Beengoo/tactmon.git
cd tactmon/
makepkg -si
```

**Note: No official AUR package is provided at this time. If you find one, it is unofficial and not maintained by the author.**

## Building manually
To compile tactmon you need this dependencies:
- GCC compiler
- PipeWire development files (libpipewire-0.3)
- aubio library

tactmon repo provides few scripts for running and compiling it

```sh
git clone https://github.com/Beengoo/tactmon.git
cd tactmon
./compile.sh // Compile executable with debug labels
./compile_prod.sh // Compile ready to use executable
./run.sh // Executes ./compile.sh and run compiled executable
```

## Support

### Maintenance
If you wanna maintain this project - you're welcome!
I do not accept more than 1 feature or fix per pull request.

### Financial support
I'm not accepting donations for now (and probably in near feature too), if you want to, you'd better support my country in the war against the terrorists: [Sternenko Fondation](https://www.sternenkofund.org/en), [Come Back Alive Foundation](https://savelife.in.ua/en/) or any other.
Even one cent is enough, there are no small donations.

## License - MIT
