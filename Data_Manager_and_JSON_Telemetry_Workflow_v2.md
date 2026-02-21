# ACMS Data Manager & JSON Telemetry –  Complete Architecture & Workflow

This document explains the **entire workflow**, **data structures**, **APIs**, and **JSON command/telemetry formats** used in the Data Manager + JSON Telemetry system for embedded targets (ESP32 / MCU-class systems).

---

## API & JSON QUICK REFERENCE

### Core Data APIs
```c
dm_system_init();
dm_set_value();
update_variable();
remove_from_list();
```

### Sync APIs
```c
sync_class(class_idx);
sync_all();
sync_all_nochange();
sync_all_classVars(class_idx);
```

### Telemetry APIs
```c

json_send();
json_receive();  
```
### Telemetry JSON (Outgoing)
```json
{
  "Sensor": [
    { "id": 1, "name": "Temp", "type": "float", "value": "26.75" },
    { "id": 2, "name": "Pressure", "type": "float", "value": "101.40" }
  ],
  "Power": [
    { "id": 5, "name": "Voltage", "type": "float", "value": "3.31" }
  ]
}
```

### Command JSON (Incoming)

#### Set Variable
```json
{
  "cmd": "set_var",
  "class": "Sensor",
  "name": "Temp",
  "type": "float",
  "value": 27.5
}
```

#### Update Variable
```json
{
  "cmd": "update_var",
  "class": "Sensor",
  "name": "Temp",
  "value": 28.0
}
```

#### Remove Variable
```json
{
  "cmd": "remove_var",
  "class": "Sensor",
  "name": "Temp"
}
```


---

## 1. Overview

This system manages **external variables** efficiently on MCUs by:
- Tracking variables in fixed pools
- Grouping them into classes
- Detecting changes in O(1)
- Syncing only what changed (or snapshotting everything)
- Exchanging data via JSON telemetry

---

## 2. Pools & Linking Model

The system has the following Configurable Instance Lengths in  Memory:
- No. of Class Instances - 32 
- No. of Variable Instances - 128
- No. of  Internal Varaible value storage - 64 (Appl. in Frontend Data Entry)
- No. of HashMap Key-ValuePairs - 2* Resp Instance

### class_pool[]
- One entry per logical class (Sensor, Power, System)
- Each class owns a doubly-linked list of variables
- `dirty` flag indicates pending sync

```
class_pool[MAX_CLASS]
used_class[MAX_CLASS]

Global Class Order (MRU → LRU)

[Class C] <-> [Class B] <-> [Class A]
```
Each `class_t` represents a logical group (Sensor, Power, System).

**Fields**
- `class_name`
- `class_idx` (stable, never reused)
- `dirty` (needs sync)
- `head` / `tail` → linked list of variables

>  **global control state**  includes **global  head  and tail  and last_idx of var_pool and class_pool **

### var_pool[]
- One entry per variable
- Holds cached value + pointer to real variable
- Linked inside its owning class

```
var_pool[MAX_VAR]
used_var[MAX_VAR]

Class "Sensor"

head (MRU)
  |
  v
[var3] <-> [var2] <-> [var1]
                          ^
                        tail (LRU)
```
Each `var_t` represents one tracked value.

**Fields**
- `var_name`
- `var_idx`
- `class_idx`
- `ext_addr` → pointer to real variable
- `cached_val`
- `prev` / `next` (per-class list)

### Rules
- New or updated variables → moved to **head**
- Oldest variables → at **tail**
- Sync walks **tail → head**

This gives:
- Priority ordering
- Efficient partial sync
- No array scans


---

## 3. Hashmaps (O(1) Lookup)

### Why Hashmaps?
Pools are indexed by numbers, but users work with names & pointers.

### Available Hashmaps

| Map        | Key         | Value     |
|     ---    |     ---     |   ---     |
| class_map | class_name | class_idx |
| var_map | (class_idx, var_name) | var_idx |
| addr_map | ext_addr | var_idx |

Used to avoid scanning pools.

---

## 4. Sync Functions Explained

### sync_class(class_idx)
- Syncs **only dirty variables**
- Walks from `tail → head`
- Moves tail during sync
- Clears `dirty` at end

Used for **incremental updates**.

---

### sync_all_classVars(class_idx)
- Snapshot-style sync
- Walks from `head → tail`
- **Does NOT modify** head or tail
- Does NOT depend on dirty flag

Used when:
- Client reconnects
- UI refresh needed
- Debug dump

---

### sync_all()
- Iterates all classes (global LRU → MRU)
- Calls `sync_class()` on dirty classes
- Normal operational mode

---

### sync_all_nochange()
- Iterates all classes
- Calls `sync_all_classVars()`
- Sends full snapshot regardless of dirty flags

---

### 🔍 Difference Summary

| Feature | sync_class | sync_all_classVars |
|---|---|---|
| Uses dirty flag | ✅ | ❌ |
| Moves tail | ✅ | ❌ |
| Partial update | ✅ | ❌ |
| Snapshot | ❌ | ✅ |
| Runtime cost | Low | Higher |

---

## 5. Telemetry JSON (Outgoing)

```json
{
  "Sensor": [
    { "id": 1, "name": "Temp", "type": "float", "value": "26.75" },
    { "id": 2, "name": "Pressure", "type": "float", "value": "101.40" }
  ],
  "Power": [
    { "id": 5, "name": "Voltage", "type": "float", "value": "3.31" }
  ]
}
```

---

## 6. Command JSON (Incoming)

### set_var
```json
{
  "cmd": "set_var",
  "class": "Sensor",
  "name": "Temp",
  "type": "float",
  "value": 27.5
}
```

### update_var
```json
{
  "cmd": "update_var",
  "class": "Sensor",
  "name": "Temp",
  "value": 28.0
}
```

### remove_var
```json
{
  "cmd": "remove_var",
  "class": "Sensor",
  "name": "Temp"
}
```

---

##  7. `data_manager.h` – Core APIs

### Initialization
```c
void dm_system_init(void);
```
Initializes pools, hashmaps, global state.

---

### Register / Set Variable
```c
bool dm_set_value(
    const char *class_name,
    const char *var_name,
    const char *type,
    void       *ext_addr,
    float       value
);
```

Used to:
- Create class if missing
- Create variable if missing
- Link variable into class
- Cache initial value

Called once during setup or on remote `set_var`.

---

### Fast Update (O(1))
```c
bool update_variable(void *ext_addr);
```

- Finds variable using `addr_map`
- Compares cached vs external value
- Moves variable to MRU if changed
- Marks class dirty

Safe to call in every loop.

---

### Remove Variable
```c
void remove_from_list(uint16_t class_idx, uint16_t var_idx);
```

Unlinks variable from class list and marks dirty.

---

### Sync One Class
```c
void sync_class(uint16_t class_idx);
```

Flow:
1. Check `dirty`
2. Walk from `tail → head`
3. Build JSON via `json_add_var`
4. Send JSON
5. Mark class clean

---

### Sync All
```c
void sync_all(void);
void sync_all_nochange(void);
```

- Iterates global class list
- Syncs dirty classes
- Snapshot mode supported

---

## 8. Telemetry Layer (`telemetry.cpp`)

### Responsibilities
- Build JSON
- Send JSON
- Receive commands
- Validate requests
- Call Data Manager APIs

---

### Sending JSON
```c
void json_add_var(uint16_t var_idx);
void json_send(void);
```

`json_add_var` appends:
```json
{ "id": 3, "name": "Voltage", "type": "float", "value": "3.31" }
```

Grouped automatically by class name.

---

## 9. Receiving Commands (`json_receive`)

### Remote Variable Pool
```c
#define MAX_REMOTE_VARS 64
static float remote_var_pool[MAX_REMOTE_VARS];
static bool  remote_var_used[MAX_REMOTE_VARS];
```

Used when server creates new variables.

---

### Supported Commands

#### `set_var`
- Allocates slot in `remote_var_pool`
- Calls `dm_set_value()`

#### `update_var`
- Validates class + var
- Updates external value
- Calls `update_variable()`
- Triggers sync

#### `remove_var`
- Validates identity
- Calls `remove_from_list()`
- Updates JSON and server

---

## 10. Integration with Other Programs

### Recommended Pattern

External program should:
1. Define real variables
2. Register once using `dm_set_value()`
3. Call `update_variable()` when values may change
4. Periodically call `sync_class()` or `sync_all()`


### Example
```c
float temperature;

dm_set_value("Sensor", "Temp", "float", &temperature, temperature);

loop() {
    temperature += 0.1;
    update_variable(&temperature);
    sync_class(sensor_idx);
    remove_variable(&temperature);  // <- args  can be   changed   laterif needed
    sync_class(sensor_idx); 
}
```

---

## 11. Complete API List

| API | Purpose |
|---|---|
| `dm_system_init` | Initialize system |
| `dm_set_value` | Register variable |
| `update_variable` | Fast update |
| `remove_from_list` | Delete variable |
| `sync_class` | Sync one class |
| `sync_all_classVars` | Sync all Vars  in one class without staging and changes |
| `sync_all(_nochange)` | Sync all |
| `json_send` | Send telemetry |  <--  called internally  during sync
| `json_receive` | Receive commands |

---


## 12.. How to Use API (Arduino / ESP32)

### setup()
```c
dm_system_init();
dm_set_value("Sensor", "Temp", "float", &temperature, temperature);
```

### loop()
```c
update_variable(&temperature);
sync_class(sensor_idx);
```

### Snapshot / UI refresh
```c
sync_all_nochange();
```

---

## 13. Integration Notes

- External program owns real variables
- All variables can exist in a memory  blob/pool 
- External/sensor/modbus data is  assigned a pool_idx for storing
- All  Varaibles are actually stored as float
- Floats accomodate  floats/int/bool values  using a single  type
- Character variable support  currently not present
- The  Variables are passed through with their type
- Variabes can be processed in front-end differently based on  their type
- Pass addresses once
- Never manipulate pools directly

---


## 14. Guarantees

- No dynamic allocation
- Deterministic RAM
- O(1) critical paths
- MCU safe design
- Designed for real-time systems
- Server-friendly JSON format

---

📌 This architecture is robust, scalable, and production-ready for embedded telemetry systems.
