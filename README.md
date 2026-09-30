# Vibrate

`vibrate` is a Defold native extension for vibration and haptic feedback on
Android, iOS, and HTML5. The same Lua API is registered on every target,
including unsupported desktop targets, so game code does not need to test
whether the global module exists.

## Installation

Add the ZIP archive for a released tag to the dependencies in your Defold
project's `game.project`, then fetch libraries:

```text
https://github.com/HGPoint/defold_vibration/archive/refs/tags/<tag>.zip
```

Replace `<tag>` with the release tag you want to use.

## Quick start

```lua
local ok, reason = vibrate.trigger({
    preset = "impact_medium",
    intensity = 0.8,
})

if not ok then
    print("Haptics unavailable:", reason)
end
```

Calling `vibrate.trigger()` or `vibrate.trigger(nil)` requests the native
default effect. Platforms that need a timed fallback use 1000 ms.

## API

### `vibrate.trigger([options])`

Plays an effect and returns:

```lua
local success, reason = vibrate.trigger(options)
```

`reason` is `nil` on success. A runtime failure returns one of:

- `unsupported`
- `not_allowed`
- `not_visible`
- `rejected`
- `no_hardware`
- `platform_error`

Invalid field types, invalid ranges, unknown enum strings, and incompatible
option combinations raise a Lua error. A successful return means that the
platform accepted the request; the operating system, browser, hardware, or
user settings can still suppress physical feedback.

### Common options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `duration_ms` | integer | `1000` | One-shot duration, from 0 to 3,600,000 ms. |
| `intensity` | number | `1.0` | Intensity from 0 to 1. Platforms without amplitude control approximate or ignore it. |
| `sharpness` | number | `0.5` | Sharpness from 0 to 1. Primarily used by iOS Core Haptics. |
| `fallback` | boolean | `true` | Permit a less capable effect when the requested effect is unavailable. |
| `preset` | string | `"default"` | Semantic effect preset. |
| `pattern` | array | none | One to 256 ON/OFF durations in milliseconds, beginning with ON. |
| `intensities` | array | none | Per-entry values from 0 to 1; length must match `pattern`. Pause-entry values are ignored. |
| `sharpnesses` | array | none | Per-entry values from 0 to 1; length must match `pattern`. Pause-entry values are ignored. |
| `android` | table | none | Android-specific override. |
| `ios` | table | none | iOS-specific override. |
| `web` | table | none | HTML5-specific override. |

Available presets are:

```text
default, selection,
impact_light, impact_medium, impact_heavy, impact_soft, impact_rigid,
success, warning, error,
click, double_click, tick, heavy_click
```

A common pattern follows the Web Vibration API convention. For example,
`{ 50, 25, 100 }` means vibrate for 50 ms, pause for 25 ms, then vibrate for
100 ms. `intensities` and `sharpnesses` have the same number of entries, but
values at even pause indexes are ignored.

### Resolution and precedence

The active platform resolves one effect source in this order:

1. A platform-specific mode or payload.
2. The common `pattern`.
3. The common `preset`.
4. `duration_ms`, or the default effect when no explicit duration was supplied.

Platform subtables are safe to include together in a single call; only the
subtable for the current platform is used. If several effect sources target the
same platform, only the highest-priority source is selected. `fallback = true`
permits that backend to degrade the selected source; it does not retry every
lower-priority field as a separate effect.

```lua
local ok, reason = vibrate.trigger({
    preset = "impact_medium", -- portable fallback

    android = {
        mode = "predefined",
        effect = "heavy_click",
        usage = "game",
    },

    ios = {
        mode = "impact",
        style = "rigid",
    },

    web = {
        pattern = { 30, 20, 50 },
    },
})
```

## Android options

```lua
android = {
    mode = "auto",
    amplitude = -1,
    effect = "click",
    usage = "game",
    timings = { ... },
    amplitudes = { ... },
    repeat_index = -1,
    primitives = { ... },
}
```

Modes:

- `auto` selects the best available implementation for the supplied payload.
- `one_shot` uses common `duration_ms` and either `android.amplitude` or common
  `intensity`.
- `waveform` uses raw `android.timings` and optional `android.amplitudes`.
- `predefined` uses `android.effect`.
- `composition` uses `android.primitives`.

`amplitude` and each `amplitudes` entry accept `-1` for the device default or
an integer from 0 to 255. When `timings` and `amplitudes` are both supplied,
their lengths must match. Both arrays contain between 1 and 256 entries.
Raw `android.timings` use Android's native waveform convention: the first
entry is an initial delay, followed by alternating vibration and pause entries.

`repeat_index` is `-1` for no repeat or a positive, 1-based Lua index into
`android.timings` (or the common `pattern` when raw timings are omitted). A
repeating waveform continues until `vibrate.cancel()`.

Predefined effects:

```text
click, double_click, tick, heavy_click
```

If `android.usage` is omitted, it defaults to `game`. An explicit value overrides
this default for all Android modes.

Usages:

```text
touch, game, notification, alarm, media
```

A composition contains between 1 and 256 primitives:

```lua
vibrate.trigger({
    fallback = true,
    android = {
        mode = "composition",
        usage = "game",
        primitives = {
            { name = "click", scale = 1.0, delay_ms = 0 },
            { name = "tick",  scale = 0.5, delay_ms = 40 },
        },
    },
})
```

Primitive names are `click`, `thud`, `spin`, `quick_rise`, `slow_rise`,
`quick_fall`, `tick`, and `low_tick`. `scale` defaults to 1 and is limited to
0..1; `delay_ms` defaults to 0.

Android feature availability depends on the OS and vibrator hardware:

- basic one-shot and patterns have legacy fallbacks;
- amplitude-controlled effects require API 26+ and suitable hardware;
- predefined effects require API 29+;
- primitive compositions require API 30+ and device support;
- `VibratorManager` is used where available on API 31+.

When `fallback = true`, unsupported advanced requests may be approximated by
the Android backend or reduced to a one-shot effect. Exact tactile output
varies by manufacturer.

With `fallback = false`, Android returns `unsupported` when the selected mode
cannot represent an explicitly requested common intensity or sharpness instead
of silently discarding that modifier.

## iOS options

```lua
ios = {
    mode = "auto",
    style = "medium",
    notification = "success",
    repeat = false,
    events = { ... },
    ahap_json = nil,
}
```

Modes:

- `system` requests the legacy system vibration.
- `selection` uses selection feedback.
- `impact` uses `style`: `light`, `medium`, `heavy`, `soft`, or `rigid`.
- `notification` uses `notification`: `success`, `warning`, or `error`.
- `core_haptics` plays custom `events` or `ahap_json`.
- `auto` selects an appropriate semantic or custom implementation.

Core Haptics events contain `type`, `time_ms`, `duration_ms`, `intensity`, and
`sharpness`:

```lua
vibrate.trigger({
    fallback = true,
    ios = {
        mode = "core_haptics",
        events = {
            {
                type = "transient",
                time_ms = 0,
                intensity = 1.0,
                sharpness = 0.9,
            },
            {
                type = "continuous",
                time_ms = 80,
                duration_ms = 240,
                intensity = 0.55,
                sharpness = 0.25,
            },
        },
    },
})
```

`type` is `transient` or `continuous`. A continuous event requires
`duration_ms > 0`. Event intensity and sharpness default to 1 and 0.5.
`time_ms` and `duration_ms` are integer milliseconds. Event arrays contain
between 1 and 256 entries.

For full Core Haptics authoring, pass an AHAP document directly:

```lua
local ahap = sys.load_resource("/haptics/reward.ahap")
local ok, reason = vibrate.trigger({
    ios = {
        mode = "core_haptics",
        ahap_json = ahap,
        repeat = false,
    },
})
```

`ios.events` and `ios.ahap_json` are mutually exclusive. Custom events, AHAP,
and repeat require Core Haptics and supporting hardware. Semantic feedback uses
UIKit where available, while the legacy system vibration is the final fallback.
UIKit one-shot feedback cannot be cancelled after emission.

## HTML5 options

```lua
web = {
    pattern = { 40, 25, 80 },
    duration_ms = 100,
}
```

`web.pattern` overrides `web.duration_ms`; platform-specific Web values override
their common equivalents. The implementation uses `navigator.vibrate()` and
therefore supports timings only. Intensity and sharpness cannot be represented.
With `fallback = false`, explicitly requesting intensity, sharpness, or a
non-default semantic preset on Web returns `false, "unsupported"` instead of
silently discarding or approximating it.

Browser support is optional. A browser may require prior user activation,
reject vibration for a hidden page, shorten a long pattern, or suppress it due
to user/device settings. Use the returned status and
`vibrate.get_capabilities()` instead of assuming that HTML5 implies vibration
support.

```lua
function on_input(self, action_id, action)
    if action_id == hash("touch") and action.pressed then
        -- Calling from user input gives browsers the best chance of accepting it.
        vibrate.trigger({ web = { duration_ms = 50 } })
    end
end
```

## Cancellation and capabilities

### `vibrate.cancel()`

Returns `success, reason` using the same result convention as `trigger()`.
It cancels Android repeating waveforms, Web vibration, and active cancellable
Core Haptics playback. A completed one-shot or emitted UIKit feedback cannot be
recalled.

### `vibrate.is_supported()`

Returns a boolean indicating whether the current runtime reports usable
vibration or haptic feedback.

### `vibrate.get_capabilities()`

Returns a table with these fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `platform` | string | Active backend name. |
| `supported` | boolean | Any vibration/haptic output is available. |
| `pattern` | boolean | Timed patterns are available. |
| `intensity` | boolean | Intensity/amplitude control is available. |
| `sharpness` | boolean | Sharpness control is available. |
| `cancel` | boolean | Active effects can be cancelled. |
| `predefined` | boolean | Platform predefined effects are available. |
| `composition` | boolean | Primitive composition is available. |
| `repeat` | boolean | Repeating effects are available. |
| `core_haptics` | boolean | iOS Core Haptics is available. |
| `user_activation_required` | boolean | The runtime requires user activation, normally Web. |

Capabilities are runtime values and may vary between devices running the same
OS version.

```lua
local caps = vibrate.get_capabilities()
if caps.supported then
    print("Haptics backend:", caps.platform)
end
```
