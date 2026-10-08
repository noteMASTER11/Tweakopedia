# Atomic Tweak Expansion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add 62 independently controlled, documented DWORD tweaks to the existing catalog.

**Architecture:** Reuse the strict YAML schema and existing registry backend. No application code or QML changes are required; category navigation and Tweakopedia articles are data-driven.

**Tech Stack:** C++20, Qt 6, yaml-cpp, CMake/CTest, YAML content.

**Spec:** `docs/superpowers/specs/2026-10-09-atomic-tweak-expansion-design.md`

## Global Constraints

- Windows 10/11 x64 only.
- Existing operation types only: `registry.set_dword` and `registry.dword` detection.
- Every entry has one unique ID and a complete Russian article.
- Browser entries declare `required_components`.
- Current engine behavior, UI and transaction format remain unchanged.

## Review Focus

- Inverted DWORD meanings must show the correct current and target labels.
- Enum policies must expose every documented value and use a matching missing state.
- Windows 10-only and Windows 11-only entries must disappear under the unsupported filter on the other OS.
- Browser policies must be hidden when the corresponding browser component is absent.
- Every state operation must address the same object used by detection.

---

### Task 1: Define the catalog contract

**Files:**
- Modify: `tests/content/TweakCatalogLoaderTest.cpp`

**Interfaces:**
- Consumes: `TweakCatalogLoader::loadDirectory()` and `TweakCatalog::find()`.
- Produces: a failing contract for 335 entries, all new IDs and representative state mappings.

- [ ] Add assertions for the total count, the 62 IDs and representative multi-state/inverted/compatibility definitions.
- [ ] Build `TweakCatalogLoaderTest` and run it; expect failure because the new content is absent.
- [ ] Commit the failing contract together with Task 2 content after it turns green.

### Task 2: Add browser policy content

**Files:**
- Create: the 13 `content/tweaks/apps/chrome-*.yaml` files and 15 `content/tweaks/apps/edge-*.yaml` files listed in the spec.

**Interfaces:**
- Consumes: the catalog contract from Task 1 and existing `apps/chrome-ai` and `apps/edge-ui` subcategories.
- Produces: 28 loadable browser policy definitions.

- [ ] Add Chrome definitions with six binary and seven three-state policies.
- [ ] Add Edge definitions with correct direct or inverted boolean mappings.
- [ ] Build and run `TweakCatalogLoaderTest`; expect the count to advance but the Task 1 contract to remain red until all tasks are present.

### Task 3: Add Windows suggestion and diagnostic content

**Files:**
- Create: the 16 `content/tweaks/desktop/*.yaml`, 3 `content/tweaks/privacy/*.yaml` and 2 `content/tweaks/apps/edge-*.yaml` files listed in the spec.

**Interfaces:**
- Consumes: the contract from Task 1 and existing desktop/privacy category metadata.
- Produces: 21 loadable user and diagnostic definitions.

- [ ] Add every independently detected DWORD as its own article and control.
- [ ] Build and run `TweakCatalogLoaderTest`; expect the contract to remain red only for the final shell package.

### Task 4: Add shell and system content

**Files:**
- Create: the 12 `content/tweaks/desktop/*.yaml` and 1 `content/tweaks/gaming/*.yaml` files listed in the spec.

**Interfaces:**
- Consumes: the contract from Task 1.
- Produces: 13 definitions and a complete 335-entry catalog.

- [ ] Add the shell entries, including the three-state Start menu selector and exact OS build bounds.
- [ ] Build and run `TweakCatalogLoaderTest`; expect PASS.
- [ ] Run all CTest targets; expect 47/47 PASS.
- [ ] Commit the content package.

### Task 5: Package and document verification

**Files:**
- Modify: `docs/testing/atomic-tweak-expansion-results.md`
- Update generated artifact: `D:/ChatGPT/Projects/Tweakopedia/dist/Tweakopedia.exe`

**Interfaces:**
- Consumes: the green 335-entry catalog.
- Produces: a verified portable EXE and an auditable test record.

- [ ] Build `mingw-release` and package the portable EXE.
- [ ] Run package layout validation and the full test suite against the final tree.
- [ ] Record counts, commands and results.
- [ ] Commit documentation and push the branch to `origin/main` as previously authorized by the user.
