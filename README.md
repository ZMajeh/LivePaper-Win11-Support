# LivePaper
![Build](https://github.com/ZMajeh/LivePaper-Win11-Support/actions/workflows/build.yml/badge.svg) ![Platforms Windows | Linux](https://img.shields.io/badge/Platforms-Windows%20%7C%20Linux-blue)

LivePaper is a lightweight application that displays live wallpapers on Windows and Linux.
<br>Both gif and video files are supported. If FFmpeg can play the file, LivePaper can show it as your desktop background.

![Screencap](assets/screencap.gif)

_Wallpaper art by [Toyoi Yuuta](https://twitter.com/1041uuu)_

## Installation
### Windows
Use the latest prebuilt binaries: [MSI installer](https://github.com/ZMajeh/LivePaper-Win11-Support/releases/download/latest/LivePaper-0.0.0-win64.msi) or [portable zip](https://github.com/ZMajeh/LivePaper-Win11-Support/releases/download/latest/LivePaper-0.0.0-win64.zip).

### Linux
Use the prebuilt package: [tarball](https://github.com/ZMajeh/LivePaper-Win11-Support/releases/download/latest/LivePaper-0.0.0-linux-x86_64.tar.gz). It contains the program in `bin/`; the libraries of SDL and of FFmpeg are the ones of your system.

You can also build it locally from source. Instructions are [here](#build).

## Usage
### Windows
Simply run live-paper.exe. You can make LivePaper run automatically at startup by following [this guide](https://support.microsoft.com/en-us/windows/add-an-app-to-run-automatically-at-startup-in-windows-10-150da165-dcd9-7230-517b-cf3c295d89dd) from Microsoft (works for Windows 7+).

On its first run, LivePaper will open a window to choose the wallpaper file. The app will remember this file and play it the next time you run LivePaper. You can also change the wallpaper file or quit the app via the tray icon.  
![Close with tray icon](assets/tray.png)

Note that if LivePaper quits due to an error, the remembered wallpaper file will be cleared. The next time the app is started, you will need to choose the wallpaper file again.

### Linux
LivePaper is a console application on Linux.
```
Usage: live-paper [-hv] [--stats] [--log-file=<path>] [--fit-mode=<mode>] [<file>]
Display a video or an animated file as desktop background.

  -h, --help   = display this help and exit
  -v, --verbose = print detailed diagnostics
  --stats      = print runtime statistics every few seconds
  --log-file=<path> = write diagnostics to the given file
  --fit-mode=<mode>
               = controls the way the wallpaper is fit on screen
                 possible values: fit, fill, center
  <file>       = video or animation file to display

Examples: live-paper loop.mp4
          live-paper --fit-mode=fill wallpaper.gif
          live-paper --verbose --stats --log-file=live-paper.log loop.mp4
```

Append `&` to run LivePaper in background. Run `killall live-paper` to stop it.

## Diagnostics
LivePaper can report what it is doing, which is useful when a wallpaper doesn't
show up or a file doesn't play. Diagnostics are written to stderr and can also
be saved to a log file with `--log-file`.

```
live-paper --verbose --stats --log-file=live-paper.log loop.mp4
```

The startup report shows whether the file is readable and what it contains,
whether the wallpaper was actually attached to the desktop, and the memory
usage of the process:

```
[2026-10-20 16:37:54] INFO : live-paper starting, file='assets/test.gif'
[2026-10-20 16:37:54] INFO : SDL 2.32.4 initialized
[2026-10-20 16:37:54] INFO : X11 display ':0' opened, screen 0 is 1920x1080
[2026-10-20 16:37:54] INFO : drawing onto the root window of ':0' (1920x1080)
[2026-10-20 16:37:54] INFO : renderer: opengl (video driver: x11)
[2026-10-20 16:37:54] INFO : loaded 'assets/test.gif': format=gif codec=gif size=1000x562 duration=3.84s fps=8.33 file_size=372242
[2026-10-20 16:37:54] INFO : wallpaper: set (X11 root window)
[2026-10-20 16:37:54] INFO : media: format=gif codec=gif resolution=1000x562 duration=3.84s fps=8.33 file_size=363.5 KiB
[2026-10-20 16:37:54] INFO : memory: rss=137.5 MiB peak_rss=142.2 MiB system_total=7.4 GiB
```

With `--stats` the frame rate, frame counters and memory usage are printed
every five seconds:

```
[2026-10-20 16:37:59] INFO : stats: fps=8.3 frames_decoded=42 frames_rendered=42 loops=1 last_decode=2.0ms rss=165.5 MiB
```

| Option | Description |
| --- | --- |
| `-v`, `--verbose` | Enable debug level messages |
| `--stats` | Print runtime statistics every few seconds |
| `--log-file=<path>` | Also write the diagnostics to the given file |

On Windows the application has no console window, so use `--log-file` to
capture the diagnostics, or attach a debugger to see them in the output window.

## Build
### Windows
Visual Studio 2019 or later is recommended. CMake will download all dependencies during configuration when building with VS.

You can do everything from Visual Studio, if desired. Choose "Clone a repostiory..." from the startup menu to clone this repository. VS will configure the project using CMake. Then build as usual.

You can also build in terminal, similarly to the Linux instructions below. You need to install the requirements manually if using e.g. MinGW.

### Linux
Install the requirements
- cmake 3.14
- ffmpeg OR libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libavdevice-dev for Ubuntu and the like
- sdl2

Example for Ubuntu 

```bash
$ sudo apt-get -yq install cmake libsdl2-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libavdevice-dev
```

For Arch
```bash
$ sudo pacman -S cmake sdl2 ffmpeg
```

Then clone the repository, configure and build with CMake.
```bash
$ git clone https://github.com/ZMajeh/LivePaper-Win11-Support.git && cd LivePaper-Win11-Support
$ cmake -B build
$ cmake --build build
```
To test, run
```
$ ./build/live-paper assets/test.gif &
```

To build the package that is published in the release, run
```bash
$ cd build && cpack -C Release
```
The result is in `build/dist/`.

### Cross-compiling for Windows from Linux
The Windows executable can also be built on Linux with the MinGW-w64 toolchain.
The required dependencies (SDL2 and FFmpeg) are downloaded automatically.

```bash
$ sudo apt-get -yq install mingw-w64 cmake curl unzip tar zip
$ ./makeLinuxToWindow.sh
```

The result is placed in `dist-windows/`, together with a `LivePaper-win64.zip`
archive that contains the executable and the DLLs it needs. Copy the whole
folder to a Windows machine and run `live-paper.exe`.

The script accepts a few options; run `./makeLinuxToWindow.sh --help` for the
full list.

```bash
$ ./makeLinuxToWindow.sh --output /tmp/win --build-type Debug
```

## Known Issues
* Probably not all video file types are supported at the moment; let me know if your file doesn't work
* Unicode is not fully supported on Windows yet
* Doesn't work with compositors on Linux (yet?)
* App quits with an error if it finds any issue with the input file
* Only one display is supported

## Notice

Art by [Toyoi Yuuta](https://twitter.com/1041uuu) is used for test wallpapers.
