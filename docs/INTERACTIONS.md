# Preserving the original interface

IndurUI carries the original robot interface's visual and interaction principles into an independent library. Its rounded panels, icon carousel, scrolling settings, tap/hold distinction, live measurements, and saved adjustments remain available. The hardware and robot algorithms belong to the application.

This table maps the original UI facilities to their general-purpose replacements. It describes UI behavior rather than copying private navigation or motor code into the library.

| Original facility | Library replacement |
| --- | --- |
| OLED text helper | `Canvas::text` and the `GfxCanvas` adapter |
| Boot logo and loading presentation | `splash`, `progress`, or a custom screen driven by the application's startup state |
| Rounded status boxes | `addStatus` compact fields, with optional editor sharing |
| Five live sensor bars | `addBar` plus Auto/Bars/Arc layouts, auto-sized and paginated |
| Sensor thresholds, min/max readings | Register arrays of values, bars, and numeric settings |
| Home button actions, short and long | `bindHomeButton` for all six labeled gestures; `onPress` for lower-level interception |
| Icon menu and current-item indicator | `addAction`, `addMenu`, `addSettings`, and `MenuStyle::Icons` |
| Select/cancel with held buttons | Two-button Back/Select mapping; optional center button |
| Held-action feedback | Default release-action overlay; `showHoldHints(false)` disables it |
| Continuous button adjustments | `ButtonFilter` repeat and three-button editor repeat |
| Stuck-low button detection | Debounced stuck timeout, `faultMask`, notification; no waiting for a stuck switch |
| Scrolling adjustment list | Three numbered two-line rows, filtered groups, sliding viewport, preserved selection |
| Scaled raw EEPROM tuning values | Variables in their engineering units, with numeric min/max/step/decimals |
| Counter adjustment | `addNumber` bound to a counter or setting variable |
| Special option lists | `addChoice`, including operating modes or application-specific options |
| Numeric parameter tuning with live context | `addContext` / settings-group context above the editor, plus `onChange` |
| Raw sensor and distance display pages | `addValue`/`addBar` or an `addScreen` callback |
| Encoder and motor diagnostics | Read-only bindings and application-owned custom pages/actions |
| Encoder-center/alignment tuning | Application-specific custom screen and numeric settings; no hardware assumptions in core |
| Debug toggle and feedback | `addToggle` or `addAction`, followed by `notify` |
| Low-battery warning | Application threshold check calling persistent `setAlert`, or a status widget |
| EEPROM refresh and saved parameters | Stable setting keys, `attachStorage`, `load`, `save`, and defaults |
| Clear stored application data | `confirm` before an application action; UI storage and application data remain independently owned |
| Maze solving, path runs, wall following | Application callbacks; the library renders the interface and receives events |
| Serial console and odometry work during UI | Keep them in the main loop/task, alongside `buttons.update` and `ui.tick` |

See the [original-style guide](ORIGINAL_STYLE.md) for native layouts and controls, including labeled home bindings, status/context placement, sensor arcs, custom value formats, and group navigation.

## Keep independent home gestures

Default navigation maps buttons onto four events. Applications that need all three buttons' independent short/long actions can intercept debounced gestures:

```cpp
bool robotButtons(uint8_t button, indur::Press press, void *) {
    if (ui.view() != indur::View::Dashboard) return false;
    if (button == 0 && press == indur::Press::Click) {
        requestManualMode();
        return true; // consumed; do not also dispatch Previous
    }
    if (button == 0 && press == indur::Press::Hold) {
        requestAutomaticMode();
        return true;
    }
    return false; // keep default behavior for the remaining gestures
}
// In setup(), before polling buttons:
buttons.onPress(robotButtons);
```

Button indices are 0 left, 1 right, and 2 center. `Press::Click`, `Hold`, and `Repeat` describe debounced physical gestures. A custom handler disables default release-action hints because the library cannot know the meaning of your overrides. Display your own feedback when appropriate. Remove interception with `buttons.onPress(nullptr)`.

The original blocking display and input loops become state transitions. Application jobs continue through the ordinary loop; screen transitions do not invoke motor routines automatically. Use callbacks that request work and return promptly.
