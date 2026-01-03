# APFramework IPC Branch

**Version**: 2.0.0 (Design Phase)
**Status**: 🔨 In Development - Architecture Design Complete

---

## What is This?

This branch contains a **complete architectural redesign** of the Palworld Archipelago Framework using **Inter-Process Communication (IPC)** instead of the submodule architecture.

### Key Improvements

✅ **Standard UE4SS Mods** - No forced submodule structure
✅ **Non-Blocking** - No thread blocking during initialization
✅ **Multi-Platform** - Supports Lua, C++, and BP Logic mods
✅ **Production-Ready** - Designed for real-world use

---

## Documentation

- **[ARCHITECTURE.md](docs/ARCHITECTURE.md)** - Complete architecture design
- **[IMPLEMENTATION_PLAN.md](docs/.claude/IMPLEMENTATION_PLAN.md)** - Detailed technical specs

---

## Quick Comparison

### Main Branch (Submodule Architecture)

```
APFramework/
└── Mods/        ← Mods forced inside framework
    └── APTest/

⚠️ Blocks other mods during initialization
⚠️ Limited to submods only
✅ Working proof-of-concept
```

### IPC Branch (This Branch)

```
ue4ss/mods/
├── APFramework/    ← Framework (Lua + C++)
├── MyLuaMod/       ← Standard Lua mod
└── MyCppMod/       ← Standard C++ mod

✅ Non-blocking operation
✅ Standard UE4SS mods
✅ Production-ready design
🔨 Implementation in progress
```

---

## Development Status

**Phase**: Architecture Design ✅ Complete

**Next Steps**:
1. Implement C++ framework core (`APFrameworkCore.dll`)
2. Implement C++ client library (`APClientLib.dll`)
3. Implement Lua client wrapper (`ap_client.lua`)
4. Create example mods
5. Test and document

**Estimated Timeline**: 3-4 weeks

---

## For Developers

If you're interested in contributing or testing, check out:
- [Architecture Design](docs/ARCHITECTURE.md) - Understand the system
- [Implementation Plan](docs/.claude/IMPLEMENTATION_PLAN.md) - See what's being built

---

**This branch represents the future of Palworld Archipelago integration!**
