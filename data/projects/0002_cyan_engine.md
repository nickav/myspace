---
title: "Cyan Engine"
desc:  "Making a game engine from scratch"
date:  "2021-11-19 10:13:03"
image: "journey_01_pixel.png"
author: "Nick Aversano"
---

I made a game engine from scratch in C++ with minimal external dependencies.

This was a very fun project to work on.

Some highlights of the project are:
- Win32, MacOS native platform layer code (window, input)
- minimal C++ features (mostly just C with overloading, a small amount of templates)
- OpenGL rendering backend
- audio backend (two backends: DirectSound on Win32, OpenAL on MacOS)
- custom audio mixer
- some SIMD code
- data structures (Array, Hash_Table)
- hot asset reloading
- asset processing system
- controller support (XInput, RawInput)
- custom IMGUI layer / in-game editor
- font rendering (with TrueType / stb_truetype as backends)
- load .tga and .wav files reading

I also made the music and sprites for the game 🙂

@img{'journey_01_pixel.png', 'Cyan Engine'}
