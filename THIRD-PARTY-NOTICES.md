# Third-party notices

- **UE4SS**: SDK revision `97b7e501c19d8b2b7c662feee73aaa0dc1f0a4d1`; Unreal headers `eb40a05f49509bdeb1ac39287032b60af585cca8`. Copyright Narknon and contributors, MIT. The runtime is a separate dependency. [Source](https://github.com/UE4SS-RE/RE-UE4SS).
- **MinHook 1.3.4**: revision `c3fcafdc10146beb5919319d0683e44e3c30d537`. Copyright Tsuda Kageyu and contributors; includes Hacker Disassembler Engine by Vyacheslav Patkov. BSD-style terms are reproduced in `LICENSES/MinHook.txt`. [Source](https://github.com/TsudaKageyu/minhook).
- **fmt 11.2.0**: header dependency, MIT; terms in `LICENSES/fmt.txt`. [Source](https://github.com/fmtlib/fmt).
- **Mod Setting Menu 1.0.6 Apply client**: `Scripts/dmm_api.lua` is the unmodified example distributed with the integration guide. The guide explicitly instructs mod authors to copy this client into their own mod. This inclusion does not relicense the menu or its assets. [Mod Setting Menu](https://www.nexusmods.com/thebloodofdawnwalker/mods/271).
- The CMake SDK setup and host import declarations were adapted from the MIT-licensed [Quiet Dawn - Configurable HUD](https://github.com/my-mods/Dawnwalker-Quiet-Dawn-Configurable-HUD), copyright 2026 my-mods. The camera and targeting implementation is separate.
- Functional inspiration: [Free Combat Camera – Camera Directed Targeting](https://www.nexusmods.com/thebloodofdawnwalker/mods/340), inspected version 0.5.11. Original DLL SHA-256: `d58ba2c63098999578a743518c2f4bc6c753cf25cd233198dec443257fad683a`. The original DLL, configuration and bootstrap are not distributed by this project.

Pinned ImGui, ImGuiColorTextEdit and Zydis headers satisfy SDK includes; their libraries are not linked into this mod. Their revisions appear in the CMake file.
