# INIRW

A single-header C++17 INI reader/writer that keeps your file's **order**, **comments** and **formatting** intact when you save.

- Header-only, standard library only
- Sections and keys keep insertion order (built on its own `oMap`)
- `write()` edits the file **in place**: comments, blank lines, spacing, BOM and line endings survive
- Atomic saves (temp file + rename), so a failed write leaves your original untouched
- Never throws on bad input; every non-blank, non-comment line is accepted as *something*

> **Read this before using it:** INIRW has a few deliberate quirks (empty keys, mandatory backslash escaping, `write()` only writing what you hand it). They're all documented below, with examples.

---

## Contents

1. [Requirements and installation](#requirements-and-installation)
2. [Quick start](#quick-start)
3. [Data model](#data-model)
4. [API](#api)
5. [Updating a file: read, modify, write](#updating-a-file-read-modify-write)
6. [Design quirks](#design-quirks)
   - [How each line is parsed](#how-each-line-is-parsed)
   - [Empty keys and value-less keys](#empty-keys-and-value-less-keys)
   - [Escaping: every backslash counts](#escaping-every-backslash-counts)
   - [Comments](#comments)
   - [Sections](#sections)
   - [Whitespace](#whitespace)
   - [Duplicates](#duplicates)
7. [What `write()` escapes](#what-write-escapes)
8. [What `write()` preserves](#what-write-preserves)
9. [Error handling](#error-handling)
10. [`oMap` reference](#omap-reference)
11. [Utility functions](#utility-functions)
12. [Limitations](#limitations)
13. [License](#license)

---

## Requirements and installation

- A C++17 compiler (uses `std::string_view` and `std::filesystem`)
- Copy `INIRW.h` into your project and include it:

```cpp
#include "INIRW.h"
```

Everything lives in the `INIRW` namespace.

---

## Quick start

```cpp
#include "INIRW.h"
#include <iostream>
#include <string>

int main() {
    INIRW::Ini ini;
    INIRW::read("config.ini", ini);          // 1. load the whole file

    // Read a value safely (contains() never inserts anything)
    if (ini.contains("Display") && ini["Display"].contains("width")) {
        int width = std::stoi(ini["Display"]["width"]);
        std::cout << "width = " << width << "\n";
    }

    // Modify
    ini["Display"]["width"] = "1920";
    ini["Audio"]["volume"] = "80";

    INIRW::write("config.ini", ini);         // 2. save everything back
    return 0;
}
```

---

## Data model

```cpp
using Ini      = INIRW::oMap<std::string, INIRW::oMap<std::string, std::string>>;
using SectData = INIRW::oMap<std::string, std::string>;
```

- An `Ini` maps **section name → `SectData`**; a `SectData` maps **key → value**.
- Every value is a `std::string`. There is no int/bool/float parsing, so convert it yourself (`std::stoi`, etc.).
- Section names and keys are **case-sensitive**.
- Order is preserved: sections and keys appear in the order they were first seen (or first inserted).
- Keys that appear **before any `[section]` header** live in the unnamed section, whose name is the empty string `""`:

```cpp
ini[""]["name"] = "Hero";   // global key, written without a [header]
```

### Iterating

```cpp
for (const INIRW::Ini::Entry& section : ini) {
    std::cout << "[" << section.first << "]\n";
    for (const INIRW::SectData::Entry& kv : section.second) {
        std::cout << kv.first << " = " << kv.second << "\n";
    }
}
```

### `operator[]` inserts

Like `std::map`, `ini["A"]["B"]` **creates** section `A` and key `B` (with an empty value) if they don't exist, and they will then be written out by `write()`. To look something up without creating it, use `contains()` or `find()`:

```cpp
INIRW::Ini::EntryIterator sec = ini.find("Display");
if (sec != ini.end()) {
    INIRW::SectData::EntryIterator it = sec->second.find("width");
    if (it != sec->second.end()) {
        std::string width = it->second;
    }
}
```

`operator[]` is not available on a `const Ini`; use `find()` there.

---

## API

```cpp
void INIRW::read(std::string_view file, INIRW::Ini& output);
void INIRW::write(std::string_view file, const INIRW::Ini& data);
```

| Function | What it does |
|---|---|
| `read` | Clears `output`, then parses `file` into it. If the file can't be opened, `output` is left empty and a message is printed to `std::cerr`. |
| `write` | Makes the file match `data` (see the next section). Creates the file if it doesn't exist. Your `data` object is **not modified** (it works on an internal copy). |

Neither function returns a status or throws on I/O problems; see [Error handling](#error-handling).

---

## Updating a file: read, modify, write

> **`write()` only writes what you give it.**
> Anything in the file that is not in the `Ini` you pass is **removed**.

If you just want to change one value, you must first `read()` the file into an `Ini`, change what you need, and then `write()` that whole `Ini` back.

### Wrong: writing a partial `Ini`

Starting file:

```ini
; Game settings
[Display]
width = 1280   ; pixels
height = 720
fullscreen = true

# audio stuff
[Audio]
volume = 80
```

```cpp
INIRW::Ini ini;
ini["Display"]["width"] = "1920";      // only this
INIRW::write("config.ini", ini);       // everything else is dropped!
```

Result:

```ini
; Game settings
[Display]
width = 1920   ; pixels

# audio stuff
```

`height`, `fullscreen`, `[Audio]` and `volume` are gone. (Comments and blank lines are always kept, even orphaned ones; see [What `write()` preserves](#what-write-preserves).)

### Right: read, modify, write

```cpp
INIRW::Ini ini;
INIRW::read("config.ini", ini);        // load everything first
ini["Display"]["width"] = "1920";      // change one value
INIRW::write("config.ini", ini);       // write it all back
```

Result:

```ini
; Game settings
[Display]
width = 1920   ; pixels
height = 720
fullscreen = true

# audio stuff
[Audio]
volume = 80
```

Only the `width` line changed, and its `; pixels` comment and spacing are intact. Untouched lines are left byte-for-byte as they were.

### Adding keys and sections

```cpp
INIRW::Ini ini;
INIRW::read("config.ini", ini);
ini["Display"]["vsync"] = "on";         // new key in an existing section
ini["Controls"]["jump"] = "space";      // brand new section
INIRW::write("config.ini", ini);
```

```ini
; Game settings
[Display]
width = 1280   ; pixels
height = 720
fullscreen = true
vsync = on

# audio stuff
[Audio]
volume = 80
[Controls]
jump = space
```

- A new key goes **right after the last key** of its section (before that section's trailing blank lines/comments).
- A new section is **appended to the end** of the file. No blank line is inserted before it.
- New global keys (section `""`) go at the **very top** of the file.

### Removing keys and sections

```cpp
INIRW::Ini ini;
INIRW::read("config.ini", ini);
ini["Display"].erase("fullscreen");   // remove one key
ini.erase("Audio");                   // remove a whole section
INIRW::write("config.ini", ini);
```

### What `write()` does, line by line

| In the file | In the `Ini` you pass | Result |
|---|---|---|
| key exists, same value | key present, same value | line untouched |
| key exists, different value | key present, new value | only the value is replaced; spacing and inline comment kept |
| key exists | key **missing** | line removed |
| key missing | key present | added to the end of its section |
| `[section]` exists | section **missing** | header and its keys removed |
| `[section]` missing | section present | appended at the end of the file |
| blank line or comment line | n/a | **always kept** |

### Other write rules

- **Writing an empty `Ini` does nothing.** It prints `Valid write data is required to write!` and leaves the file alone. You can't blank a file with `write()`; delete it with `std::filesystem::remove` instead.
- A section that is present but has no keys is fine: its header is kept/written and its old keys are removed.

---

## Design quirks

### How each line is parsed

The parser never rejects a line. After trimming whitespace and cutting off any unescaped comment, each line becomes exactly one of: nothing (blank/comment), a section header, or a key/value pair.

| Line in the file | Key | Value |
|---|---|---|
| `name = Bob` | `name` | `Bob` |
| `name=Bob` | `name` | `Bob` |
| `=yo` | `` (empty key) | `yo` |
| `lonely` | `lonely` | `` (empty) |
| `key =` | `key` | `` (empty) |
| `a = b = c` | `a` | `b = c` |
| `msg = hi ; note` | `msg` | `hi` |
| `color = #ff0000` | `color` | `` (the `#` started a comment) |
| `color = \#ff0000` | `color` | `#ff0000` |
| `path = C:\Games` | `path` | `C:Games` (lone backslash vanishes) |
| `path = C:\\Games` | `path` | `C:\Games` |
| `a\=b = 1` | `a=b` | `1` |
| `[oops` (no closing `]`) | `[oops` | `` (empty) |
| `name = "Bob"` | `name` | `"Bob"` (quotes are kept) |

### Empty keys and value-less keys

- **A line with no `=` is a key with an empty value.** `lonely` gives `ini[section]["lonely"] == ""`. Handy for flag-style entries.
- **Text before the first `=` can be empty, giving an empty key.** `=yo` gives `ini[section][""] == "yo"`.
- Only the **first** unescaped `=` splits the line. Everything after it, including more `=` signs, is the value.

```ini
=yo
lonely
equation = 1 + 1 = 2
```

```cpp
ini[""][""]          // "yo"
ini[""]["lonely"]    // ""
ini[""]["equation"]  // "1 + 1 = 2"
```

Both forms round-trip through `write()`: an empty key is written as ` = yo`, and a value-less key as `lonely = `.

If you write a non-empty value for a key that exists in the file *without* an `=`, the `=` is added for you: `lonely` becomes `lonely = something`.

### Escaping: every backslash counts

The backslash `\` is the escape character in **section names, keys and values**. **Every** backslash in the file consumes itself and makes the next character literal. There are no escape sequences like `\n` or `\t`: `\n` is simply the letter `n`.

That means **every literal backslash in your file must be written as `\\`**:

| In the file | You get in memory |
|---|---|
| `C:\\Games` | `C:\Games` |
| `C:\Games` | `C:Games` |
| `\\\\server\\share` | `\\server\share` |
| `\;` and `\#` | `;` and `#` |
| `\n` | `n` |
| `dir = oops\` (backslash at the end) | `oops` (it's dropped) |

Two separate layers are involved, so don't mix them up:

- **In memory** the values are raw. You never escape them for INIRW; the library escapes when it writes and unescapes when it reads.
- **In the file** they are escaped.
- **In your C++ source** you additionally escape for the compiler.

```cpp
ini["Paths"]["gta"] = "C:\\Games\\GTA San Andreas";  // compiler turns this into: C:\Games\GTA San Andreas
INIRW::write("paths.ini", ini);
```

File produced:

```ini
[Paths]
gta = C:\\Games\\GTA San Andreas
```

Reading it back gives `C:\Games\GTA San Andreas` again.

> **Hand-edited files:** if someone types `path = C:\Games` into a file, it silently reads as `C:Games`. Since that line's parsed value equals what's in memory, `write()` leaves it untouched until you actually change that key; when you do, the new value is written with proper escaping.

### Comments

- A comment starts at the first **unescaped** `;` or `#`, **anywhere** on the line (not only at the start, and no whitespace is required before it).
- That means values containing `;` or `#` (URLs with fragments, hex colours, etc.) **must** escape them:

```ini
; whole-line comment
# also a whole-line comment
speed = 10     ; inline comment, value is "10"
colour = \#ff8800
url = http://example.com/\#top
note = a\;b
```

- Comments are **not** stored in the `Ini`; they're only preserved in the file by `write()`.

### Sections

- `[Name]` must be the first thing on the line (leading whitespace is fine).
- The name is trimmed: `[ Display ]` is `Display`.
- The header ends at the first unescaped `]`. **Anything after it is ignored**, whether or not it's a comment (`[Display] junk` is `Display`).
- To put a `]` inside a name, escape it: `[a\]b]` is `a]b`.
- A header without a closing `]` isn't a header; the line becomes a key (see `[oops` above).
- `[]` (empty name) is the same unnamed section `""` that holds keys before the first header.
- Empty sections (a header with no keys) are kept in the `Ini`.
- A header only counts at the start of a line, so `key = [x]` is just a key with the value `[x]`.

### Whitespace

- Spaces, tabs, `\r` and `\n` around section names, keys and values are **trimmed**. Whitespace *inside* is kept (`my key = my value`).
- Because of that, values with leading or trailing whitespace can't be stored and won't survive a write/read round trip.

### Duplicates

- **Duplicate keys** in a section: the **last** value wins on `read()`, at the position of the **first** occurrence.
- **Duplicate section headers** are merged into one section on `read()`, in order of first appearance.
- On `write()`, duplicates in the file are **collapsed**: the first occurrence is kept (with the current value) and the repeats are removed.

---

## What `write()` escapes

When writing a new or changed line, INIRW escapes (prefixes with `\`) the backslash itself plus these characters:

| Where | Escaped characters |
|---|---|
| Keys | `\` `;` `#` `=` `[` `]` |
| Values | `\` `;` `#` |
| Section names | `\` `;` `#` `]` |

```cpp
ini["Misc"]["a=b"]  = "x;y";
ini["Misc"]["color"] = "#ff0000";
ini["Misc"]["path"] = "C:\\tmp";
```

```ini
[Misc]
a\=b = x\;y
color = \#ff0000
path = C:\\tmp
```

When reading, only the escapes that are actually needed are required. For example, a stray `]` in the middle of a key is fine.

Newlines inside keys, values or section names are **not** escaped. Don't put them in; they would split the line in the file.

---

## What `write()` preserves

- **Comments and blank lines**: always kept, **even when the key or section they sit near is removed**. Comments aren't tied to keys, so deleting a key doesn't delete the comment above it.
- **Layout of unchanged lines**: kept exactly as written.
- **Layout of changed lines**: only the value is replaced; the key text, spacing and inline comment stay.
- **Order**: the file's order is the source of truth; only new content is added at the places described above.
- **UTF-8 BOM**: detected on read (so it never breaks the first key/section), and written back if the file had one.
- **Line endings**: detected from the **first line** (CRLF or LF). Existing lines keep their own endings; new lines use the detected style. Every line, including the last, ends with a newline after a write.
- **Atomic save**: output goes to `<file>.tmp`, then is renamed over the target. If anything fails, the original is left untouched.

---

## Error handling

`read()` and `write()` return `void` and report problems on `std::cerr`:

| Situation | Message | Effect |
|---|---|---|
| `read()` can't open the file | `Failed to open file: <file>` | `output` is left empty |
| `write()` given an empty `Ini` | `Valid write data is required to write!` | nothing is written |
| `write()` can't create the temp file | `Error whilst opening <file>.tmp for writing!` | nothing is written |
| `write()` fails while writing | `Error whilst writing <file>.tmp, <file> left untouched!` | temp file removed, original intact |
| rename over the target fails | `Could not replace <file>: <reason>` | temp file removed, original intact |

Writing to a file that doesn't exist yet is **not** an error; the file is created. Use `std::filesystem::exists` first if you need to tell "missing file" from "empty file" after a `read()`.

---

## `oMap` reference

`INIRW::oMap<kT, vT>` is an insertion-ordered map (a `std::list` of entries plus an `std::unordered_map` index). You can use it on its own.

| Member | Description |
|---|---|
| `vT& operator[](const kT& key)` | Returns the value, inserting a default `vT{}` at the **end** if the key is new. |
| `find(key)` | Iterator to the entry, or `end()`. `const` overload provided. |
| `contains(key)` | `true` if present; never inserts. |
| `erase(key)` | Removes the entry if present; silently does nothing otherwise. |
| `size()`, `empty()`, `clear()` | Usual meanings. |
| `begin()`, `end()` | Iterate in **insertion order** (`const` overloads provided). |
| copy / move | Copy rebuilds the index (copy-assignment is copy-and-swap); move is defaulted. |

Notes:

- Entries are `std::pair<const kT, vT>`: keys are immutable, values are mutable.
- Re-assigning an existing key keeps its position. Erasing a key and inserting it again moves it to the end.
- `kT` needs `std::hash` and `operator==`; `vT` must be default-constructible.
- Type aliases: `Entry`, `DataList`, `EntryIterator`, `CEntryIterator`, `LocationMap`, `LocationIterator`, `CLocationIterator`.

---

## Utility functions

These are public helpers in the `INIRW` namespace (used internally, available to you):

| Function | Description |
|---|---|
| `size_t fU(std::string_view str, char esc, std::string_view filter)` | "Find unescaped": index of the first character in `filter` that isn't escaped by `esc`, or `std::string::npos`. |
| `std::string trim(std::string_view str, std::string_view chars)` | Strips any of `chars` from both ends. |
| `std::string uesc(std::string_view str, char esc)` | Removes escape characters; the character after each one is kept literally. |
| `std::string esc(std::string_view str, char escC, std::string_view filter)` | Prefixes `escC` before every `escC` and every character in `filter`. |

Constants: `spaces` (`" \t\r\n"`), `kEsc` (`";#=[]"`), `vEsc` (`";#"`), `sEsc` (`";#]"`).

---

## Limitations

- Values are always strings; no typed getters.
- No multi-line values or line continuation, and no quote handling (quotes are ordinary characters).
- Newlines inside keys/values/sections are not escaped (see above).
- Comments aren't exposed in the `Ini`; they're only preserved in the file.
- `write()` can't truncate a file (an empty `Ini` is refused).
- Only a UTF-8 BOM is recognised; files are otherwise treated as raw bytes (UTF-16 won't parse).
- Not thread-safe, and no file locking.
- `write()` copies the whole `Ini` internally.

---

## License

MIT. See the license text at the top of `INIRW.h`.

# The code itself was made by me. Claude was used for this temporary README. This will soon be replaced by a handwritten one.
