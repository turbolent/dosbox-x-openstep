# DOSBox-X for OPENSTEP

Experimental native port of DOSBox-X 2026.10.01 (`e013b8bf8`) to OPENSTEP 4.2
Intel. The full emulator builds with GCC/G++ 4.2.1 and boots DOS with the normal
and dynamic x86 CPU cores. AppKit handles windows, menus and events;
Interceptor Kit handles video; SoundKit handles stereo audio. This target has
no SDL headers, libraries or compatibility layer.

## Build and package

Runtime requirement: OPENSTEP 4.2 Intel. The standalone app needs only the
system AppKit, Foundation, SoundKit, Interceptor and System frameworks.
It needs no SDL, compiler, Python or libraries from `/usr/local` at runtime.

Build requirements: OPENSTEP 4.2 Intel, GCC/G++ 4.2.1, GNU make, ksh, Python 3,
and installed static libiconv and zlib libraries and headers in `/usr/local`.
Run from the source root:

```sh
/usr/local/bin/ksh openstep/build.sh dosbox-x gui-tools
/usr/local/bin/ksh openstep/build.sh app
/usr/local/bin/ksh openstep/build.sh package-source
```

The app is `openstep/output/DOSBox-X.app`, with executable `DOSBox-X`.
The binary archive is `openstep/output/DOSBox-X-OPENSTEP-i386.tar.gz`; the native
source archive is `openstep/output/dosbox-x-openstep-source.tar.gz`. The source
archive contains the current `src`, `include`, native port and zlib sources;
optional dependencies for other platform builds are not included. Packaging
keeps test executables, local work, configuration and guest data out of releases.
The icon is converted from the existing `contrib/icons/dosbox-x.png`.
The main link explicitly uses static libiconv and zlib archives. Packaging
audits the Mach-O load commands before and after stripping and rejects
non-system dependencies or an incompatible executable. The bundle includes
a short user README and library notices in `Resources/Licenses`.

Always use `build.sh` on the target. Its supervisor builds serially, samples
process-group virtual memory and free disk each second, and stops the build
above 256 MiB or below 256 MiB free disk. Sampled limits can briefly overshoot.
Core dumps are disabled. Native Mach allocations can bypass ordinary data
limits, so an unsupervised build is unsafe on the 512 MiB test host.

The large interpreter switches, configuration initializer and DOS utilities
use `-O0`; other C++ units use `-O1` with frequent compiler garbage collection.
A full integration build with the corrected compiler peaked at 173 MiB.
Embedded DOS binaries are converted from the original byte arrays into
assembler data to avoid exhausting the old
compiler. Do not enable forced inlining or parallel compilation on this host.

The compiler must include the [GCC PR33887 fix](https://gcc.gnu.org/legacy-ml/gcc-patches/2008-01/msg00862.html).
Unpatched Apple GCC 4.2.1 build 5666.3 miscompiles narrow C++ arithmetic,
including descriptor switches with the port's `-fwrapv` option. Install a
corrected OPENSTEP compiler package before building. Compiler patches and
bootstrap validation belong to that package.

Every emulator build recompiles and runs `compiler-test` before compiling
objects. It checks five-bit descriptor dispatch and 24-bit arithmetic wrap,
and rejects the original compiler. `Descriptor::Type()` uses the normal
bit-field accessor; there is no longer a DOSBox-specific compiler workaround.
`descriptor-test` also checks all 32 types through the actual emulator header.

MSYS2 is used locally for source checks and transfers. Final Mach-O objects are
compiled on OPENSTEP. No WSL or newer GCC release is needed; the 4.2.1 backport
above is required.
For a new remote checkout, transfer the source archive and extract it; do not
transfer local `openstep/output` or `openstep/work` directories.

## Launch and use

Copy the entire `DOSBox-X.app` to an applications directory, then open
it in Workspace. Its files can be relocated together; the bundle need not be
writable when running. Command-line launch:

```sh
/path/to/DOSBox-X.app/DOSBox-X -defaultconf -fastlaunch
/path/to/DOSBox-X.app/DOSBox-X -conf /path/to/dosbox-x.conf
/path/to/DOSBox-X.app/DOSBox-X /me/dosgames/GAME.EXE -exit
```

For Telnet sessions, join the logged-in desktop's Mach bootstrap namespace.
Find the current user's Workspace/WM PID using `ps -auxww`, then run:

```sh
openstep/output/generic-exec DESKTOP_PID /usr/bin/open /path/to/DOSBox-X.app
```

The helpers use the openstep-telnet-gui workflow. They do not change Unix identity.
Recheck the desktop PID after reboot or login. `open` returning does not alone
prove an app launched; check the process and displayed window.

First launch creates defaults in `~/Library/Preferences/DOSBox-X/dosbox-x.conf`
when neither a working-directory `dosbox-x.conf` nor preferences exist. Explicit
`-conf` files take precedence; `-defaultconf` skips automatic configuration.
Existing preferences are never replaced during first-launch creation.
Use `CONFIG` in DOS to change settings, and `CONFIG -writeconf` to save them.
`-noconfig` disables the emulated CONFIG.SYS section, as upstream does.
At most eleven `-c` startup commands are supported; use a DOS batch file for
longer sequences. `-time-limit SECONDS` is available for bounded tests.
Positional BAT/COM/EXE files run from an automatically mounted directory;
directories and ZIP files mount as a DOS drive. Use `-o` for guest program
arguments. Quote host paths containing spaces. Shell metacharacters in startup
paths are rejected because these paths are used to construct DOS batch commands.

The native menu provides fullscreen, mouse capture, pause, reset, mount,
unmount and quit. Shortcuts include Command-F for fullscreen, Command-R for
reset, Command-Pause for pause, Ctrl-F10 for mouse capture and Ctrl-F9 for quit.
Ctrl-F6 pastes text from the OPENSTEP clipboard into the DOS BIOS keyboard buffer.
The graphical configuration and mapper editors are unavailable.

Useful native settings:

```ini
[openstep]
fullscreen=false
buffered=false
autolock=true
sensitivity=100
showdetails=false
output=interceptor

[cpu]
core=dynamic

[mixer]
rate=44100
blocksize=1024

[render]
aspect=true
scaler=none
```

Window resizing and fullscreen use aspect-correct nearest-neighbor scaling.
`buffered=true` selects Interceptor's buffered window path. Fullscreen uses the
current main display mode. Audio chooses supported 44100 or 22050 Hz, signed
16-bit stereo. SoundKit queues only PCM that the guest has already mixed;
empty producer requests wait for samples instead of repeating the last sample.
Catch-up bursts stay in order when SoundKit requests several blocks at once;
only a stale mixer backlog of at least 250 ms is discarded.
The native clock retains up to 250 ms of pending emulation after scheduling
delays, processed in 20 ms batches, to avoid starving audio by dropping time.
Pause and modal dialogs suspend audio and release mouse capture.
Physical output selection belongs to the sound driver. IntelHDA 0.20 supports
automatic jack detection and live selection through
`/private/Drivers/i386/IntelHDA.config/intelhda-route`. Use `auto` for automatic
selection or the driver's reported pin number for a manual route. No emulator
change or reboot is required; SoundKit mute flags do not select the jack.

Host folders, FAT images, ZIP drives, ISO images and BIN/CUE are supported.
WAV-backed CUE audio must be 44100 Hz, stereo, 16-bit PCM. Examples:

```text
mount c /me/dosgames
imgmount d /me/discs/game.cue -t iso
imgmount -u d
mount -u c
```

Host stdio offsets are limited to signed 32-bit values; oversized seeks fail.
The compatibility layer supplies unsigned-byte `strcmp`/`strncmp` ordering
for DOS filenames and code-page strings; OPENSTEP's native routines can order
bytes above 127 incorrectly.
Overlay copies preserve the base file and discard their newly created partial
file if copying or flushing fails. WAV, OPL and MIDI capture use the existing
DOSBox-X capture code; `DX-CAPTURE /A /-D program` records audio around a DOS
program. PNG screenshots and video recording are not implemented in this target.

## Validation and limits

Verified on the rebuilt OPENSTEP host:

- Relocated `.app` launched through Workspace, with its native icon and menu.
  Workspace's `-MachLaunch` value contains two numbers in a single argument.
- Normal and dynamic DOS boot; integer and 32-bit operations, x87 arithmetic,
  self-modifying code, memory operations, bundled MEM, local read/write, ZIP
  mounting and unmounting. Actual DOS framebuffer captures were inspected.
- Bundled protected-mode ZIP help, archive creation and update, and UNZIP
  integrity checks with both CPU cores; host Python independently decompresses
  and compares both archive entries byte for byte.
- AppKit event routing for typed DOS commands and native menu actions;
  fullscreen/window changes (including while paused), mouse capture, pause/resume,
  reset, native folder mounting/unmounting verified by DOS file reads, and clean quit.
- Modal file chooser Cancel and unchanged guest pixels after returning in direct
  window, fullscreen and buffered window modes. AppKit needs an outer application
  loop for modal actions to finish; the emulator runs inside that loop.
- Actual DOS MSCDEX data reads from ISO, BIN/CUE and WAV/CUE; bundled CDPLAY audio
  playback; completed WAV recordings with 440 Hz left and 660 Hz right channels.
  A DOS PC-speaker program also records the expected tone through the mixer.
- A separate 60-second SoundKit output harness reported zero underruns. This and
  mixer recordings do not establish physical speaker output or audible quality.
- Native pixel/stride, input/focus, unsigned-byte string ordering, checked file
  offset, overlay write failure,
  libiconv, ZIP stream, executable memory, renderer and CD parser tests.
- First-launch preferences, preservation across PATH-based relaunch, positional
  programs and folders, missing-file errors, and real DOS overlay append with
  unchanged base data. The launch tests use an isolated temporary home.
- All 103 modified shared C++ translation units pass an MSYS2 C++14/SDL2 syntax
  check with OPENSTEP disabled. This is not a complete build of the other targets.
- Embedded DOS data: 65 arrays, 4,234,639 bytes checked against original literals.
  Float80 digest `c80d2f7eff541609` and DBOPL OPL2/OPL3 digests
  `14818c05` / `c3be1c71` match current upstream compiled under MSYS2.

Stunts 1.1 was also exercised through its manual-word prompt, acceleration and
braking with AppKit events. The test profile uses `core=dynamic`, `cycles=8000`,
`[render] frameskip=1` and `[video] scanline render on demand=false`, with
`loadfix -64 stunts.com`. The conventional scanline setting avoids a blank
picture seen with frame skipping and on-demand rendering. SoundKit remains
at 44100 Hz with 1024-frame blocks. Native mixer continuity and clock tests
pass; physical listening confirmation is still required.

Physical keyboard/mouse behavior and audible output still need user testing.
Windows guests, PC-98 and save-state compatibility are not acceptance claims.
This profile disables OpenGL/Voodoo/Glide, networking, hardware port passthrough,
MIDI synthesizers, MT-32, TrueType, xBRZ, printer rendering, DOS clipboard-device
integration, compressed CD audio, CHD, CloneCD and host CD devices. Pure UDF
probing and CUE lookup through guest drive paths are not enabled. MIDI/OPL
recording does not imply MIDI device output. Unsupported features in a shared
configuration should be disabled; native defaults do so.

Run focused checks with:

```sh
python3 openstep/tests/package_test.py
/usr/local/bin/ksh openstep/build.sh host-test core-headers-test libc-test math-test
/usr/local/bin/ksh openstep/build.sh code-cache-test float80-test render-test input-test
/usr/local/bin/ksh openstep/build.sh compiler-test descriptor-test
/usr/local/bin/ksh openstep/build.sh iconv-test minizip-test cdrom-test shell-env-test opl-test builtin-test
/usr/local/bin/ksh openstep/build.sh dialog-test gui-emulator-test
```

The last two build separate instrumented GUI executables; they are never copied
into the app. `native-test` and `render-gui-test` are graphics/audio fixtures,
not DOS guests. GUI tests require the desktop namespace and bounded controllers.
`audio-queue-test` builds a native SoundKit regression fixture that exercises
empty and partial producer requests, paced playback, pause/resume and reopen.
Run `openstep/output/audio-queue-test` through `generic-exec` in the same way.
`tick-backlog-test` checks that short scheduling delays preserve elapsed guest
time and that long pauses or sustained overload leave bounded catch-up work.
`mixer-queue-test` checks the real mixer callback for sample continuity through
catch-up bursts and ring wrap, incomplete requests and stale-backlog recovery.

With the desktop's current WM PID, run the protected-mode ZIP regression:

```sh
openstep/output/generic-exec WM_PID /usr/local/bin/python3 openstep/tests/protected_mode_test.py /path/to/source
```

It creates and updates an archive with both CPU cores, checks it with the
bundled UNZIP, and independently decompresses and compares its contents.

Run `openstep/tests/launch_smoke.py SOURCE_ROOT /path/to/DOSBox-X.app` through
the same desktop helper to test an installed bundle with a fresh temporary
home, unrelated working directory and PATH limited to the bundle and system
commands. It checks preferences, positional files (including paths with
spaces), CPU/FPU execution and overlays without using the source-tree binary.

## Licenses

DOSBox-X and this port are GPL-2.0-or-later; see `COPYING`. Native display/audio
code adapts the GPL Quake II OPENSTEP port by the same workspace's `quake2` tree.
Third-party source notices remain in their respective files.

`vendor/stb_sprintf.h` is unmodified stb_sprintf 1.10 from nothings/stb commit
`2c980bb59875b0d32144a71867fbdebb2f77cd20`. Its MIT/public-domain license is at the
end of the file. SHA-256:
`e0b8c56e1084602290b9a17ef59f429b5293deb8cb9d8b0f20a14abc39b56520`.
It supplies bounded formatting and 64-bit integer output missing from native libc.
