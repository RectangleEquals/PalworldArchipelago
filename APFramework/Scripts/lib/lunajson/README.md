# APFramework Libraries

This directory contains third-party Lua libraries used by the framework.

## Required Libraries

### lunajson

**Source**: https://github.com/grafi-tt/lunajson
**License**: MIT
**Purpose**: JSON encoding/decoding for Lua

**Installation**:
1. Download lunajson from the GitHub repository
2. Place the following files in this directory:
   - `lunajson.lua` (main module)
   - `lunajson/` (subdirectory with implementation files)

**Usage in framework**:
```lua
local json = require("APFramework.lib.lunajson")
local data = json.decode(json_string)
local json_string = json.encode(data)
```

## Adding the Library

**Option 1: Manual Download**
1. Visit https://github.com/grafi-tt/lunajson
2. Download the release or clone the repository
3. Copy `src/lunajson.lua` to this directory
4. Copy `src/lunajson/` directory to this directory

**Option 2: Git Submodule** (if using git)
```bash
cd APFramework/lib
git clone https://github.com/grafi-tt/lunajson.git temp
cp temp/src/lunajson.lua ./
cp -r temp/src/lunajson ./
rm -rf temp
```

## Directory Structure

After installation, this directory should look like:
```
lib/
├── README.md (this file)
├── lunajson.lua
└── lunajson/
    ├── decoder.lua
    ├── encoder.lua
    └── ... (other implementation files)
```

## License Compliance

When distributing APFramework, ensure you:
1. Include the lunajson LICENSE file
2. Credit the original authors
3. Comply with the MIT license terms

## Future Libraries

Additional libraries may be added as needed for:
- WebSocket connections (lua-apclientpp)
- Additional utilities

---

**Note**: Until lunajson is added, the framework modules will fail to load. This is expected during development.
