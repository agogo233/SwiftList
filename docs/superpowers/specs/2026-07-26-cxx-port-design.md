# SwiftList C++ Port: Design & Implementation Plan

## 1. Overview

### 1.1 Motivation

| Current (.NET 10) | Target (C++ Native) |
|---|---|
| Requires .NET 10 Desktop Runtime (~120 MB install) | Zero runtime dependency, single executable |
| Self-contained publish ~60 MB | Static-linked binary ~3-5 MB |
| JIT startup ~800 ms | Native startup ~50-100 ms |
| Working set ~80-120 MB | Working set ~25-40 MB |
| Depends on JIT/GC at runtime | Compile-time optimization only |

### 1.2 Constraints

- **Platform**: Windows 10/11 x64 only (Win32 API exclusive)
- **Target Runtime**: No .NET, no VC++ redistributable (static CRT `/MT`)
- **UI**: Complete rewrite (WPF → pure Win32 + Direct2D)
- **Plugins**: No plugin system in v1.0 (deferred)
- **IPC Protocol**: Binary-compatible with existing C# version (enable gradual migration)
- **Build System**: CMake + vcpkg

### 1.3 Scope

Rewrite ~57,500 lines of C# production code (488 files) and ~17,700 lines of tests (198 files) into C++. Covers four processes:

| Process | Project | Lines | Role |
|---------|---------|-------|------|
| Service | `Service/` + `Core/` | ~18,000 | Windows Service, USN/MFT indexing, IPC server |
| App | `App/` + `Core/` | ~24,000 | WPF→Win32 UI, search client, plugin host, hook IPC |
| Hook | `Core/Hook/` | ~3,500 | Global keyboard/mouse hooks, Explorer integration |
| CLI | `Cli/` + `Core/` | ~6,500 | fzf-style terminal search tool |

## 2. Target Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                     SwiftList C++ Architecture                    │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌──────────┐          Named Pipes          ┌────────────────┐   │
│  │  App.exe │◄══════════════════════════════►│ Service.exe    │   │
│  │          │   "SwiftListPipe" (search)    │                │   │
│  │ Win32 UI │                               │ Win32 Service  │   │
│  │ D2D/DW   │◄══════════════════════════════►│ USN Monitor    │   │
│  │          │   Hook IPC (event+cmd)        │ MFT Scanner    │   │
│  └────┬─────┘                               │ Search Engine  │   │
│       │                                     └────────────────┘   │
│       │ Named Pipe (per-user)                                     │
│       │ "SwiftList_App_Search_Pipe_{user}"                        │
│  ┌────┴─────┐                               ┌────────────────┐   │
│  │ slf.exe  │                               │ Hook.exe       │   │
│  │ (CLI)    │                               │                │   │
│  │ Terminal │                               │ SetWindowsHook │   │
│  │ fzf-like │                               │ WinEvent Hook  │   │
│  └──────────┘                               │ IPC (2 pipes)  │   │
│                                             └────────────────┘   │
│                                                                   │
│  ┌────────────────── Shared Libraries ──────────────────────┐     │
│  │  core_engine.dll  (IndexV2, Search, Fzf, IPC)           │     │
│  │  ui_framework.dll (Win32 Window, D2D Renderer, Controls)│     │
│  └─────────────────────────────────────────────────────────┘     │
└──────────────────────────────────────────────────────────────────┘
```

### 2.1 Library Structure

```
swiftlist/
├── libengine/          # Static lib: core indexing + search
│   ├── index_v2/       #  Snapshot, DeltaOverlay, LiveIndex
│   ├── indexer/        #  JournalReader, MftParser, UsnMonitor
│   ├── search/         #  Fzf, SearchMatcher, SearchCoordinator
│   └── common/         #  Defs, Utf8, memory arena
├── libipc/             # Static lib: named pipe + wire protocol
│   ├── pipe/           #  CreateNamedPipe wrap, client, server
│   └── wire/           #  IpcMessage, SearchRequest binary serde
├── service/            # EXE: Windows Service
├── libui/              # Static lib: Win32 + Direct2D UI framework
│   ├── window/         #  Window class, message loop, DPI mgmt
│   ├── rendering/      #  D2D/DWrite surfaces, layers, clip
│   ├── controls/       #  Button, TextBox, ListBox, ScrollBar
│   └── theme/          #  Color palette, font, DynamicResource
├── app/                # EXE: Main UI (links libengine + libipc + libui)
├── hook/               # EXE: Global hook process
├── cli/                # EXE: Terminal search tool
└── third_party/        # vcpkg manifest (empty for v1)
```

## 3. C# → C++ Component Mapping

### 3.1 Core Engine (libengine)

| C# Concept | C++ Replacement | Difficulty | Notes |
|---|---|---|---|
| `MemoryMappedFile` + `byte*` | `CreateFileMappingW` + `MapViewOfFile` + `uint8_t*` | Easy | Win32 API 1:1 |
| `ReadOnlySpan<T>` | `std::span<T>` (C++20) | Easy | Same semantics |
| `MemoryMarshal.Read/Cast` | `memcpy` + `reinterpret_cast` | Easy | No alignment issues on x64 |
| `unsafe` | Native pointer arithmetic | Trivial | C++ default |
| `System.Runtime.Intrinsics` | `<intrin.h>` `_mm256_*` | Easy | Almost 1:1 instruction mapping |
| `ReaderWriterLockSlim` | `std::shared_mutex` | Easy | Same semantics |
| `CancellationToken` | `std::atomic<bool>` + `std::condition_variable` | Medium | Need manual cancellation chains |
| `Channel<T>` (BFS queue) | `moodycamel::ConcurrentQueue` or manual | Medium | Or lock-free SPSC ring buffer |
| `Parallel.For` | `std::thread` pool + `std::partition` | Medium | Manual work-stealing vs parallel_for |
| `ConcurrentBag<T>` | `std::mutex` + `std::vector` per thread | Medium | Thread-local + merge |
| `stackalloc T[]` | `std::array<T, N>` or `_alloca` | Easy | Fixed capacity, fallback to heap |
| `Encoding.UTF8` | `WideCharToMultiByte(CP_UTF8)` / manual | Medium | Already used in hot path |
| `Interlocked.CompareExchange` | `_InterlockedCompareExchange` | Easy | Intrinsic |
| `Volatile.Read/Write` | `std::atomic<T>` with `memory_order` | Easy | Better control |

#### Snapshot Columnar Layout (most critical data structure)

```
C# struct SnapshotSection (aligned, 0-copy)
C++ struct SnapshotHeader {  // sizeof ~256 bytes
    uint32_t magic;           // "SNP1"
    uint32_t version;
    uint32_t row_count;
    uint32_t max_relative_id;
    uint64_t checksum;

    // Per-column offset and stride
    struct Column {
        uint32_t offset;      // relative to base
        uint32_t stride;      // bytes per element
    };
    Column name_ids;          // uint32_t[]
    Column flags;             // uint16_t[]
    Column parent_indexes;    // int32_t[]
    Column unique_masks;      // uint64_t[]
    Column name_offsets;      // uint32_t[]
    Column name_blob;         // uint8_t[] (UTF-8)
    Column ids;               // UInt128[][]
    Column sizes;             // int64_t[]
    Column creation_times;    // uint32_t[]
    Column last_write_times;  // uint32_t[]
    Column child_starts;      // int32_t[]
    Column children;          // int32_t[]
    Column alias_starts;      // int32_t[]
    Column alias_provider_ids;// uint8_t[]
    Column alias_blob;        // uint8_t[]
    Column orphan_rows;       // int32_t[]
    Column orphan_frns;       // UInt128[][]
};
```

Access pattern in C++:

```cpp
// C++: same as C#, but more natural — pointers without `unsafe`
struct Snapshot {
    uint8_t* base_;
    SnapshotHeader* header_;  // (SnapshotHeader*)base_

    std::span<const uint32_t> NameIds() const {
        auto& col = header_->name_ids;
        return { (uint32_t*)(base_ + col.offset), header_->row_count };
    }
    std::span<const char> NameBlob() const {
        auto& col = header_->name_blob;
        return { (char*)(base_ + col.offset), col.stride };
    }
};
```

#### UInt128 (FRN = File Reference Number)

```cpp
// Both NTFS (64-bit) and ReFS (128-bit) need to be supported
struct UInt128 {
    uint64_t low;
    uint64_t high;

    auto operator<=>(const UInt128&) const = default;
};
// Hasher for unordered_map:
template<> struct std::hash<UInt128> {
    size_t operator()(UInt128 v) const noexcept {
        return hash_combine(v.low, v.high);
    }
};
```

### 3.2 IPC & Named Pipes (libipc)

| C# | C++ | Notes |
|---|---|---|
| `NamedPipeServerStream` | `CreateNamedPipeW` + `ConnectNamedPipe` | Need OVERLAPPED for async |
| `NamedPipeClientStream` | `CreateFileW` + `WaitNamedPipeW` | Simpler than server side |
| `PipeOptions.Asynchronous` | `FILE_FLAG_OVERLAPPED` | Windows IOCP pattern |
| `PipeDirection.InOut` | `PIPE_ACCESS_DUPLEX` | Identical |
| `PipeSecurityFactory` | `ConvertStringSecurityDescriptorToSecurityDescriptorW` | SDDL string |
| `Task.Run` listener loop | `std::thread` + `OVERLAPPED` pool | Or Win32 thread pool |
| `BinaryWriter`/`BinaryReader` | Manual `memcpy` into buffer | Wire protocol is fixed-size structs |

**IOCP Server Design (Replaces async/await)**:

```
┌──CompletionPort──────────────┐
│  I/O Completion Port (IOCP)  │  ← All pipe handles associated
├──────────────────────────────┤
│  Worker Thread Pool (4-8)    │  ← GetQueuedCompletionStatusEx
└──────────────────────────────┘
      │
      ├─ OnReadComplete → DispatchMessage → WriteResponse
      ├─ OnWriteComplete → ReadNextRequest
      └─ OnConnectComplete → PostRead
```

Server process:

```
CreateIoCompletionPort → completionPort
for i = 1..maxConnections:
    hPipe = CreateNamedPipe(...)  // FILE_FLAG_OVERLAPPED
    CreateIoCompletionPort(hPipe, completionPort, ..., 0)
    ConnectNamedPipe(hPipe, &overlapped)

// Worker pool:
for i = 1..workerCount:
    thread([&]{
        while (running) {
            GetQueuedCompletionStatusEx(completionPort, ..., INFINITE);
            // handle completed I/O
        }
    })
```

### 3.3 USN Journal / MFT (mostly Win32 calls, easy mapping)

```cpp
// C#: P/Invoke call wrapped in an async method
// C++: Direct Win32 call, synchronous on worker thread

HANDLE hVolume = CreateFileW(
    LR"(\\.\C:)", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
    nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);

USN_JOURNAL_DATA_V2 journal = {};
DWORD bytesReturned;
DeviceIoControl(hVolume, FSCTL_QUERY_USN_JOURNAL, nullptr, 0,
    &journal, sizeof(journal), &bytesReturned, nullptr);

// MFT raw parsing — already pointer-based in C#
// C++ is identical, just without 'unsafe' keyword
MftParser::ParseMftEntry(uint8_t* record) {
    auto* header = (MftRecordHeader*)record;
    // USA fixup, attribute walk, data run decode — identical logic
}
```

### 3.4 UI Framework (libui) — the hard part

WPF has no C++ equivalent. This section defines what we build.

#### 3.4.1 Rendering Pipeline

```
Per Frame:
  ├─ BeginDraw
  ├─ Clear background (theme color)
  ├─ Draw layer tree (bottom-up):
  │   ├─ Layer 0: Window chrome (shadow, border, title bar)
  │   ├─ Layer 1: Sidebar (filters, sections)
  │   ├─ Layer 2: Results list (virtualized, only visible rows)
  │   │   ├─ Row 0: Icon + Name + Path + Size + Date
  │   │   ├─ Row 3: (highlight from search)
  │   │   └─ Row 7: (focused selection)
  │   ├─ Layer 3: Search bar (Edit field, translucent overlay)
  │   └─ Layer 4: Popup overlays (context menu, QuickLook)
  └─ EndDraw (present 1 swap chain)
```

Rendering is Direct2D retained-mode (same as WPF) — invalidation-based, not per-frame redraw.

#### 3.4.2 Control Hierarchy

```
UIElement (base)
├── FrameworkElement
│   ├── TextBlock          (DirectWrite text)
│   ├── Image              (ID2D1Bitmap from WIC/HICON)
│   ├── TextBox            (editable, caret, selection)
│   ├── Button             (rect + text, hover/click states)
│   ├── ListBox            (virtualized vertical list + scrollbar)
│   └── Panel
│       ├── StackPanel     (vertical/horizontal layout)
│       └── DockPanel      (fill/left/top/right/bottom)
└── Window (top-level, owns HWND)
```

Each element implements:

```cpp
struct UIElement {
    // Layout
    virtual D2D1_SIZE_F Measure(D2D1_SIZE_F constraint) = 0;
    virtual void Arrange(D2D1_RECT_F finalRect) = 0;

    // Render
    virtual void Render(ID2D1RenderTarget* rt) = 0;

    // Hit test
    virtual UIElement* HitTest(D2D1_POINT_2F pt);

    // Property change notification
    void InvalidateVisual();
    void InvalidateLayout();

    // Tree
    UIElement* parent_;
    std::vector<std::unique_ptr<UIElement>> children_;
};
```

#### 3.4.3 Incremental Result List (Replacing ObservableCollection)

WPF's `ObservableCollection<T>` + `CollectionView` pattern replaced with:

```cpp
class ResultList {
    static constexpr int kPageSize = 64;
    static constexpr int kPreloadPages = 2;  // render-ahead

    // Data (owned by ViewModel, atomic snapshot)
    std::shared_ptr<const std::vector<SearchResult>> results_;

    // Visible range tracking (set by scroll)
    int first_visible_ = 0;
    int last_visible_ = 0;

    // Row heights pre-cached
    std::vector<float> row_heights_;

    // Efficient partial re-render on new data
    void OnNewSnapshot(std::shared_ptr<const std::vector<SearchResult>> newResults);
};
```

Key difference from WPF: instead of per-property change notifications, take atomic snapshots of the entire result set and diff against previous for minimal redraw. This avoids the complexity of `INotifyPropertyChanged` while achieving better performance.

#### 3.4.4 Themes

```cpp
struct Theme {
    // Colors
    D2D1_COLOR_F background;
    D2D1_COLOR_F foreground;
    D2D1_COLOR_F accent;
    D2D1_COLOR_F selection_background;
    D2D1_COLOR_F selection_foreground;
    D2D1_COLOR_F highlight_background;
    D2D1_COLOR_F highlight_foreground;
    D2D1_COLOR_F border;
    D2D1_COLOR_F shadow;

    // Fonts
    std::wstring font_family;
    float font_size_body;
    float font_size_title;
    float font_size_caption;
};

// ThemeManager singleton, switch at runtime
// All controls read current theme via ThemeManager::Current()
```

### 3.5 Hook Process

| C# | C++ | Notes |
|---|---|---|
| `SetWindowsHookEx(WH_KEYBOARD_LL)` | Same API | Direct call, no P/Invoke wrapper |
| `SetWindowsHookEx(WH_MOUSE_LL)` | Same API | Direct call |
| `SetWinEventHook` | Same API | For Explorer tracking |
| `STA Thread` + `GetMessage` loop | `GetMessage` loop | Every Win32 message pump is STA |
| `CoInitializeEx(COINIT_APARTMENTTHREADED)` | Same API | Direct COM init |
| Channel-based event dispatch | `std::queue` + `PostMessage` | Win32 message to signal main thread |

## 4. Implementation Phases

### Phase 1: Foundation — Build System + Core Types (Weeks 1-2)

**Deliverable**: Testable static lib with no external dependencies.

```
src/
├── CMakeLists.txt
├── common/
│   ├── defs.h              // UInt128, DriveLetter, FileAttrs
│   ├── utf8.h              // UTF-8 encode/decode helpers
│   └── arena.h             // Fixed-size arena allocator
└── tests/
    ├── CMakeLists.txt
    ├── defs_tests.cpp
    └── utf8_tests.cpp
```

Tasks:
- [ ] Set up CMake project with static CRT (`/MT`)
- [ ] Configure vcpkg (empty manifest for now)
- [ ] Set up Google Test with CMake integration
- [ ] Implement `UInt128` with full comparison/hash
- [ ] Implement UTF-8 thin wrappers (WideCharToMultiByte wrappers)
- [ ] Implement `ArenaAllocator` (linear bump allocator for DP matrix in Fzf)
- [ ] CI: GitHub Actions with Windows x64 build + test

### Phase 2: IndexV2 Data Structures (Weeks 3-6)

**Deliverable**: `libengine` can create, write, and read snapshots.

```
src/
└── index_v2/
    ├── snapshot.h             // Snapshot reader (memory-mapped)
    ├── snapshot_writer.h      // Snapshot builder + atomic commit
    ├── delta_overlay.h        // DeltaOverlay: mutable overlay on Snapshot
    ├── live_index.h           // LiveIndex: Snapshot + Delta + RWLock
    ├── compact.h              // Periodic merge: Delta → new Snapshot
    └── snapshot_section.h     // Column type registry
```

Tasks:
- [ ] `Snapshot` — open memory-mapped file, validate header, column accessors
- [ ] `SnapshotWriter` — build column layout in memory, atomic `replace via rename`
- [ ] `DeltaOverlay` — concurrent dictionary for each delta type
- [ ] `LiveIndex` — `std::shared_mutex`, forward reads, queue writes
- [ ] Compact — merge DeltaOverlay into new Snapshot
- [ ] Tests: Write Snapshot, read back, verify column integrity
- [ ] Tests: Concurrent read (search) + write (USN update) race condition
- [ ] Tests: Compact produces byte-identical snapshot to C# version

### Phase 3: File System / USN / MFT Engine (Weeks 7-11)

**Deliverable**: `libengine` can index a drive and build a Snapshot.

```
src/
├── indexer/
│   ├── volume_helper.h       // Volume open, FS type detection
│   ├── journal_reader.h      // USN Journal reader (FSCTL_READ_USN_JOURNAL)
│   ├── journal_reader_helper.h // Incremental catch-up
│   ├── mft_parser.h          // $MFT raw structure parsing
│   ├── mft_index_scanner.h   // Drive scan via MFT
│   ├── refs_scanner.h        // ReFS via OpenFileById BFS
│   └── file_record_store.h   // In-memory record cache
└── drive_monitor/
    ├── usn_monitor.h          // Per-drive monitoring loop
    ├── usn_record_parser.h   // V2/V3 record parsing
    └── drive_watcher_host.h  // Error recovery, retry logic
```

Tasks:
- [ ] `VolumeHelper` — `CreateFileW(\\.\X:)`, `GetVolumeInformationW`
- [ ] `MftParser` — USA fixup, attribute header walk, data run decode, `$INDEX_ROOT`/`$INDEX_ALLOCATION` name extraction
- [ ] `MftIndexScanner` — scan drive, produce `IndexRecord` stream
- [ ] `JournalReader` — `FSCTL_QUERY_USN_JOURNAL`, `FSCTL_READ_USN_JOURNAL`, reason mask filtering
- [ ] `UsnMonitor` — per-drive thread, poll loop with `Task.Delay(200-1000ms)`, error recovery
- [ ] `UsnRecordParser` — `READ_USN_JOURNAL_DATA_V1`, validate USN_RECORD_V2/V3
- [ ] Integration test: Index a temp VHD drive, verify against C# snapshot

### Phase 4: Search Engine (Weeks 12-15)

**Deliverable**: `libengine` can fuzzy-search an indexed Snapshot.

```
src/
└── search/
    ├── fzf_pattern.h          // Query tokenizer (fuzzy/exact/prefix/suffix)
    ├── fzf_byte_matcher.h     // ASCII byte-level fuzzy match (hot path)
    ├── fzf_fuzzy_matcher.h    // UTF-16 DP matcher
    ├── fzf_scoring.h          // Score calculation, bonus tables
    ├── fzf_top_n.h            // Bounded top-N priority queue (array-based)
    ├── fzf_rank_radix_sorter.h // Rank sort via LSB radix
    ├── fzf_byte_pattern.h     // Byte-level pattern view
    ├── fzf_slab.h             // DP matrix arena
    ├── search_matcher.h       // Parallel search with AVX2 pre-filter
    ├── index_v2_searcher.h    // Search context → SearchMatcher → result stream
    ├── search_coordinator.h   // Fan-out across all drives LiveIndex
    ├── search_query_parser.h  // Parse "keyword operator:value"
    └── highlight_mask.h       // Compute character-level highlight
```

Tasks:
- [ ] `FzfBytePattern` — ASCII fast path (C# `FzfBytePattern` is already self-contained)
- [ ] `FzfFuzzyMatcher` — DP table, score computation, position finding
- [ ] `FzfScoring` — bonus tables, match length vs position trade-off
- [ ] `FzfTopN` — bounded heap with SIMD acceleration (`_mm256_*`)
- [ ] `FzfRankRadixSorter` — 8-pass LSB radix on 64-bit SortKey
- [ ] `SearchMatcher` — `_mm256_movemask_epi8` pre-filter identical to C#, parallel worker dispatch
- [ ] `SearchCoordinator` — parallel search across drives, merge with rank order
- [ ] Tests: Fuzzy match score identical to C# for every case in existing test suite
- [ ] Tests: Top-N output matches C# (deterministic sort)

### Phase 5: IPC Layer (Weeks 16-18)

**Deliverable**: `libipc` supports pipe server/client with binary protocol.

```
src/
└── pipe/
    ├── pipe_server.h           // IOCP-based listener + connection pool
    ├── pipe_client.h           // Connect + send + receive
    ├── pipe_security.h         // SDDL-based ACL
    ├── search_request_wire.h  // SearchRequestMessage binary serde
    ├── search_response_wire.h // PipeResponse stream
    └── hook_ipc_message.h     // IpcMessage binary struct
```

Tasks:
- [ ] `PipeServer` — `CreateNamedPipe` + `CreateIoCompletionPort` + worker threads
- [ ] `PipeClient` — `CreateFileW` + `WaitNamedPipeW` synchronous path + optional OVERLAPPED
- [ ] Binary serialization — replicate byte-level format exactly
- [ ] `PipeSecurity` — `ConvertStringSecurityDescriptorToSecurityDescriptorW` for both system-wide and user-only ACLs
- [ ] Tests: C# ↔ C++ interchange (run C# client against C++ server and vice versa)
- [ ] Tests: 1000x concurrent request/response with data integrity check

### Phase 6: Service Process (Weeks 19-21)

**Deliverable**: `service.exe` is a working Windows Service with USN monitoring.

```
src/
└── service/
    ├── main.cpp                // Entry: --service | --install | --hook
    ├── usn_service.h           // ServiceBase equivalent: StartServiceCtrlDispatcher
    ├── service_installer.h     // CreateService / DeleteService
    ├── hook_mode_launcher.h   // Launch hook subprocess
    └── service_control_runner.h // sc.exe helper wrappers
```

Tasks:
- [ ] Service entry: `StartServiceCtrlDispatcher` + `HandlerEx` for control
- [ ] Service installer: `CreateServiceW` + `ChangeServiceConfig2` (SID type)
- [ ] Engine integration: tie JournalReader → LiveIndex → Snapshot lifecycle
- [ ] PipeServer integration: accept app connections, dispatch search queries
- [ ] Hook mode launcher: `CreateProcess` for hook.exe with IPC pipe names
- [ ] Tests: Install service → start → query status → stop → uninstall (automated test VM)

### Phase 7: UI Framework (Weeks 22-30)

**Deliverable**: `libui` supports windows, controls, layout, text rendering, themes.

```
src/
└── ui/
    ├── window.h                // Window: HWND, message pump, DPI, theme
    ├── direct2d_surface.h      // ID2D1HwndRenderTarget, DPI scale
    ├── directwrite_text.h      // IDWriteTextFormat, text layout
    ├── layout.h                // Measure/Arrange, StackPanel, DockPanel
    ├── text_block.h            // Read-only text
    ├── image_box.h             // Icon + file thumbnail
    ├── text_box.h              // Editable text + caret + selection
    ├── button.h                // Push button with hover/press/disabled
    ├── list_box.h              // Virtualized vertical list
    ├── scroll_bar.h            // Scrollbar with track + thumb
    ├── context_menu.h          // Popup menu
    ├── theme.h                 // Theme struct + ThemeManager
    ├── animation.h             // Easing, fade, translate transitions
    └── icon_cache.h            // Shell icon extraction + LRU cache
```

Tasks:
- [ ] `Window` — `RegisterClassEx`, `CreateWindowEx`, WNDPROC, hit testing
- [ ] `Direct2DSurface` — `D2D1CreateFactory`, per-window render target, DPI event handling
- [ ] `Layout` — Measure phase (ideal size) → Arrange phase (final rect), invalidate propagation
- [ ] `TextBox` — caret blink, IME input, selection, clipboard, undo
- [ ] `ListBox` — virtualized: only layout/render visible items, scroll events trigger re-layout
- [ ] `ThemeManager` — load from JSON, singleton, broadcast change event to all windows
- [ ] `IconCache` — `SHGetFileInfoW` (SHGFI_ICON), `HICON` → `ID2D1Bitmap`
- [ ] Tests: stress test ListBox with 10,000 items, verify memory < 5 MB
- [ ] Tests: TextBox input with IME composition
- [ ] Tests: High DPI (150%, 200%, 250%) correctness

### Phase 8: App UI (Weeks 31-36)

**Deliverable**: `app.exe` is a working search launcher.

```
src/
└── app/
    ├── app.h                   // Application: lifecycle, single-instance
    ├── quick_search_window.h   // QuickSearch: popup, topmost, transparent
    ├── main_search_window.h    // SearchWindow: sidebar + results + detail
    ├── settings_window.h       // Multi-tab settings
    ├── inline_search_window.h  // Inline bar: child HWND in Explorer
    ├── quick_look_window.h     // File preview overlay
    ├── search_view_model.h     // ViewModel: query → pipe → results
    ├── settings_view_model.h   // Settings: hotkey, indexing, appearance
    ├── pipe_client_wrapper.h   // Reusable client connection
    └── action_menu_builder.h   // Context menu from matched actions
```

Tasks:
- [ ] App lifecycle: single-instance mutex, startup arguments, tray icon
- [ ] `QuickSearchWindow` — centered on cursor, keyboard-only navigation, escape to dismiss
- [ ] `MainSearchWindow` — sidebar with filters, results grid, column layout, mouse interaction
- [ ] `SearchViewModel` — debounce typing (50ms), send via pipe, stream results, reconcile display
- [ ] Settings storage — JSON file, hotkey registration (`RegisterHotKey`), index config
- [ ] `QuickLookWindow` — transparent overlay on hover, file content preview, dismiss on move
- [ ] Tests: Simulate typing sequence, verify pipe messages match expected binary
- [ ] Tests: 5000 results, verify scroll is fluid (>30 fps)

### Phase 9: Hook Process (Weeks 37-40)

**Deliverable**: `hook.exe` supports global hotkeys and Explorer inline search.

```
src/
└── hook/
    ├── main.cpp                // Entry: STA message pump + HookIpcServer
    ├── keyboard_hook.h         // WH_KEYBOARD_LL: double-ctrl detection, key routing
    ├── mouse_hook.h            // WH_MOUSE_LL: inline search dismissal
    ├── explorer_tracker.h      // WinEvent: foreground, name-change, focus
    └── inline_search_manager.h // InlineSearchWindow lifecycle + IPC
```

Tasks:
- [ ] Keyboard/mouse hooks — `SetWindowsHookEx`, low-level hook proc in DLL or same process
- [ ] Double-ctrl activation detection (same algorithm as C# version)
- [ ] `ExplorerTracker` — `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)`, window title comparison
- [ ] `InlineSearchManager` — mirror App's InlineSearchManager state, IPC event dispatch
- [ ] Tests: Simulate key events, verify IPC messages match expected
- [ ] Integration test: Hook process + App process IPC round-trip

### Phase 10: CLI & Packaging (Weeks 41-43)

**Deliverable**: `slf.exe` + installer + optimized binary.

```
src/
└── cli/
    ├── main.cpp                // Entry: pipe probe → input loop → render → output
    ├── terminal.h              // CONOUT$/CONIN$ Win32 API
    ├── app_search_pipe_client.h // Connect + stream search results
    ├── search_session.h        // Query/result/selection state machine
    └── renderer.h              // ANSI escape frame render
```

Tasks:
- [ ] `Terminal` — `ReadConsoleInputW`, `WriteConsoleW`, `SetConsoleMode`, raw mode
- [ ] `SearchSession` — state machine: idle → searching → results → chosen
- [ ] `AppSearchPipeClient` — per-query connect, stream, disconnect
- [ ] `Renderer` — ANSI frame, incremental vs full, scroll region
- [ ] Inno Setup installer script (adapt existing)
- [ ] UPX compression (optional, for extreme binary size reduction)
- [ ] Final memory/performance tuning

## 5. Timeline

| Phase | Description | Weeks | Cumulative | Milestone |
|-------|-------------|-------|------------|-----------|
| 1 | Foundation | 2 | 2 | CMake + common types + CI |
| 2 | IndexV2 | 4 | 6 | Snapshot read/write, delta |
| 3 | USN/MFT Engine | 5 | 11 | Index a drive → snapshot |
| 4 | Search Engine | 4 | 15 | Fuzzy search working |
| 5 | IPC Layer | 3 | 18 | Pipe + binary protocol |
| 6 | Service Process | 3 | 21 | Working Windows Service |
| 7 | UI Framework | 9 | 30 | Windows, controls, text, theme |
| 8 | App UI | 6 | 36 | Search launcher functional |
| 9 | Hook Process | 4 | 40 | Global hotkeys + inline bar |
| 10 | CLI + Packaging | 3 | 43 | Release-ready binary |

**Total: ~10 months (43 weeks) for a single developer**

## 6. Key Technical Risks & Mitigation

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| UI framework effort underestimation | High | Critical | Phase 7 is the largest single phase. Start with bare-minimum controls; iterate visual polish after functional completion. |
| AVX2 SIMD code correctness | Medium | High | Tests must verify byte-identical results against C# for every match scenario. Use `_mm256_testc_si256` for debug assertions. |
| IOCP pipe server bugs (race conditions, deadlocks) | Medium | High | Comprehensive stress test with C# client sending 1000 concurrent requests. Use ThreadSanitizer during development. |
| Memory-mapped snapshot corruption | Low | Critical | Include header checksum, verify on open, handle `ERROR_COMMITMENT_LIMIT`. |
| MFT parser edge cases (compressed extents, sparse files, EA, reparse points) | Medium | Medium | Test against real user drives with diverse filesystems. All known C# cases. |
| UTF-8 / UTF-16 transcoding hot path | Low | Medium | Use system `WideCharToMultiByte` (CP_UTF8) — same as C#. Cache conversions. |
| UInt128 performance (hash table lookups) | Low | Medium | Reserve `std::unordered_map` with custom allocator if default is too slow. Consider `absl::flat_hash_map`. |

## 7. Verification Strategy

### 7.1 Per-Phase Verification

Each phase must pass before the next begins:

| Phase | Verification Gate |
|-------|------------------|
| 1 | `ctest --output-on-failure` green |
| 2 | Byte-identical snapshot to C# version for same input data |
| 3 | C# version's temp VHD index produces same row count + FRNs as C++ version |
| 4 | Top-100 search results identical to C# for 50 test queries |
| 5 | C# client ↔ C++ server full round-trip (binary match) |
| 6 | Install → start → search (via C++ CLI) → stop → uninstall |
| 7 | `libui` self-hosted demo app renders all controls without crashes |
| 8 | End-to-end: app.exe launches, searches, shows results |
| 9 | Double-ctrl activates search window, inline bar appears in Explorer |
| 10 | `slf.exe` searches and outputs path correctly |

### 7.2 Testing Per Commit

- All Google Test unit tests pass
- ASan + UBSan clean (MemorySanitizer as available)
- No compiler warnings at `/W4`

### 7.3 Regression Test Suite

Maintain a directory of `C# generated` snapshots and query results:

```
tests/fixtures/
├── snapshot_small.bin          # 1,000-entry snapshot from C#
├── snapshot_large.bin          # 100,000-entry snapshot
├── queries.txt                 # 100 queries with expected top-20 results
└── expected/                   # C# output for each query
    ├── query_001.json
    ├── query_002.json
    └── ...
```

## 8. Existing Code to Reuse

These files from the C# project can be used as test oracles, but not compiled into the C++ project:

| File | Purpose for C++ port |
|------|---------------------|
| `make.bat` / `build_and_run.bat` | Build + release mechanics (keep as-is for C# version) |
| `Installer/installer.iss` | Inno Setup packaging script (adapt for C++ exe) |
| `Tests/` fixtures | Test oracles: expected query results, snapshot files |
| `.editorconfig` | C++ style adaptation (naming conventions: `PascalCase` → `snake_case`) |
| `.github/workflows/release.yml` | CI/CD template for Windows + artifacts |

## 9. Appendices

### A. Key Data Flow: Search Request

```
User types "hello" in search box
       │
       ▼
TextBox::OnChar() → invalides view model input
       │
       ▼
SearchViewModel::SetQuery("hello")
  - Debounce (50ms timer reset on each keystroke)
  - On timer fire: pipe_client_.SendAsync(query)
       │
       ▼
PipeClient → WriteFile request bytes → PipeServer
  request = { RequestId::Search, query: "hello", flags: Fuzzy }
       │
       ▼
PipeServer → SearchCoordinator::Search("hello")
  parallel_for each drive_live_index:
    index_v2_searcher.SearchStreaming("hello") →
      FzfBytePattern → NameSearch → SearchMatcher → FzfTopN
       │
       ▼
Result stream: matches sorted by score
  PipeServer → WriteFile response bytes for each batch
       │
       ▼
PipeClient::OnData() → SearchViewModel::AppendResults(batch)
  - Reconcile with current displayed results
  - Invalidate ListBox (only changed rows)
       │
       ▼
ListBox::Render() → measure visible rows → draw with Direct2D
```

### B. Key Data Flow: Index Update (USN Journal)

```
UsnMonitor thread (per-drive):
  loop:
    DeviceIoControl(hVolume, FSCTL_READ_USN_JOURNAL, ...)
    if records found:
      for each USN_RECORD:
        UsnIndexer.Apply(record):
          LiveIndex.mutex_.lock()
          delta_overlay_.AddLink(record.FileReferenceNumber, ...)
          delta_overlay_.RemoveLink(record.FileReferenceNumber, ...)
          delta_overlay_.RefreshMetadata(record.FileReferenceNumber, ...)
          LiveIndex.file_count_ += ...
          LiveIndex.dir_count_ += ...
          LiveIndex.mutex_.unlock()
    else:
      Sleep(200)  // no new journal entries
    if shutdown_requested: break
```

### C. Win32 API Reference (by subsystem)

| API | Used In |
|-----|---------|
| `CreateFileMappingW` / `MapViewOfFile` | Snapshot |
| `DeviceIoControl` | USN Journal, MFT |
| `CreateFileW` | Volume open, Named Pipe client |
| `CreateNamedPipeW` / `ConnectNamedPipe` | Pipe server |
| `ReadFile` / `WriteFile` (OVERLAPPED) | Pipe I/O |
| `CreateIoCompletionPort` / `GetQueuedCompletionStatusEx` | Pipe server |
| `RegisterClassExW` / `CreateWindowExW` / `WNDPROC` | All windows |
| `D2D1CreateFactory` / `ID2D1Factory` / `ID2D1HwndRenderTarget` | All rendering |
| `DWriteCreateFactory` / `IDWriteFactory` | All text |
| `SetWindowsHookEx(WH_KEYBOARD_LL / WH_MOUSE_LL)` | Hook process |
| `SetWinEventHook` | Explorer tracking |
| `SHGetFileInfoW` / `IExtractImageW` | Icon loading |
| `ShellExecuteExW` | File execution |
| `WideCharToMultiByte(CP_UTF8)` / `MultiByteToWideChar(CP_UTF8)` | UTF conversion |
| `StartServiceCtrlDispatcherW` | Service entry |
| `RegisterHotKey` | Global hotkey (if used instead of hook) |
| `ReadConsoleInputW` / `WriteConsoleW` | CLI terminal |
