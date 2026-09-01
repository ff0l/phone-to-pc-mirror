# Mirror

Windows AirPlay receiver. The iPhone stays stock. Open Control Center, tap Screen Mirroring, pick this PC.

No iPhone app. No account. Same Wi-Fi as the phone.

<p align="center">
  <img src="docs/connected.png" alt="iPhone lock screen mirrored on Windows">
</p>

Written by [ff0l](https://github.com/ff0l).

## Use

1. Run `Mirror.exe` from the release zip, or from `dist\` after a build.
2. Set the Windows network profile to **Private** and allow the firewall prompt.
3. On the iPhone: Control Center → **Screen Mirroring** → the name in the window (this PC’s computer name).
4. Stay on Screen Mirroring. Do not tap AirPlay inside TikTok or YouTube. That sends the clip only.

Drag the window from the picture. The X in the top-right closes it.

| Key | Action |
| --- | --- |
| F11 | Fullscreen |
| Esc | Leave fullscreen |

Phone and PC must share one LAN. Ethernet plus Wi-Fi on the same router is fine. Cellular, guest Wi-Fi, and AP isolation will not see the receiver.

Protected apps (Netflix and similar) stay black. That is Apple DRM.

## Release

A standalone Windows build is on [Releases](https://github.com/ff0l/phone-to-pc-mirror/releases). Unzip and run `Mirror.exe`. Keep the DLLs and `lib\` folder next to the exe.

## Tree

```
app/                    Win32 host
scripts/build.ps1       MSYS2 build and portable package
third_party/uxplay      UxPlay 1.74, library mode
assets/                 close-button font
docs/                   screenshots
```

## Build

Windows 10 or 11. [MSYS2](https://www.msys2.org/) at `C:\msys64`.

```powershell
.\scripts\build.ps1 bootstrap
.\scripts\build.ps1 package
```

`bootstrap` installs the UCRT64 toolchain and GStreamer. `package` compiles a Release build and writes `dist\Mirror.exe` plus the runtime.

```powershell
.\scripts\build.ps1 build
```

Compile only. Output is `build\Mirror.exe`. Run the packaged `dist\` copy, not the bare build exe.

## Notes

UxPlay is vendored at `third_party/uxplay`. AirPlay is one-way video and audio. The mouse cannot tap the phone.
