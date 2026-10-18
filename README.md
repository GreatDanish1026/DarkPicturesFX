# DarkPicturesFX

A small ReShade add-on that turns camera effects on or off in **The Dark Pictures
Anthology** (House of Ashes, Little Hope, Man of Medan, The Devil in Me). It works on its own
or next to [DarkPicturesDLSS](https://github.com/GreatDanish1026/DarkPicturesDLSS), on
DirectX 11 or 12.

Version **0.1.0**. Tested in The Devil in Me: every toggle works there except light
shafts, which that game doesn't expose (the toggle shows as unavailable).

## Effects

| Toggle | What it turns off | Engine setting forced while off |
|---|---|---|
| Depth of field | Background and foreground blur, mostly in cutscenes | `r.DepthOfFieldQuality 0` |
| Motion blur | Blur on fast camera and object motion | `r.MotionBlurQuality 0` |
| Lens flares | Flares and ghosts from bright lights | `r.LensFlareQuality 0` |
| Chromatic aberration | Colour fringing towards the screen edges | `r.SceneColorFringeQuality 0` |
| Bloom | Glow around bright areas, including lens dirt | `r.BloomQuality 0` |
| Light shafts | God rays from the sun and bright lights | `r.LightShaftQuality 0` |
| Film grain and vignette (experimental) | Grain and darkened corners | `r.Tonemapper.Quality 0` |

Every effect is on by default (the game's own look). Untick one to turn it off; tick it again
to give the game its own setting back. Settings are saved in `ReShade.ini` under
`[DarkPicturesFX]`.

Film grain and vignette are experimental: the game has no separate setting for them, so the
toggle switches to Unreal's simplest tonemapper, which leaves both out. In The Devil in Me it
removes the vignette and leaves colours and contrast as they were; no film grain could be
measured there with it on or off. Grain that a game draws in its own post-process pass may stay.

## Install

1. Install [ReShade](https://reshade.me) 6.8 or newer **with full add-on support** for the
   game's `*-Win64-Shipping.exe` (in `SMG0xx\Binaries\Win64`).
2. Copy `DarkPicturesFX.addon64` into that same `Win64` folder.
3. In game, press **Home**, open the **Add-ons** tab and the **DarkPicturesFX** section.

## Reporting a problem

Send `DarkPicturesFX.log` from the `Win64` folder. It lists which settings were found and
every change.

## Building (developers)

```powershell
.\build.ps1             # -> bin\DarkPicturesFX.addon64
.\build.ps1 -Install    # also copies it into the game folder (-GameBin, House of Ashes by default)
.\build.ps1 -Package    # -> bin\release\DarkPicturesFX-<version>.zip
```

Building needs Visual Studio 2022 or newer with the C++ x64 tools. The ReShade add-on API and
Dear ImGui headers are in `third_party/`. The engine settings are found at run time from the
game's own code (`src/cvars.hpp`): the console manager is located through the code that looks
up `r.ScreenPercentage`, and each setting through `FindConsoleVariable`, with Unreal Engine
4.26's interface layout.

## Credits

Uses the [ReShade](https://reshade.me) add-on API and [Dear ImGui](https://github.com/ocornut/imgui).
See THIRD_PARTY_NOTICES.txt.
