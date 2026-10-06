DOSBox-X for OPENSTEP 4.2 Intel
=============================

Extract the archive, copy DOSBox-X.app to your applications directory, and
open it in Workspace. Keep the whole .app folder together; it can be moved.
No compiler, SDL, Python, Telnet helper or /usr/local library is needed to run
the app. AppKit, Interceptor Kit and SoundKit are supplied by OPENSTEP.
Sound playback requires a working OPENSTEP audio driver.

The DOS prompt and bundled DOS utilities are included. Games and their data
are separate. Choose Mount... to mount a game folder as C:, then type C: and
the game's executable name at the DOS prompt. Choose Quit to exit.
Command-F toggles fullscreen; Ctrl-F10 toggles mouse capture.

Preferences are stored in ~/Library/Preferences/DOSBox-X/dosbox-x.conf.
The app preserves existing preferences. A dosbox-x.conf in the launch working
directory takes precedence; -conf selects an explicit configuration file.
The .app itself need not be writable.

SoundKit follows the system audio driver. IntelHDA 0.20 can change its output
live, automatically or through intelhda-route; DOSBox-X needs no driver-specific
code or restart for that feature. Driver tools are not part of this app.

This is an experimental port. The normal and dynamic CPU cores, DOS programs,
Sound Blaster/OPL audio, local folders, disk images and CD images are supported.
The graphical configuration/mapper editors, networking, OpenGL/Voodoo and MIDI
device output are unavailable. Windows guests and PC-98 are not qualified.
If using frame skipping, set [video] scanline render on demand=false; their
combination otherwise has a known blank-screen issue.

DOSBox-X is GPL-2.0-or-later; see COPYING and Resources/Licenses.
Keep the matching source archive available when sharing the binary.
The source archive's openstep/README.md contains build instructions,
validation details and current limitations.
