# Repository Guidelines

## Project Structure & Module Organization
`src/` contains the main MFC clipboard manager plus bundled third-party code under `src/sqlite`, `src/TinyXml`, `src/chaiscript`, `src/QRCode`, and `src/zlib`. `Shared/` holds helpers reused across projects. Solution-level companion projects live in `Addins/DittoUtil/`, `EncryptDecrypt/`, `ICU_Loader/`, `focusdll/`, `FocusHighlight/`, and `U3Stop/`. UI assets and icons are under `res/`; runtime language and theme files are staged in `Debug/Language/` and `Debug/Themes/`. Packaging and release automation live in `DittoSetup/`.

## Build, Test, and Development Commands
Run commands from a Visual Studio 2022 Developer Command Prompt on Windows.

`nuget restore CP_Main_10.sln` restores native dependencies declared in `packages.config`.

`msbuild CP_Main_10.sln /p:Configuration=Release /p:Platform=x64` matches pull request CI and builds the primary release target.

`msbuild CP_Main_10.sln /p:Configuration=Release /p:Platform=Win32` is the expected follow-up when touching platform-specific code.

`DittoSetup\BuildPortableZIP.bat "DittoPortable_local" bit64` packages a portable x64 build from `Release64/`.

## Coding Style & Naming Conventions
Follow the existing MFC/C++ style instead of reformatting files wholesale: tabs for indentation, Allman braces, and `#include "stdafx.h"` first in translation units. Keep comments in English to match the repository. Use PascalCase for types, `m_` prefixes for member fields, and `Name.h` / `Name.cpp` file pairs. Prefer small, module-local changes over opportunistic cleanup across unrelated files.

## Testing Guidelines
This repository does not include a dedicated unit-test project. The minimum bar for a change is a clean x64 `Release` build and a Windows smoke test of the affected path, such as clipboard capture, Quick Paste, network sync, add-ins, or installer behavior. If you modify packaging or release scripts, run the relevant `DittoSetup/` step locally when possible. For UI changes, include screenshots in the PR.

## Commit & Pull Request Guidelines
Match the existing history: short, imperative commit subjects such as `Update ReadMe.md`, `Added option to...`, or `Fixed issue with...`. Keep each commit focused on one logical change. Pull requests should summarize user-visible impact, link the related issue, list the commands or manual checks performed, and call out installer, signing, or release-script effects explicitly.
