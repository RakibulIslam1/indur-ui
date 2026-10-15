# Persistent settings

The UI's storage layer is independent of your robot's map, telemetry, calibration history, or application state. Choose an exclusive region and register only the settings you want persisted. Values are stored in their native numeric types, without the original robot's byte scaling conventions.

## Setup order

1. Initialize your board's EEPROM implementation if it requires it. ESP32 uses `EEPROM.begin(size)`; classic AVR EEPROM does not.
2. Construct an `EEPROMStorage<decltype(EEPROM)>` adapter with the initialized device's usable capacity.
3. Register settings with stable keys, valid initial values, and bounds. Those initial values become defaults.
4. Initialize the display/buttons and check `ui.begin()`.
5. Check `ui.attachStorage(adapter, offset, bytes, schema)`.
6. Call `ui.load()` and handle its result.

The [complete example](../examples/PersistentSettings/PersistentSettings.ino) reserves bytes 64–575. Application bytes outside that region remain untouched by UI reads and writes. The EEPROM implementation's own flash-sector operations are a separate lower-level concern.

`bytes = 0` uses all capacity after `offset`. Regions split evenly into two banks; an odd final byte is unused. Maximum bank size is 65,535 bytes. The minimum for your registrations is:

```text
requiredStorageBytes = 2 × (20 + Σ(6 + sizeof(setting variable)))
```

Example: one 4-byte float, one 2-byte integer, and one 1-byte bool need `2 × (20 + 10 + 8 + 7) = 90` bytes. Always ask `requiredStorageBytes()` because type sizes vary by board.

## Load results

| Result | Effect and application response |
| --- | --- |
| `Loaded` | Newest valid matching-schema bank applied; unknown or incompatible records ignored |
| `Empty` | Neither bank has the recognized magic; existing live/default values unchanged; explicitly save if desired |
| `Corrupt` | Recognized records exist but no bank validates, including schema mismatch; keep current values and offer deliberate reset |
| `IOError` | Storage read failed; no staged setting values applied |
| `NotAttached` | Attach a storage backend first |
| `OutOfMemory` | Staging allocation failed; no setting values applied |

The loader validates bank metadata, payload bounds, record structure, schema, and CRC32 before staging recognized settings. Matching records must have the expected key hash, numeric type, and byte width. Nonfinite or out-of-range values and invalid bool bytes are ignored, leaving that variable unchanged. All staged values are applied together before change callbacks run. This is a single-threaded application operation, not an atomic cross-task transaction.

A valid older bank is used when the newer bank is damaged. An actual reported I/O failure is returned conservatively even when another bank appears valid. If neither bank's magic is recognizable, the result is `Empty`; a random or partially initialized region is not automatically overwritten by `load()`.

## Save behavior

`save()` checks configured bounds, captures a snapshot of all registered settings, and compares it with the newest valid bank. An identical snapshot returns success without writes or flushes. Otherwise it:

1. Selects the inactive bank and invalidates its magic.
2. Writes metadata, the setting records, and their CRC32.
3. Flushes the incomplete bank.
4. Writes valid magic last and flushes again.

The previous bank remains available throughout byte-level writes. Partial magic, incomplete records, and bad CRCs are rejected on the next load. If the final flush reports failure, the library attempts to invalidate the new bank again. Backend failures still require application recovery; no software can promise successful repair of a device that refuses writes.

After an accepted editor change, autosave is enabled by default. If it fails, the edited project variable is restored and the editor remains open. `setAutoSave(false)` permits batching changes, followed by explicit `save()`. Application-originated variable changes do not trigger automatic saves.

`resetDefaults(true)` restores values captured at registration and saves them if a backend is attached. A failed persistent reset restores the prior live values. `resetDefaults(false)` changes RAM only and does not erase the existing journal.

## EEPROM adapters and endurance

`EEPROMStorage` checks address bounds and skips writes of unchanged bytes. It caps the configured capacity at `length()` when the device provides it. `flush()` calls a bool-returning `commit()`, a void `commit()`, or a no-op for devices such as AVR EEPROM without commit. The application retains ownership of initialization and device lifetime.

Some EEPROM APIs provide no physical byte read/write error status. The adapter cannot invent that status; commit errors are returned where supported, and CRC detects later data inconsistency. A backend that can detect byte I/O errors should implement `Storage` directly.

Avoid saving in every loop iteration or sensor update. The journal alternates two banks and suppresses unchanged snapshots; it is **not general wear leveling**. Device endurance, commit granularity, and application update frequency determine longevity.

On flash-emulated EEPROM, both banks can share a physical erase sector. The journal does not protect both copies from loss during a shared sector erase. Power-loss resilience depends on the underlying medium and backend. Use a storage arrangement with the durability guarantees your application requires, and test actual power interruptions on the target hardware.

## Schema changes and migration

Stable setting keys allow registration order changes and newly added settings without positional dependence. New settings retain their current/default values when no compatible record exists. Removed settings are ignored on load; they disappear from the next changed save.

Values use **native byte representation**. Record headers are little-endian, but settings are not a cross-platform interchange format. Changing CPU endianness, numeric representation, variable widths, or the meaning of keys requires a migration strategy. In particular, AVR and ESP32 `double` widths may differ.

Use a new schema number when the meaning/layout contract changes. A mismatching recognized bank is rejected as corrupt for that schema; it is not silently reinterpreted. Offer a confirmed default reset or write application-specific migration code. Increasing the region size or changing the offset also changes bank locations; migrate explicitly rather than expecting automatic discovery.

An FNV-1a hash identifies each key. Duplicate hashes in one UI are rejected, but the storage format does not retain the full original strings. Keep keys unique and stable; bump schema if a key is repurposed.

The old robot EEPROM addresses and scaled byte values are not automatically imported into this independent library. Read them in application code and convert to engineering units if you deliberately migrate an existing robot.

## Journal format, version 1

Each bank starts with a 20-byte header:

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 4 | Magic `49 55 49 01` (`IUI`, format version 1) |
| 4 | 4 | Generation counter, little-endian uint32 |
| 8 | 2 | Payload byte count, little-endian uint16 |
| 10 | 2 | Record count, little-endian uint16 |
| 12 | 4 | Application schema, little-endian uint32 |
| 16 | 4 | CRC32 over header bytes 4–15 followed by payload |

Each record is key hash (4 bytes), numeric kind (1), native width (1), and native value bytes (1–8). Kinds: signed integer=1, unsigned integer=2, floating point=3, bool=4. CRC uses the reflected IEEE polynomial `0xEDB88320`, initial value and final XOR `0xFFFFFFFF`. Generations compare with wrap-aware signed subtraction.

## Custom backend

```cpp
class MyStorage : public indur::Storage {
public:
    size_t capacity() const override;
    bool read(size_t address, uint8_t &value) override;
    bool write(size_t address, uint8_t value) override;
    bool flush() override;
};
```

Return false for failed operations and out-of-range addresses. Reads should observe prior successful writes in the current session; `flush()` commits pending data according to the device's contract. Respect application-owned regions and take synchronization responsibility in the adapter. The UI's journal cannot compensate for undocumented backend behavior.

The Arduino [EEPROM documentation source](https://github.com/arduino/docs-content/blob/main/content/learn/07.built-in-libraries/03.eeprom/eeprom.md) explains the standard API. Check your board core's implementation for initialization, capacity, and commit behavior; an Arduino-compatible API does not imply identical physical storage.
