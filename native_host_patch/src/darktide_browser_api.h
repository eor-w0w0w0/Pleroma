#pragma once

// Inferred LanLobbyBrowser API/data layout recovered from Darktide.exe.

namespace darktide {
namespace browser {

struct BrowserResultEntry {
    unsigned int blob_size;
    unsigned int blob_capacity;
    void *blob_ptr;
    void *blob_owner;
    unsigned int key_size;
    unsigned int key_capacity;
    void *key_ptr;
    void *key_owner;
    unsigned long long opaque_id;
    unsigned int match_key;
    unsigned int ordinal;
    unsigned char updated_flag;
    unsigned char padding[7];
};

static constexpr unsigned long kResultEntrySize = 0x48;
static constexpr unsigned long kBrowserConnectionlessThunkRva = 0x0056F610;
static constexpr unsigned long kBrowserConnectionlessHandlerRva = 0x0056F630;
static constexpr unsigned long kBrowserConnectionlessRejectGateRva = 0x0056F65E;
static constexpr unsigned long kCreateLobbyBrowserWrapperRva = 0x0048AF20;
static constexpr unsigned long kCreateLobbyBrowserRegisterSiteRva = 0x0048AFE0;
static constexpr unsigned long kBrowserRefreshLuaWrapperRva = 0x0048B240;
static constexpr unsigned long kBrowserRefreshInitHelperRva = 0x0056FC00;
static constexpr unsigned long kBrowserRefreshNativeRva = 0x0056F2E0;
static constexpr unsigned long kLanLobbyBrowserRefreshInitializedOffset = 0xB0;
static constexpr unsigned long kLanLobbyBrowserRefreshIntervalOffset = 0xB4;

// Cached descriptor / browser-id notes:
// - browser+0x78 points at a cached 64-bit descriptor/lobby key value
// - browser+0x80..+0xA4 is a fixed 0x25-byte textual browser/lobby name field
// - stock discovery send appends: [u64 descriptor key][0x25-byte browser name field]
// - stock connect-request send reuses the same cached descriptor/browser-name pair
// - `establish_connection_to_server` still requires this cached state; Lua endpoint args alone are not sufficient
// - there is a partial stock prep path around `0x140514710` + `0x140514870`:
//   - `0x140514710` copies four 16-byte lanes into manager-owned state, but the browser parser for events 0x11/0x13 only consumes a 64-bit key from the packet
//   - `0x140514870` builds a 0xA0 wrapper object with the same 0x25 text-field struct at +0x70
//   - this path is not self-contained and depends on parent manager registration

// Browser object notes:
// - +0x50 : browser/discovery transport object
// - +0x58/+0x60 : result count / result array
// - +0x68 : allocator / memory context
// - +0x70 : transport callback/registration handle
// - +0x78 : cached 64-bit descriptor/lobby key value
// - +0x80..+0xA4 : fixed 0x25-byte textual browser/lobby identifier field

// Result entry notes:
// - blob_ptr points at a NUL-separated string blob used by the `lobby()` accessor
// - key_ptr is compared during refresh/update matching
// - opaque_id at +0x30 is formatted as `%016llx` by the Lua accessor fallback path
// - match_key at +0x38 is the first-stage discriminator in update matching

} // namespace browser
} // namespace darktide
