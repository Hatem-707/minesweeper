# Roboto Regular

Hinted TrueType font from the official Google Fonts Roboto repository:
https://github.com/googlefonts/roboto-2/blob/main/src/hinted/Roboto-Regular.ttf

Downloaded October 6, 2026. Distributed under Apache License 2.0; the original
license is included as `LICENSE-Roboto.txt`.

SHA-256: `56a45233d29f11b4dfb86d248e921939d115778f87325e7ae8cc108383d6664d`

CMake copies the font and license beside the executable in `assets/fonts`.
The app loads an atlas at each UI pixel size once, after opening the window,
and unloads those atlases before closing the window. No runtime download is used.
