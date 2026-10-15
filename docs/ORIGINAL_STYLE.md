# The original interface, made reusable

IndurUI 1.2 restores the original robot's information hierarchy: compact live status at the top, sensor content in the middle, and consistent button labels at the bottom. Settings use three numbered rows on a 128×64 OLED. Their editor keeps live context above a framed pending value. The implementation remains cooperative and hardware independent.

## Build an organized interface

```cpp
// The variables remain owned by your application.
ui.addStatus("L", leftPosition);
ui.addStatus("SP", speed);
ui.addStatus("R", rightPosition);

for (uint8_t i = 0; i < sensorCount; ++i)
    ui.addBar(sensorNames[i], sensorValues[i], 0, sensorMaximum);

// Context appears while editing settings group 1.
ui.addContext("Kp", gain, "", 1, 1);
ui.addContext("Kd", damping, "", 1, 1);
ui.addContext("Err", error, "", 0, 1);
ui.addNumber("control.gain", "Controller gain", gain, 0, 20, 0.1, "", 1, 1);
```

`addStatus` creates a widget in `StatusAndEditor` placement. `addContext` creates an editor-only widget; its optional final argument selects a settings group. Omit that group to show the context in every editor. All existing number/text/computed widgets can also be placed with `setWidgetPlacement` and filtered with `setWidgetContextGroup`.

Keep status labels short. The renderer reserves space for the whole value first, shortening the label when needed. It displays `--` rather than partial numeric digits. Units are kept on full cards and setting editors; compact strip fields show label and value.

The original style is the visual default. To select it explicitly:

```cpp
indur::Theme theme;
theme.style = indur::InterfaceStyle::Original;
theme.minBarWidth = 24;       // Up to five sensor bars across 128 pixels.
theme.minStatusWidth = 40;    // Three compact status fields across 128 pixels.
theme.statusRows = 1;         // One or two rows, when the screen has room.
theme.minWidgetWidth = 64;    // Two wider numeric cards per page.
theme.contextPageMs = 2500;   // Rotate extra editor context pages every 2.5 s.
ui.setTheme(theme);
```

`InterfaceStyle::Cards` preserves the earlier card-based dashboard and settings/editor rendering. The two styles share bindings, menus, settings, callbacks, and storage. Styling does not remove registered features or data.

## Separate content into pages and sections

Widgets start in body group 0. Group IDs are 0–254; `AllGroups` selects all groups. Grouping does not change ownership or storage.

```cpp
ui.setHomeGroup(0); // Keep the home screen focused on the sensors.
auto temperatureId = ui.addValue("Temperature", temperature, "C", 1);
ui.setWidgetGroup(temperatureId, 1);
auto tools = ui.addMenu("Tools", indur::Root, indur::icons::tools());
ui.addDashboard("Sensor readings", 0, indur::DashboardLayout::Arc,
                tools, indur::icons::sensors());
ui.addDashboard("System data", 1, indur::DashboardLayout::Cards,
                tools, indur::icons::info());
ui.addSettings("Controller", 1, tools, indur::icons::settings());
```

Status fields remain global across ordinary dashboards. Context fields are independent of body groups. `setWidgetVisible` can hide any placement. A grouped dashboard reached through a menu returns to its own menu item. A dashboard opened directly returns home. Single-page grouped dashboards return on any navigation event, matching the original diagnostic screens; multi-page dashboards use left/right paging and hold-left/center Back.

| Layout | Content arrangement |
| --- | --- |
| `Auto` | Original style puts bar widgets on sensor pages and other body widgets on separate card pages; Cards style uses the earlier mixed grid |
| `Cards` | All matching body widgets use adaptive cards in registration order |
| `Bars` | A row of vertical readings; registered bars use their explicit ranges, other widgets show label/value |
| `Arc` | Staggered labels and values across a sensor-shaped arc; paginates extra readings |

Use `setDashboardLayout(layout)` for home, or `openDashboard(group, layout)` for direct navigation. `dashboardPages()` and `dashboardPage()` describe the current dashboard. `home()` returns to the first page of the configured home group/layout.

There is no fixed five-sensor limit: the available width and minimum bar width determine each page's capacity. More sensors produce more pages. Narrow bars prioritize their label and shape; numeric values appear from 32 pixels per column. The Arc page shows full readings in staggered positions. Additional status fields also page; the status strip and body repeat independently until every page has been shown. Editor context pages rotate by `contextPageMs`; zero keeps the first context page.

On displays below 48 pixels high, status fields join the ordinary body pages and editors use the compact layout. The two-row telemetry editor needs a taller screen. The classic Nano/Uno memory restriction still applies; automatic layout does not eliminate metadata or framebuffer RAM.

## Keep the original gestures

Visual style and input style are independent. The default input style remains `Standard` for existing applications. Select the original control direction explicitly:

```cpp
ui.setInputStyle(indur::InputStyle::Original);
```

| Gesture | Original profile, two or three buttons |
| --- | --- |
| Menu/settings tap left / right | Previous / next |
| Editor tap left / right | Increase / decrease |
| Hold left, release | Back / discard edit |
| Hold right, release | Select / save edit |
| Center tap, when present | Select/save; on an unbound home screen, next dashboard page |
| Center hold, release | Back/cancel |
| Unbound home right tap | Open menu |

Holds remain save/back gestures in the editor even with three buttons. The Standard profile retains left-decrease/right-increase and three-button repeating edits. Hold feedback clears the screen and shows the context's action, then restores the interface after release. There is no blocking input wait loop. `showHoldHints(false)` disables that feedback.

## Assign all six home gestures

```cpp
void openTools(void *) { ui.openMenu(); }
void openData(void *) { ui.openDashboard(1, indur::DashboardLayout::Cards); }
void requestWork(void *) { workRequested = true; }

// Button indices: 0 left, 1 right, 2 center.
ui.bindHomeButton(1, indur::Press::Click, "MENU", openTools);
ui.bindHomeButton(1, indur::Press::Hold, "RUN", requestWork);
ui.bindHomeButton(2, indur::Press::Click, "DATA", openData);
```

Each physical button has an independent Click and Hold binding. Tap labels appear in their actual left/center/right footer positions. Held-action feedback uses the corresponding hold label. Bindings apply only to home, not to menu entries or grouped diagnostic pages. Two-button hardware never polls center bindings. A null action removes a binding. Return to default navigation for gestures you have not assigned. If you assign every navigation gesture, provide another way to reach additional home pages, such as a dashboard menu entry.

`Buttons::onPress` remains available for lower-level interception. `UI::handleButton` applies the same profile and home bindings for other physical-input adapters. `UI::handle(Event)` retains semantic Previous/Next/Select/Back behavior; in an editor Next always increases, regardless of physical button direction. `shortcut` overrides semantic events on home only.

## Tuning formats and warnings

Custom setting formatting receives the live or pending value, so engineering-unit display does not require raw EEPROM arithmetic in the UI:

```cpp
void gridSize(double value, char *out, size_t capacity, void *) {
    snprintf(out, capacity, "%dx%d", int(value), int(value));
}
ui.setSettingFormatter(gridSettingId, gridSize);
```

The formatter changes presentation; bounds, typed values, pending edits, save/cancel, and storage remain unchanged. Pass null to restore the built-in format. Formatter contexts and home labels are borrowed and must outlive their use.

`setAlert("LOW BATTERY")` shows a persistent home/dashboard warning above the footer; `setAlert(nullptr)` clears it. Timed notifications remain available separately. Warning detection and response belong to the application.

The library supplies UI building blocks for sensor calibration, encoder-center adjustment, motor diagnostics, counters, and operating modes. Register the actual project's values and actions. The core does not command motors, assign calibration values, or read device-specific registers on its own. See [MiniRobotTest](MINI_ROBOT_TEST.md) for a complete wired example with neutral motor outputs and isolated demo storage.
