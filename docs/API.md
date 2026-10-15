# API reference

Include `<IndurUI.h>` for the independent C++11 core. All names live in `indur`. Register your widgets, settings, and menus in `setup()`, then check `begin()`. Run input polling and `tick(millis())` from the application loop. Arduino firmware must meet the [target policy](COMPATIBILITY.md); classic Nano/Uno and other small AVR targets are rejected at compile time.

## Ownership and IDs

```cpp
indur::UI ui(canvas, "My Robot");
```

The canvas must outlive the UI. UI objects cannot be copied. An `Id` is a `uint16_t` index in its registration category: **widget IDs, setting IDs, and menu IDs are separate namespaces**. Pass each ID to the matching method. `Invalid` means registration failed; `Root` is the parent of top-level menu entries. There is no item removal or reordering API; hide widgets or disable menu actions instead.

Registration allocates memory and copies metadata. Use it before `begin()`, and never register inside draw/change/action callbacks. With the current allocation strategy, setup repeatedly grows each metadata array; temporary registration memory includes both old and new arrays. Actual capacity depends on board RAM, data types, display framebuffer, and application use. Counts have a defensive upper bound of 65,000 per category, not a practical capacity promise.

## Bindings and lifetimes

Direct numeric bindings accept arithmetic types up to eight bytes, including `bool`, integer types, `float`, and `double`. A `const` numeric value works as a read-only dashboard binding. Editable settings require mutable references. `volatile` references can be displayed, but volatile does not make multi-byte reads atomic.

**Variables, label/unit strings, text buffers, choice arrays, icon data, callback contexts, and notification/modal titles are borrowed. They must remain valid while registered or displayed.** Use global/static storage or an object that outlives the UI. A temporary `String(...).c_str()`, a setup-local variable, or a local label buffer is not a valid long-lived binding. Labels are ordinary RAM-addressable C strings; AVR `F()`/flash strings are not accepted by this API.

If a value is written by an interrupt, another core, or a task, take a coherent snapshot in your application or provide a synchronized reader. Call UI methods from one task/context. Library storage and rendering are not thread-safe. Do not call them from an ISR.

Computed numeric signature:

```cpp
double batteryVoltage(void *context) {
    return static_cast<MyRobot *>(context)->batteryMillivolts() / 1000.0;
}
ui.addValue("Battery", batteryVoltage, &robot, "V", 2);
```

Text signature, useful for strings, objects, enums, or custom representations:

```cpp
void status(void *context, char *out, size_t capacity) {
    MyRobot *robot = static_cast<MyRobot *>(context);
    snprintf(out, capacity, "%s", robot->statusText());
}
ui.addText("Status", status, &robot);
```

Write at most `capacity - 1` characters and terminate with `\0`. The current widget text buffer is 48 bytes including its terminator; the library also terminates its last byte defensively. Text is read-only; implement application-specific string editing in a custom screen.

Integer dashboard values format directly from their original type, preserving even 64-bit integer text. Numeric editing, computed readers, and bar scaling use `double`; values beyond the board's exact floating-point integer precision should use read-only/custom text display. On AVR, `double` typically has float precision. Numeric decimal display is capped at six places, and very large/small floating values use scientific notation. The default backend uses fixed ASCII glyphs, not UTF-8 shaping.

## Dashboard registration

| Method | Purpose |
| --- | --- |
| `addValue(label, variable, unit="", decimals=1)` | Live arithmetic/bool value; bool renders ON/OFF |
| `addValue(label, NumberReader, context, unit="", decimals=1)` | Computed numeric value |
| `addBar(label, variable, min, max, unit="")` | Live bar, clamped to range; requires finite `max > min` |
| `addBar(label, NumberReader, context, min, max, unit="")` | Computed bar |
| `addText(label, TextReader, context=nullptr)` | Callback-rendered text |
| `addText(label, const char *liveText)` | Borrowed C string, reread every frame |
| `addStatus(label, variable, unit="", decimals=0)` | Compact status field on dashboards and in the editor |
| `addContext(label, variable, unit="", decimals=0, group=AllGroups)` | Live editor-only context, optionally restricted to a settings group |
| `setWidgetPlacement(widgetId, placement)` | Move a widget to Body, Status, Editor, or StatusAndEditor |
| `setWidgetGroup(widgetId, group)` | Assign a body group 0–254 |
| `setWidgetContextGroup(widgetId, settingsGroup)` | Restrict its editor context to a group; AllGroups removes filtering |
| `setWidgetVisible(widgetId, visible)` | Hide/show without removing registration; returns false for bad ID |
| `widgetCount()` | Registered count, including hidden widgets |
| `dashboardPages()` / `dashboardPage()` | Current page count / zero-based current page |

Register returns the category's `Id`, or `Invalid` and an error. No widget reference is rewritten by the dashboard.

### Automatic layout

The default Original style separates status, bar pages, and other value cards. `Auto` puts registered bars on sensor pages before value-card pages. Body groups can further separate independent sections. Explicit Cards, Bars, and Arc layouts are available for home and menu-accessible dashboards. See the [complete original-style and content guide](ORIGINAL_STYLE.md).

Card minimums remain 48×22. Columns use `width / max(24, minWidgetWidth)` and rows use available height with a minimum of one. Bars use `minBarWidth`, default 24, so five fit across 128 pixels. Values appear below vertical bars when a column is at least 32 pixels wide. A partial final page expands remaining cards. Status/context fields use `minStatusWidth` and at most two rows. Extra context pages rotate using `contextPageMs`. Below 48 pixels high, status fields join body pages and editor telemetry is omitted for a usable compact editor.

Long labels use ellipses when at least six characters fit; narrower labels retain their first characters. Numeric values that cannot fit show `--` rather than partial digits. Control/non-ASCII characters become `?`. Units show on sufficiently tall cards and in the settings editor. Custom screens own their layout and rendering. Choose `Theme::style = InterfaceStyle::Cards` with `DashboardLayout::Cards` to retain the earlier mixed-card presentation.

## Settings registration

| Method | Purpose |
| --- | --- |
| `addNumber(key, label, variable, min, max, step=1, unit="", decimals=0, group=0)` | Bounded numeric editor |
| `addToggle(key, label, boolVariable, group=0)` | OFF/ON editor |
| `addChoice(key, label, indexVariable, labels, count, group=0)` | Named options for an arithmetic index variable |
| `setReadOnly(settingId, readOnly)` | Show a setting but prevent editor entry |
| `setSettingFormatter(settingId, ValueFormatter, context=nullptr)` | Format live/pending values; null restores built-in formatting |
| `onChange(handler, context=nullptr)` | One callback for accepted edits, loaded records, and defaults |
| `edit(settingId)` | Open editor; returns false for missing/read-only setting |
| `selectedSetting()` | Selected setting ID, or `Invalid` when no matching setting |
| `pendingValue()` / `editing()` | Inspect editor value / active editor state |

Keys are nonempty, stable strings; their FNV-1a 32-bit hashes identify stored values. Duplicate keys or hash collisions in the same UI are rejected. The key text itself is not retained. Give each setting a descriptive unique key such as `drive.max_rpm`.

Groups are 0–254. `AllGroups` (255) selects every group and cannot be assigned to a setting. Choice count is 1–255; labels and their array must outlive the UI. Set index values to 0..count−1. Number bounds must fit the underlying variable type, and the initial value must be finite and within bounds. Integer settings require integral bounds and steps; use floating types for fractional increments.

The editor starts from a snapshot, clamps adjustments to min/max, and keeps a pending value. Save writes the variable and optionally storage; a failed persistent save restores the old variable and leaves the editor open. Cancel discards pending changes. A read-only setting reports `READ ONLY`. Settings autosave only after an accepted editor change; modifying a variable in your application requires explicit `save()` if you want to persist it.

Original-style settings use three two-line numbered rows at 128×64, with a sliding viewport and a bounded scroll indicator. The editor displays up to two rows of registered live context above the pending value. A formatter has signature `void(double value, char *out, size_t capacity, void *context)` and must respect the output capacity. It changes presentation without changing the stored value or adjustment step.

```cpp
void onChange(indur::Id setting, indur::Change reason, void *context);
```

`Change` is `Edited`, `Loaded`, or `Defaults`. A successful load applies all accepted values before firing callbacks. Callbacks may update derived configuration and navigate the UI. Do not register items, recursively load/save, or recursively dispatch events inside a callback. The UI does not enforce robot-specific safety states; use `setReadOnly`/`setEnabled` and application checks to decide when changes are allowed.

## Menu registration

| Method | Purpose |
| --- | --- |
| `addAction(label, Action, context=nullptr, parent=Root, icon=Icon())` | Execute `void callback(void*)` |
| `addMenu(label, parent=Root, icon=Icon())` | Create a submenu |
| `addSettings(label="Settings", group=AllGroups, parent=Root, icon=Icon())` | Open a settings group |
| `addScreen(label, ScreenDraw, ScreenEvent=nullptr, context=nullptr, parent=Root, icon=Icon())` | Custom drawing and optional input handler |
| `addDashboard(label, group, layout=DashboardLayout::Auto, parent=Root, icon=Icon())` | Menu entry for a filtered adaptive dashboard |
| `setIcon(menuId, icon)` | Replace icon; an empty `Icon()` clears it |
| `setEnabled(menuId, enabled)` | Disable selection while keeping the item visible |
| `setMenuStyle(MenuStyle::Icons / MenuStyle::List)` | Choose carousel or scrolling list |
| `menuCount()` | Total registered entries, including nested menus |
| `selectedMenuItem()` | Selected menu entry ID, or Invalid outside the menu |

Parents must be `Root` or an already registered submenu ID. No depth limit is hardcoded; hierarchy metadata and RAM set the practical limit. `begin()` adds a root Settings entry if settings exist and no explicit settings entry was added anywhere. Icons default to a numbered tile if no bitmap is set. On screens under 48 pixels high, menus use list layout even when `Icons` is selected.

```cpp
indur::Id tools = ui.addMenu("Tools");
ui.addSettings("Drive", 1, tools);
ui.addAction("Calibrate", calibrate, &robot, tools);
```

Custom callbacks:

```cpp
void draw(indur::Canvas &canvas, void *context);
bool event(indur::Event event, void *context);
```

Drawing starts with a cleared canvas; the callback owns the entire page and its layout. Return true to consume an event. An unconsumed `Back` returns to the menu. Your custom renderer must respect canvas bounds. It can use your own screen size, icon, graph, or project-specific tuning behavior without coupling the core library to the robot.

Returning from submenus, settings, custom screens, and grouped dashboards restores the originating menu item. Settings preserve their selected row after save/cancel. Directly opened settings and dashboards return home when they have no originating menu entry.

## Inputs

```cpp
#include <IndurUIButtons.h>
indur::Buttons buttons(ui, leftPin, rightPin, centerPin = -1,
                       indur::ButtonTiming(), true, true);
buttons.begin();
buttons.update(millis());
```

The last booleans select active-low and internal pull-ups. Active-high input uses `INPUT`, so provide an appropriate external pull-down where needed. Pins must be valid for your board. A negative center pin enables two-button mode. `begin()` sets input modes and calls `ui.setThreeButtons(...)` for the corresponding footer layout.

| Gesture | Two buttons | Three buttons |
| --- | --- | --- |
| Tap left / right | Previous / Next; decrement / increment in editor | Same |
| Hold left, release | Back / Cancel | Back outside editor; repeats decrement inside editor |
| Hold right, release | Select / Save | Select outside editor; repeats increment inside editor |
| Tap center | — | Select / Save |
| Hold center, release | — | Back / Cancel |

Click and hold events fire **after debounced release**. Repeats fire while held in the three-button editor, then suppress the release event. Default timing: debounce 25 ms, hold 350 ms, repeat 120 ms, stuck 5000 ms. Configure `ButtonTiming` fields before passing it to `Buttons`. `stuckMs = 0` disables the stuck timeout. Otherwise a stuck input is suppressed until release and a notification appears. `faultMask()` reports left/right/center bits 0/1/2. A held repeating edit also reaches this timeout; adjust it for your needs.

That table describes the default `InputStyle::Standard`. `setInputStyle(InputStyle::Original)` selects original-style physical controls: left tap increases and right tap decreases in the editor; holds stay Back/Save in both button counts, with no editing repeat. Menus still use left/right Previous/Next; optional center selects. On unbound home, right tap opens the menu and center tap changes page. `inputStyle()` reports the profile. See the [gesture and home-binding guide](ORIGINAL_STYLE.md#keep-the-original-gestures).

`ButtonFilter(timing).update(pressed, now, repeat=false)` is available independently and returns `Press::None`, `Click`, `Hold`, or `Repeat`. `stuck()` exposes its fault status; `held(now)` reports whether a stable, non-stuck press has crossed the hold threshold. Millisecond arithmetic supports the normal `millis()` rollover; provide monotonically advancing uint32_t timestamps and poll often enough to observe transitions. Do not use hold/debounce timing as a safety interlock.

While a navigation hold is active, a full-screen action caption provides release feedback. It uses Save, Back, Select, or the configured home gesture label. Disable it with `buttons.showHoldHints(false)`. Intercept physical gestures with `buttons.onPress(handler, context=nullptr)`: `bool handler(uint8_t button, Press press, void *context)` returns true to consume the gesture. Indices are left=0, right=1, center=2. Returning false uses the profile's mapping. A custom handler suppresses default hold hints.

`bindHomeButton(button, Press::Click/Hold, label, action, context=nullptr)` binds each of the six home gestures and supplies its footer/hold label. Null action clears a binding. These bindings apply to home only; grouped dashboards retain their own navigation. `handleButton(button, press)` exposes the same mapping to other input adapters, while `setButtonHeld(button, held)` controls the associated held-action feedback. Indices and press types are validated. `Press::Repeat` is ignored for center and in original-style editors.

Other input devices call `ui.handle(Event::Previous/Next/Select/Back)`. Semantic Next increases the editor value regardless of physical control profile. Use `shortcut(event, action, context=nullptr)` to override that event **only on home**; passing a null action restores normal behavior. `setThreeButtons(bool)` adjusts labels when using custom event sources.

## Runtime and navigation

| Method | Behavior |
| --- | --- |
| `begin()` | Validate accumulated registration error and canvas dimensions; auto-add Settings if needed |
| `tick(uint32_t now)` | Update timestamps, expire splash/notifications, render when dirty or refresh interval elapsed |
| `render()` | Force a frame after begin; does not advance time |
| `handle(Event)` | Dispatch an event after begin |
| `home()` | Return to page zero of the configured home group/layout |
| `setHomeGroup(group)` | Filter home body widgets; default AllGroups |
| `setDashboardLayout(layout)` | Set home layout; default Auto |
| `openDashboard(group, layout=DashboardLayout::Auto)` | Open a grouped dashboard; Back returns to its originating menu or home |
| `openMenu(parent=Root)` | Open root or a valid submenu, selecting its first item |
| `openSettings(group=AllGroups)` | Open filtered settings, selecting the first row |
| `view()` | Return the current `View` |
| `setTitle(const char*)` | Change dashboard title |
| `setTheme(const Theme&)` | Copy layout/refresh preferences |
| `notify(message, duration=1200)` | Timed overlay; does not block input or change view |
| `setAlert(message)` | Persistent dashboard warning above the footer; null clears it |
| `splash(title, icon=Icon(), duration=1000)` | Timed splash, then home; Back/Select skips it |
| `progress(title, fraction)` | Progress screen; finite fraction clamps to 0..1, NaN becomes 0 |
| `confirm(question, yesCallback, context=nullptr)` | YES/NO dialog, defaults NO; restores previous view before callback |

`View` values: `Dashboard`, `Menu`, `Settings`, `Editor`, `Custom`, `Confirmation`, `Progress`, `Splash`. Confirmation Left/Right toggles choice; Select accepts it; Back declines. Leaving progress with Back/Select returns home. Your application owns background work: leaving a progress view does not cancel a job. Finish it with `home()`, `openMenu()`, or your own next view.

Call `tick` with the current time before showing timed views if your application has not called it recently. Durations start from the UI's most recent tick timestamp. Do not pass durations spanning an entire uint32_t time cycle.

### Theme fields

| Field | Default | Meaning |
| --- | --- | --- |
| `style` | Original | InterfaceStyle::Original or Cards; independent of input style |
| `radius` | 3 | Rounded panel radius; GFX adapter clamps to rectangle size |
| `minWidgetWidth` | 48 | Desired minimum card width, clamped to at least 24 during layout |
| `minWidgetHeight` | 22 | Desired minimum card height, clamped to at least 22 during layout |
| `minBarWidth` | 24 | Minimum vertical sensor-column width |
| `minStatusWidth` | 40 | Minimum compact status/context field width |
| `statusRows` | 1 | One or two status rows, bounded to preserve body space |
| `contextPageMs` | 2500 | Editor context-page rotation period; zero keeps the first page |
| `iconSize` | 32 | Maximum icon slot size; also constrained to the screen |
| `refreshMs` | 100 | Refresh interval, clamped to at least 1 ms |

Layout dimensions and row counts are `uint8_t`; refresh and context-page periods are `uint16_t`. The timed splash includes a loading bar on screens at least 48 pixels high. Other application animations use custom screens and cooperative updates. More frequent refreshes consume more bus and CPU time.

## Display backends

`Canvas` supplies `width`, `height`, `clear`, `pixel`, `fillRect`, `rect`, `roundRect`, `text`, and `present`; see their virtual signatures in [IndurUI.h](../src/IndurUI.h). Coordinates use signed 16-bit pixels. Text uses a 6×8 character cell times scale, and color is monochrome bool. Default core layouts use sizes 1 and 2.

`GfxCanvas<Display>` calls Adafruit GFX-style primitives and `display.display()` to present. It selects the built-in font, disables wrapping, and uses monochrome color 0/1. Initialize your display and bus before calling `ui.begin()`. It does not allocate its own framebuffer.

Use a custom `Canvas` for another display library, transport, or framebuffer. Match the fixed text metrics or adapt them inside the backend; the core cannot measure a proportional font. Canvas dimensions must be 64×32 or greater and no greater than 1024×1024.

## Icons

```cpp
Icon(const uint8_t *data=nullptr, uint16_t width=0, uint16_t height=0,
     Memory memory=Memory::Flash);
bool Icon::valid() const;
bool Icon::pixel(uint16_t x, uint16_t y) const;
```

Bitmaps are monochrome, row-padded, MSB-first. White/1 means lit. Icon dimensions can be 1–256 per side; converter export is limited to 128 to keep generated assets practical. The bitmap allocation must contain `ceil(width/8) * height` bytes. The core cannot verify pointer length. Out-of-range pixel reads return false. `INDURUI_PROGMEM` and `Memory::Flash` use program-memory reads on AVR; ESP32 uses ordinary memory-mapped const data. Use `Memory::Ram` for a runtime buffer.

`<IndurUIIcons.h>` provides `icons::settings()`, `run()`, `sensors()`, `info()`, `tools()`, `storage()`, `robot()`, and `home()`, each 16×16. Include only where used. See [bitmap documentation](BITMAPS.md).

## Persistence methods

| Method | Behavior |
| --- | --- |
| `attachStorage(storage, offset=0, bytes=0, schema=1)` | Use a caller-owned region; 0 bytes selects the remaining capacity |
| `requiredStorageBytes()` | Minimum bytes for two copies of current settings |
| `load()` | Return `Loaded`, `Empty`, `Corrupt`, `IOError`, `NotAttached`, or `OutOfMemory` |
| `save()` | Validate and persist current settings; unchanged snapshot skips all writes/flushes |
| `resetDefaults(persist=true)` | Restore registration-time values; with attached storage, optionally persist; rollback on failed save |
| `setAutoSave(bool)` | Enable/disable automatic editor saves; default enabled |

Storage is optional. With no storage attached, editing and defaults operate in RAM; explicit `save()` fails with `StorageIO`. Without an attached backend, `load()` returns `NotAttached`. `resetDefaults(false)` does not erase existing saved records, and existing values will return on a later load. Read-only settings still participate in load/save/defaults.

`Storage` exposes `capacity() const`, `read(address, uint8_t&)`, `write(address, uint8_t)`, and `flush()`, each operation reporting success where applicable. Its lifetime must exceed the UI's storage use. Implement a backend for FRAM, emulation, or another byte-addressable device. The library accesses only the configured region; custom implementations must honor their own bounds and commit guarantees.

See [STORAGE.md](STORAGE.md) for exact format and failure handling.

## Errors

`error()` returns `None`, `OutOfMemory`, `InvalidArgument`, `DuplicateKey`, `StorageTooSmall`, `StorageIO`, or `CorruptData`. `errorText()` gives a static readable message; `clearError()` clears it. Failed registration returns `Invalid`. Bounds/ID setters return false for invalid IDs; not every false navigation/ID result creates a global error. Check return values for operations that can fail.

`begin()` refuses to start while a registration/configuration error remains. Fix the configuration rather than clearing a real setup failure. Successful load/save clears a prior storage error. Handle `LoadResult` explicitly: `Empty` leaves current values alone, and `Corrupt` preserves them for deliberate recovery. Storage adapters using APIs that do not report physical byte I/O failures cannot detect those failures; CRC verifies data on subsequent reads, while commit failures are reported when the backend supports them.
