# jASTERIX — ASTERIX to JSON Converter

jASTERIX is a C++ library for EUROCONTROL ASTERIX binary data to JSON conversion. It is part of the OpenATS COMPASS project but released as a standalone library for use in other projects. ASTERIX definitions are configuration-only (JSON), so categories and editions can be added without recompilation.

## Platform & distribution

- **OS**: Linux 64-bit (x86_64) only — no Windows or macOS support
- **Distribution format**: AppImage — single self-contained executable, no installation required. Downloaded from GitHub releases. The AppImage is built on Debian 10 (Buster) via Docker to maximize glibc compatibility across distributions.
- **Licensing**: Source code is GPL-3.0; AppImage binary is CC BY 4.0. Free for all use including commercial.
- **Also used as**: An embedded library inside COMPASS for ASTERIX decoding during data import.

## Architecture

jASTERIX is a **shared library** (`libjasterix`) with a **CLI client** (`jasterix_client`). There is no GUI — all interaction is via command line or library API.

**Key architectural layers:**
- **Parsing layer**: `jASTERIX` class is the main entry point. Decodes binary ASTERIX data in chunks using callbacks. Frame parsing (`FrameParser`) handles network framings (IOSS, RFF, raw/netto). Data block parsing (`ASTERIXParser`) splits blocks into records.
- **Definition layer**: All ASTERIX structure is defined in JSON files under `definitions/`. Categories, editions, REFs, SPFs, mappings, and framings are loaded at runtime — no hardcoded ASTERIX knowledge in the C++ code.
- **Item parsing layer**: Hierarchical item parsers (`ItemParserBase` subclasses) handle the various ASTERIX data item types: fixed bits/bytes, extendable, compound, repetitive, dynamic bytes, optional, skip.
- **Threading**: Intel TBB for multi-threaded frame/data-block processing. `DataBlockFinderTask` and `FrameParserTask` run as TBB tasks. Single-thread mode available via `--single_thread`.
- **Output**: JSON via nlohmann/json. Streaming callback-based architecture for processing large files without loading everything into memory.

**Design patterns**: Factory pattern for item parsers (`ItemParserBase::createItemParser()`), callback-based streaming (`std::function` callbacks for frame/record/data-block events), shared_ptr for editions/mappings.

## Build

```bash
cmake -B build -S .
make -C build -j$(nproc)
```

Build output goes to `build/bin/` (executables) and `build/lib/` (libraries).

**C++ standard**: The project uses **C++17** (`-std=c++17`). The AppImage is built inside a Debian 10 Docker container using GCC 8.3. C++17 features such as structured bindings, `std::optional`, `std::variant`, `if constexpr`, `std::string_view`, and `std::any` are available and may be used freely.

**Compiler flags**:
- Docker: `-UNDEBUG -Wall -std=c++17 -fext-numeric-literals`
- Local: `-UNDEBUG -Wall -std=c++17 -fno-omit-frame-pointer`

**Targets:**
- `jasterix` — shared library (core ASTERIX decoding)
- `jasterix_client` — CLI application
- `test_categories` — unit tests for all ASTERIX categories
- `test_limits` — edge-case/resource tests
- `test_performance` — performance benchmarks

## Testing

Framework: **Catch2** (header-only, in `lib/catch.hpp`)

```bash
./build/bin/test_categories --definition_path definitions/ --data_path src/test/
```

Unit tests live in `src/test/`. Test files are named `test_cat<NNN>_<edition>.cpp` (one per category/edition). A shared `test_jasterix.h` header provides common test utilities. Tests use `TEST_CASE`, `SECTION`, `REQUIRE`, and `Approx()`. Test data (small binary files per category) lives alongside the test source in `src/test/`.

Test executables require `--definition_path` and `--data_path` CLI arguments (Catch2 custom options via `clara`).

When adding tests for a new category/edition, add a `test_cat<NNN>_<edition>.cpp` file and register it in `src/test/CMakeLists.txt`.

## Project structure

```
include/jasterix/        Public API headers
  jasterix.h              Main library entry point (jASTERIX class)
  category.h              Category management (editions, REFs, SPFs, mappings)
  edition.h / editionbase.h   Edition definitions
  refedition.h / ref.h    Reserved Expansion Field support
  spfedition.h / spf.h    Special Purpose Field support
  mapping.h               Data mapping between formats
  frameparser.h           Frame parsing interface
  record.h                Record parsing (FSPEC, UAP)
  itemparserbase.h        Base class for item parsers
  iteminfo.h              Category item metadata
  global.h.in             Build-time config template (generates global.h)
src/                      Implementation source code
  jasterix.cpp            Main library implementation (~42KB)
  asterix/                ASTERIX parsing core (ASTERIXParser, Category, Edition, Record, Mapping, REF, SPF)
  frames/                 Frame parsing (FrameParser, FrameParserTask)
  items/                  Item parser implementations (fixed bits/bytes, extendable, compound, repetitive, dynamic, optional, skip)
  utils/                  Utilities (files, logger, string_conv, hashchecker, traced_assert)
  write/                  JSON output writing (JsonWriter, JsonFileWriteTask)
  client/                 CLI application (main.cpp)
  test/                   Catch2 unit tests + binary test data files
lib/                      Third-party headers (json.hpp, catch.hpp)
definitions/              ASTERIX definitions (JSON, configuration-only)
  categories/             Per-category definitions (001/, 002/, ..., 252/)
  framings/               Frame format definitions (ioss.json, rff.json, ioss_seq.json)
  data_block_definition.json   ASTERIX data block structure
  categories.json         Category registry with default editions/mappings
cmake_modules/            CMake find-scripts (FindTBB.cmake, FindLOG4CPP.cmake)
analyze/                  Python analysis scripts (data_items.py, adsb_quality.py, mapping checks)
appimage/                 AppImage packaging files
```

## Code conventions

### Naming
- **Files:** lowercase with underscores: `category.cpp`, `frameparser.h`, `string_conv.h`
- **Headers:** `.h` (not `.hpp`), use `#pragma once`
- **Classes:** PascalCase: `Category`, `Edition`, `FrameParser`, `ItemParserBase`, `ReservedExpansionField`
- **Functions:** camelCase: `hasEdition()`, `setCurrentEdition()`, `decodeFile()`, `parseItem()`
- **Getters:** no `get` prefix, just the property name: `name()`, `number()`, `comment()`
- **Setters:** `set` prefix: `setCurrentEdition()`, `setDebug()`
- **Query methods:** `has` prefix: `hasEdition()`, `hasMapping()`, `hasCurrentMapping()`
- **Member variables:** snake_case with trailing underscore: `number_`, `comment_`, `current_edition_`, `definition_path_`
- **Local variables:** snake_case without trailing underscore
- **Namespace:** `jASTERIX`

### Patterns
- **Factory pattern:** `ItemParserBase::createItemParser()` dispatches to concrete item parser subclasses based on JSON type field
- **Smart pointers:** `std::shared_ptr` for editions/mappings, `std::unique_ptr` for item parsers
- **JSON-driven configuration:** all ASTERIX structure defined in JSON, loaded at runtime via nlohmann/json
- **Callback-based streaming:** `std::function` callbacks for processing frames, data blocks, and records without buffering entire files
- **using declarations:** `using namespace std;`, `using namespace nlohmann;` at top of `.cpp` files

### Logging
Use the LOG4CPP-based stream macros defined in `src/utils/logger.h`. They auto-prepend the function name and are used like C++ output streams:
- `logerr` — errors (always printed)
- `logwrn` — warnings
- `loginf` — informational messages
- `logdbg` — debug (compiled in, but filtered by runtime log level)

Usage: `loginf << "decoded " << count << " records";`

Do **not** use `std::cout`, `std::cerr`, or `printf` for application logging.

LOG4CPP is optional — can be disabled via `USE_LOG4CPP=false` in CMakeLists.txt.

### Include order
1. Project includes: `#include <jasterix/...>`
2. Third-party headers: `#include "json.hpp"`
3. Standard library: `#include <string>`, `#include <map>`, `#include <memory>`

### License header
All source files must include the GPL-3.0 header referencing jASTERIX (see any existing `.h`/`.cpp` file).

## Key dependencies

- **Boost** (>= 1.73.0: program_options, filesystem, iostreams, regex, system, stacktrace_backtrace)
- **Intel TBB** — multi-threaded frame/data-block processing
- **LibArchive** — archive handling (ZIP output)
- **LOG4CPP** — logging (optional, enabled by default)
- **OpenSSL** — ARTAS MD5 hash computation (optional, enabled by default)
- **nlohmann/json** — JSON serialization (header-only in `lib/`)
- **Catch2** — unit testing (header-only in `lib/`)

## Domain concepts

- **ASTERIX**: EUROCONTROL standard binary format for surveillance data exchange. Data is organized into categories (e.g. CAT048 = radar, CAT062 = tracker, CAT021 = ADS-B).
- **Category**: An ASTERIX category number (001–252). Each category has one or more editions.
- **Edition**: A specific version of a category's data item definitions (e.g. CAT048 v1.15, v1.23). Defines the UAP and item structure.
- **UAP (User Application Profile)**: Ordered list of data item numbers that defines the record structure for a category edition. The FSPEC bits select which UAP items are present.
- **FSPEC (Field Specification)**: Variable-length bit field at the start of each record indicating which data items are present.
- **Data Item**: A numbered field within a record (e.g. "010" = Data Source Identifier, "140" = Time of Day). Each has a type (fixed bits, fixed bytes, compound, extendable, repetitive, etc.).
- **REF (Reserved Expansion Field)**: Optional extension items defined per category edition.
- **SPF (Special Purpose Field)**: Vendor-specific optional extension items (e.g. ARTAS TRIs for CAT062).
- **Mapping**: Transforms decoded data between formats (e.g. edition-specific field names to normalized names).
- **Framing**: Network encapsulation format around ASTERIX data blocks (IOSS, IOSS with sequence numbers, RFF, or raw/netto for no framing).
- **Data Block**: ASTERIX container: 1-byte category + 2-byte length + records.
- **SAC/SIC**: System Area Code / System Identification Code — identifies the data source sensor.
- **Mode 3/A**: SSR transponder code (4-digit octal, 0000–7777).
- **Mode C**: SSR altitude (flight level in 25ft increments).

## Supported ASTERIX categories

| CAT | Editions           | REFs | SPFs       | Description              |
|-----|--------------------|------|------------|--------------------------|
| 001 | 1.1                |      |            | Monoradar target reports |
| 002 | 1.0                |      |            | Monoradar service msgs   |
| 004 | 1.4                |      |            | Safety net messages      |
| 010 | 0.24, 0.31         |      |            | Surface movement data    |
| 019 | 1.2, 1.3           |      |            | Multilateration status   |
| 020 | 1.5, 1.8           | 1.3  |            | Multilateration reports  |
| 021 | 0.26, 2.1, 2.4     |      |            | ADS-B target reports     |
| 023 | 1.2                |      |            | CNS/ATM ground status    |
| 030 | 7.0                |      |            | ARTAS track messages     |
| 034 | 1.26               |      |            | Monoradar service msgs   |
| 048 | 1.15, 1.23         | 1.9  |            | Monoradar target reports |
| 062 | 1.12, 1.16, 1.18   | 1.2  | ARTAS TRIs | SDPS tracker reports     |
| 063 | 1.0, 1.1           |      |            | Sensor status messages   |
| 065 | 1.2, 1.3           |      |            | SDPS service status      |
| 247 | 1.2                |      |            | Version number exchange  |
| 252 | 7.0                |      |            | ARTAS track updates      |

## CLI usage

```
jasterix_client --definition_path definitions/ --filename <file> [options]

Key options:
  --framing <type>         Frame format: ioss, ioss_seq, rff (default: raw/netto)
  --frame_limit <n>        Max frames to process (-1 = unlimited)
  --frame_chunk_size <n>   Frames per chunk (default: 1000)
  --data_block_limit <n>   Max data blocks without framing (default: 10000)
  --print                  Print JSON output to stdout
  --print_indent <n>       JSON indentation (-1 = compact)
  --write_type <type>      Output format: text, zip
  --write_filename <path>  Output file path
  --only_cats <list>       Restrict categories (e.g. 20,21,48)
  --single_thread          Disable TBB multi-threading
  --debug                  Enable debug output
  --log_perf               Print performance statistics after processing
  --add_artas_md5          Compute ARTAS MD5 hashes
  --check_artas_md5 <cats> Compute and verify MD5 hashes for specified categories
  --add_record_data        Include original record hex data in output
  --print_cat_info         Print category/edition information
```

## ASTERIX definition format

Category definitions in `definitions/categories/<NNN>/cat<NNN>_<edition>.json` follow this structure:

```json
{
    "name": "cat048_1.15_record",
    "type": "record",
    "field_specification": { "name": "FSPEC", "type": "extendable_bits", ... },
    "uap": ["010", "140", "020", "040", "070", "090", "130", ...],
    "items": [
        { "number": "010", "name": "Data Source Identifier", "type": "item",
          "data_fields": [
              { "name": "SAC", "type": "fixed_bytes", "length": 1, "data_type": "uint" },
              { "name": "SIC", "type": "fixed_bytes", "length": 1, "data_type": "uint" }
          ]
        },
        ...
    ]
}
```

**Item types**: `fixed_bytes`, `fixed_bits`, `fixed_bitfield`, `extendable_bits`, `extendable`, `compound`, `repetitive`, `dynamic_bytes`, `optional`, `skip_bytes`

## Library API

The main entry point is the `jASTERIX` class in `include/jasterix/jasterix.h`:

```cpp
jASTERIX jasterix(definition_path, print, debug, debug_exclude_framing);

// Decode a file with framing
jasterix.decodeFile(filename, framing, callback);

// Decode raw data buffer
jasterix.decodeData(data, size, callback);

// Category management
jasterix.hasCategory(cat_number);
jasterix.decodeCategory(cat_number);  // enable/disable
jasterix.category(cat_number);        // get Category object

// Stop decoding (from another thread)
jasterix.stopDecoding();
```

Callbacks receive `std::unique_ptr<nlohmann::json>` containing decoded frames/data blocks.
