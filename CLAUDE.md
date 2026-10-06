# Contributing guide and coding rules

These rules exist so the firmware stays readable and safe to change by anyone, including
contributors who have never seen the hardware. They apply to humans and to AI assistants alike.

## Language

**Everything in this repository is written in English**: identifiers, comments, commit messages,
configuration keys, on-screen text and documentation. No exceptions.

## Layering

```
app        main, tasks, mode switching
ui / web / profile
input / transport
hal        the only code that touches registers, pins or peripherals
shared/OmniRCProtocol
```

A layer may call downwards. It must never call upwards, and it must never reach into another
module's internal headers.

`lib/ctl_input`, `lib/ctl_profile` and `shared/OmniRCProtocol` must not include `Arduino.h`
or any `esp_*` header. That restriction is what keeps them testable on the host, and it is
checked by the fact that they compile in the `native` environment.

## Language rules

- C++17. Exceptions and RTTI are disabled.
- No dynamic allocation after `setup()` on any hot path. LVGL uses its own static pool.
- Every fallible function returns `ctl::Result<T>` or `ctl::Status`. Nothing fails silently, and
  no error is discarded without a comment saying why it is safe to ignore.
- Prefer fixed-size buffers and `constexpr` bounds over runtime growth.
- `static_assert` anything that must hold at compile time, especially wire format sizes.

## Naming

| Kind | Style | Example |
|---|---|---|
| Types | `PascalCase` | `ControlFrame`, `RadioManager` |
| Functions, variables | `camelCase` | `encodeControl`, `channelCount` |
| Member variables | `m_camelCase` | `m_windowLost` |
| Constants, `constexpr` | `kPascalCase` | `kMaxChannels` |
| Macros | `CTL_UPPER_SNAKE` | `CTL_LOGI` |
| Namespaces | `lowercase` | `ctl`, `omnirc` |

Firmware code lives in namespace `ctl`, short for control. The shared protocol lives in
namespace `omnirc`, because receivers pull it into projects that know nothing about this
firmware and need a name they can recognise.

## Comments

Comment the *why*, not the *what*. A comment that restates the code is noise; a comment that
records a hardware quirk, a safety decision or a non-obvious trade-off is the most valuable text
in the file. Public headers document behaviour and contracts, including what happens on failure.

## Tests

Anything that does not touch hardware gets a host test in `test/`. That covers the protocol,
sequence tracking, calibration curves, channel mapping and profile validation. Tests must not
depend on wall clock timing; pass time in explicitly instead.

```bash
pio test -e native
```

## Formatting

`clang-format` with the repository's `.clang-format`, pinned to the version used in CI:

```bash
pip install clang-format==23.1.1
find src lib shared test -type f \( -name '*.cpp' -o -name '*.h' \) -print0 \
  | xargs -0 clang-format -i
```

Bumping the pinned version and reformatting the tree must happen in the same commit.

## Safety rules that are not negotiable

1. A decode failure never produces partially parsed data. Stale values are safer than invented
   ones on a control link.
2. Exactly one transport may be active at a time. Switching profiles fully shuts down the
   previous transport's stack rather than leaving it idle.
3. The firmware must never let total current draw fall below 80 mA while powered on. The power
   module cuts its output below 50 mA, so a deeper idle state would look like a random shutdown.
4. Battery protection is enforced in firmware: warn at 3.5 V, force a clean shutdown at 3.3 V.
5. Every change to the wire format bumps `kProtocolVersion`. Receivers reject frames from a
   version they do not understand.

## Adding things

- **A transport**: implement `ITransport` and register it in the transport factory. Nothing else
  should need to change.
- **A screen**: add one file under `lib/ctl_ui`. Screens do not talk to hardware directly.
- **An input source**: extend the source catalogue in `lib/ctl_input` and document it in
  `docs/PROFILES.md`. The catalogue is part of the profile format, so removing a source is a
  breaking change.
