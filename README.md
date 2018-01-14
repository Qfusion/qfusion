# qfusion

[![Build Status][travis-badge]][travis-url]
[![Build Status][appveyor-badge]][appveyor-url]
[![Coverity Scan Build Status][coverity-badge]][coverity-url]

http://qfusion.github.io/qfusion/

qfusion is the id Tech 2 derived game engine

## Features (incomplete list)

- Fully open-source under the GPLv2 and easy to mod
- Runs on Linux, macOS, Windows and Android (in development)
- Modern and fast OpenGL 3.0 and OpenGL ES 3.0 renderer, running in a dedicated program thread
- Scriptable <a href="https://github.com/Qfusion/qfusion/wiki">User Interface</a> based on XHTML/CSS standards with support for remote content and scalable vector graphics
- Support for vertex and skeletal animation
- HDR & Bloom support with configurable color correction profiles
- Fullscreen Anti-Aliasing support in the form of MSAA or FXAA
- Powerful multiplayer & eSports features (global stats, friend lists, IRC, TV-server, etc.)
- Ready to go FPS example gametype scripts from Warsow
- Multithreaded sound mixer design
- OpenAL support
- Hardware-accelerated Ogg Theora video playback

## Extensible

- C/C++ mods (plugins) can ship new gameplay features while maintaining compatibility with the core
- UI and game mechanics scriptable with <a href="http://www.angelcode.com/angelscript/">AngelScript</a> (C++ style syntax)
- Flexible HUD scripting
- "Pure" (models, maps, textures, sounds) game content is automatically delivered to players by game servers
- Players are allowed to locally override non-"pure" game content

## Notable games
- <a href="https://www.warsow.net/">Warsow</a>

## License (GPLv2)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


[travis-badge]: https://travis-ci.org/Qfusion/qfusion.svg?branch=master
[travis-url]: https://travis-ci.org/Qfusion/qfusion
[appveyor-badge]: https://ci.appveyor.com/api/projects/status/ijn380lud31mepv6?svg=true
[appveyor-url]: https://ci.appveyor.com/project/viciious/qfusion
[coverity-badge]: https://scan.coverity.com/projects/qfusion/badge.svg
[coverity-url]: https://scan.coverity.com/projects/qfusion
