// Minimal no-CRT Windows DLL scaffold for native Darktide patch work.

#include "darktide_lan_symbols.h"
#include "darktide_transport_api.h"
#include "darktide_packet_api.h"
#include "darktide_browser_api.h"
#include "darktide_connectionless_api.h"
#include "darktide_wan_client_api.h"

extern "C" unsigned long long __readgsqword(unsigned long);
extern "C" int _fltused = 0;

using BYTE = unsigned char;
using WORD = unsigned short;
using DWORD = unsigned long;
using ULONG = unsigned long;
using ULONG_PTR = unsigned long long;
using LONG = long;
using BOOL = int;
using WCHAR = wchar_t;
using HINSTANCE = void *;
using HMODULE = void *;
using LPVOID = void *;
using LPCSTR = const char *;
using LPCWSTR = const WCHAR *;
using FARPROC = void *;
using HANDLE = void *;
using size_t = decltype(sizeof(0));

#define DLL_EXPORT __attribute__((dllexport))

static constexpr DWORD DLL_PROCESS_ATTACH = 1;
static constexpr DWORD GENERIC_WRITE = 0x40000000;
static constexpr DWORD FILE_SHARE_READ = 0x00000001;
static constexpr DWORD OPEN_EXISTING = 3;
static constexpr DWORD OPEN_ALWAYS = 4;
static constexpr DWORD FILE_ATTRIBUTE_NORMAL = 0x00000080;
static constexpr DWORD FILE_END = 2;
static constexpr DWORD PAGE_EXECUTE_READWRITE = 0x40;
static constexpr int VALIDATION_WORKER_MAX_ATTEMPTS = 900;
static constexpr WORD IMAGE_DOS_SIGNATURE = 0x5A4D;
static constexpr DWORD IMAGE_NT_SIGNATURE = 0x00004550;
static constexpr int IMAGE_DIRECTORY_ENTRY_EXPORT = 0;

struct LIST_ENTRY {
    LIST_ENTRY *Flink;
    LIST_ENTRY *Blink;
};

struct UNICODE_STRING {
    WORD Length;
    WORD MaximumLength;
    WCHAR *Buffer;
};

struct PEB_LDR_DATA {
    ULONG Length;
    BYTE Initialized;
    BYTE Reserved[3];
    LPVOID SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
};

struct LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    LPVOID DllBase;
    LPVOID EntryPoint;
    ULONG SizeOfImage;
    ULONG Padding;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
};

struct PEB {
    BYTE Reserved[0x18];
    PEB_LDR_DATA *Ldr;
};

struct IMAGE_DOS_HEADER {
    WORD e_magic;
    WORD e_cblp;
    WORD e_cp;
    WORD e_crlc;
    WORD e_cparhdr;
    WORD e_minalloc;
    WORD e_maxalloc;
    WORD e_ss;
    WORD e_sp;
    WORD e_csum;
    WORD e_ip;
    WORD e_cs;
    WORD e_lfarlc;
    WORD e_ovno;
    WORD e_res[4];
    WORD e_oemid;
    WORD e_oeminfo;
    WORD e_res2[10];
    LONG e_lfanew;
};

struct IMAGE_DATA_DIRECTORY {
    DWORD VirtualAddress;
    DWORD Size;
};

struct IMAGE_FILE_HEADER {
    WORD Machine;
    WORD NumberOfSections;
    DWORD TimeDateStamp;
    DWORD PointerToSymbolTable;
    DWORD NumberOfSymbols;
    WORD SizeOfOptionalHeader;
    WORD Characteristics;
};

struct IMAGE_OPTIONAL_HEADER64 {
    WORD Magic;
    BYTE MajorLinkerVersion;
    BYTE MinorLinkerVersion;
    DWORD SizeOfCode;
    DWORD SizeOfInitializedData;
    DWORD SizeOfUninitializedData;
    DWORD AddressOfEntryPoint;
    DWORD BaseOfCode;
    unsigned long long ImageBase;
    DWORD SectionAlignment;
    DWORD FileAlignment;
    WORD MajorOperatingSystemVersion;
    WORD MinorOperatingSystemVersion;
    WORD MajorImageVersion;
    WORD MinorImageVersion;
    WORD MajorSubsystemVersion;
    WORD MinorSubsystemVersion;
    DWORD Win32VersionValue;
    DWORD SizeOfImage;
    DWORD SizeOfHeaders;
    DWORD CheckSum;
    WORD Subsystem;
    WORD DllCharacteristics;
    unsigned long long SizeOfStackReserve;
    unsigned long long SizeOfStackCommit;
    unsigned long long SizeOfHeapReserve;
    unsigned long long SizeOfHeapCommit;
    DWORD LoaderFlags;
    DWORD NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[16];
};

struct IMAGE_NT_HEADERS64 {
    DWORD Signature;
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;
};

struct IMAGE_EXPORT_DIRECTORY {
    DWORD Characteristics;
    DWORD TimeDateStamp;
    WORD MajorVersion;
    WORD MinorVersion;
    DWORD Name;
    DWORD Base;
    DWORD NumberOfFunctions;
    DWORD NumberOfNames;
    DWORD AddressOfFunctions;
    DWORD AddressOfNames;
    DWORD AddressOfNameOrdinals;
};

using LoadLibraryExWFn = HMODULE (*)(LPCWSTR, void *, DWORD);
using GetProcAddressFn = FARPROC (*)(HMODULE, LPCSTR);
using GetModuleFileNameWFn = DWORD (*)(HMODULE, WCHAR *, DWORD);
using CreateFileWFn = void *(*)(LPCWSTR, DWORD, DWORD, void *, DWORD, DWORD, void *);
using WriteFileFn = BOOL (*)(void *, const void *, DWORD, DWORD *, void *);
using SetFilePointerFn = DWORD (*)(void *, LONG, LONG *, DWORD);
using CloseHandleFn = BOOL (*)(void *);
using SleepFn = void (*)(DWORD);
using ThreadStartRoutineFn = DWORD (*)(LPVOID);
using CreateThreadFn = HANDLE (*)(void *, size_t, ThreadStartRoutineFn, void *, DWORD, DWORD *);
using VirtualProtectFn = BOOL (*)(void *, size_t, DWORD, DWORD *);
using FlushInstructionCacheFn = BOOL (*)(void *, const void *, size_t);

namespace patch {

struct PatchStatus {
    int version;
    int build_stage;
    int has_hidden_lobby_create;
    int has_hidden_server_handshake;
    int has_connection_host_bridge;
};

struct HostBootstrapSymbols {
    void *network_join_lan_lobby;
    void *network_leave_lan_lobby;
    void *network_create_game_session;
    void *hidden_create_lan_lobby;
    void *hidden_start_server_handshake;
};

struct RuntimeState {
    HMODULE self_module;
    HMODULE kernel32_module;
    HMODULE patch_target_module;
    LoadLibraryExWFn load_library_exw;
    GetProcAddressFn get_proc_address;
    GetModuleFileNameWFn get_module_file_name_w;
    CreateFileWFn create_file_w;
    WriteFileFn write_file;
    SetFilePointerFn set_file_pointer;
    CloseHandleFn close_handle;
    SleepFn sleep;
    CreateThreadFn create_thread;
    VirtualProtectFn virtual_protect;
    FlushInstructionCacheFn flush_instruction_cache;
    darktide::packet::WriteBits32Fn write_bits32;
    darktide::packet::WriteBits64Fn write_bits64;
    darktide::packet::WriteBlobBitsFn write_blob_bits;
    darktide::packet::ReadBits32Fn read_bits32;
    darktide::packet::ReadBits64Fn read_bits64;
    int initialized;
};

struct ValidationState {
    int worker_started;
    int worker_finished;
    int control_callback_registered;
    int browser_callback_feature_enabled;
    int synthetic_discovery_feature_enabled;
    int browser_callback_registered;
    unsigned long control_registration_token;
    unsigned long browser_registration_token;
    void *control_registration_handle;
    void *browser_registration_handle;
    void *lan_client;
    void *browser_transport;
    void *control_transport;
    unsigned long control_callback_count;
    unsigned long browser_callback_count;
    unsigned long long last_control_arg2;
    unsigned long long last_control_arg3;
    unsigned long long last_control_arg4;
    unsigned long long last_control_arg5;
    unsigned long long last_browser_arg2;
    unsigned long long last_browser_arg3;
    unsigned long long last_browser_arg4;
    unsigned long long last_browser_arg5;
    unsigned long long synthetic_lobby_id;
    unsigned int browser_transport_ready_observations;
    int logged_browser_waiting_for_stability;
    int logged_browser_not_ready;
    int logged_control_not_ready;
    BYTE control_registration_storage[64];
    BYTE browser_registration_storage[64];
};

static PatchStatus g_status = {
    1,
    0,
    0,
    0,
    0,
};

static HostBootstrapSymbols g_symbols = {};
static RuntimeState g_runtime = {};
static ValidationState g_validation = {};

// Keep browser/discovery validation disabled until its callback ABI is confirmed.
static constexpr int kDefaultEnableBrowserValidation = 0;
// Keep synthetic discovery replies disabled until browser transport validation is stable.
static constexpr int kDefaultEnableSyntheticDiscoverLobbyReply = 0;
// Keep browser event 0x12 hook disabled until a safe detour/trampoline is added.
static constexpr int kEnableBrowserEvent12Hook = 0;

struct SyntheticDiscoverReplyPlan {
    unsigned short envelope_tag;
    unsigned int reserved24;
    unsigned long long descriptor_key;
    BYTE browser_name_blob[darktide::connectionless::kBrowserNameFieldSizeBytes];
    unsigned int browser_service_id;
    unsigned int browser_reserved8;
    unsigned int browser_payload_len_bits;
    unsigned int browser_payload_len_bytes;
    BYTE browser_payload_token;
    unsigned long long browser_payload_lobby_id;
};

struct InlineHookPatch {
    void *target;
    BYTE original[16];
    unsigned int length;
};

static inline PEB *current_peb()
{
    return reinterpret_cast<PEB *>(__readgsqword(0x60));
}

static size_t ascii_length(const char *str)
{
    size_t len = 0;

    while (str && str[len] != '\0') {
        ++len;
    }

    return len;
}

static char ascii_upper(char c)
{
    return c >= 'a' && c <= 'z' ? static_cast<char>(c - ('a' - 'A')) : c;
}

static bool ascii_equals_ci(const char *lhs, const char *rhs)
{
    size_t i = 0;

    for (;; ++i) {
        const char left = ascii_upper(lhs[i]);
        const char right = ascii_upper(rhs[i]);

        if (left != right) {
            return false;
        }

        if (left == '\0') {
            return true;
        }
    }
}

static bool unicode_equals_ascii_ci(const UNICODE_STRING &value, const char *ascii)
{
    const size_t wchar_count = value.Length / sizeof(WCHAR);
    const size_t ascii_len = ascii_length(ascii);

    if (!value.Buffer || wchar_count != ascii_len) {
        return false;
    }

    for (size_t i = 0; i < ascii_len; ++i) {
        const char left = ascii_upper(static_cast<char>(value.Buffer[i] & 0xFF));
        const char right = ascii_upper(ascii[i]);

        if (left != right) {
            return false;
        }
    }

    return true;
}

static void *rva_to_ptr(HMODULE module, DWORD rva)
{
    return reinterpret_cast<void *>(reinterpret_cast<ULONG_PTR>(module) + rva);
}

static void write_log_line(const char *message);

static void append_hex(char *dst, size_t &index, ULONG_PTR value)
{
    static const char HEX[] = "0123456789ABCDEF";

    for (int shift = static_cast<int>(sizeof(ULONG_PTR) * 8) - 4; shift >= 0; shift -= 4) {
        dst[index++] = HEX[(value >> shift) & 0xF];
    }
}

static void write_hex_line(const char *prefix, ULONG_PTR value)
{
    char buffer[160] = {};
    size_t index = 0;

    for (size_t i = 0; prefix[i] != '\0'; ++i) {
        buffer[index++] = prefix[i];
    }

    buffer[index++] = '0';
    buffer[index++] = 'x';
    append_hex(buffer, index, value);
    buffer[index] = '\0';

    write_log_line(buffer);
}

static void log_known_rva(const char *name, DWORD rva)
{
    if (!g_runtime.patch_target_module) {
        return;
    }

    char prefix[160] = {};
    size_t index = 0;

    for (size_t i = 0; name[i] != '\0'; ++i) {
        prefix[index++] = name[i];
    }

    prefix[index++] = ' ';
    prefix[index++] = '@';
    prefix[index++] = ' ';
    prefix[index] = '\0';

    write_hex_line(prefix, reinterpret_cast<ULONG_PTR>(rva_to_ptr(g_runtime.patch_target_module, rva)));
}

static void *read_global_pointer(DWORD rva)
{
    if (!g_runtime.patch_target_module) {
        return nullptr;
    }

    auto **slot = reinterpret_cast<void **>(rva_to_ptr(g_runtime.patch_target_module, rva));
    return slot ? *slot : nullptr;
}

static unsigned long read_u32_field(void *base, unsigned long offset)
{
    if (!base) {
        return 0;
    }

    auto *field = reinterpret_cast<unsigned long *>(reinterpret_cast<BYTE *>(base) + offset);
    return *field;
}

static void *read_ptr_field(void *base, unsigned long offset)
{
    if (!base) {
        return nullptr;
    }

    auto **field = reinterpret_cast<void **>(reinterpret_cast<BYTE *>(base) + offset);
    return *field;
}

static void log_runtime_lan_state()
{
    void *network_owner = read_global_pointer(darktide::lan::kNetworkOwnerGlobalRva);
    void *lan_client = read_global_pointer(darktide::lan::kLanClientGlobalRva);

    write_hex_line("network owner global = ", reinterpret_cast<ULONG_PTR>(network_owner));
    write_hex_line("lan client global = ", reinterpret_cast<ULONG_PTR>(lan_client));

    if (!lan_client) {
        return;
    }

    void *browser_transport = read_ptr_field(lan_client, darktide::lan::kLanClientBrowserTransportOffset);
    void *transport = read_ptr_field(lan_client, darktide::lan::kLanClientTransportOffset);
    unsigned long active_lobbies = read_u32_field(lan_client, darktide::lan::kLanClientActiveLobbiesCountOffset);
    void *active_lobby_array = read_ptr_field(lan_client, darktide::lan::kLanClientActiveLobbiesArrayOffset);
    unsigned long known_peers = read_u32_field(lan_client, darktide::lan::kLanClientKnownPeersCountOffset);
    void *known_peer_array = read_ptr_field(lan_client, darktide::lan::kLanClientKnownPeersArrayOffset);

    write_hex_line("lan client browser transport = ", reinterpret_cast<ULONG_PTR>(browser_transport));
    write_hex_line("lan client transport = ", reinterpret_cast<ULONG_PTR>(transport));
    write_hex_line("lan client active lobby array = ", reinterpret_cast<ULONG_PTR>(active_lobby_array));
    write_hex_line("lan client active lobby count = ", active_lobbies);
    write_hex_line("lan client known peer array = ", reinterpret_cast<ULONG_PTR>(known_peer_array));
    write_hex_line("lan client known peer count = ", known_peers);
}

static void set_synthetic_browser_name(SyntheticDiscoverReplyPlan *plan, const char *name);
static bool is_plausible_runtime_pointer(void *pointer);
static bool copy_bytes(BYTE *dst, const BYTE *src, unsigned int count);
static bool send_synthetic_discover_reply_explicit(void *browser_transport, const darktide::connectionless::SockAddrIn6Like *destination, const SyntheticDiscoverReplyPlan &plan);

static SyntheticDiscoverReplyPlan make_synthetic_discover_reply_plan(unsigned long long descriptor_key)
{
    SyntheticDiscoverReplyPlan plan = {};
    plan.envelope_tag = darktide::connectionless::kConnectionlessEnvelopeTag;
    plan.reserved24 = darktide::connectionless::kConnectionlessReserved24;
    plan.descriptor_key = descriptor_key;
    plan.browser_service_id = darktide::connectionless::kBrowserServiceIdLobbyBrowser;
    plan.browser_reserved8 = darktide::connectionless::kBrowserReserved8;
    plan.browser_payload_len_bits = 4;
    plan.browser_payload_len_bytes = 9;
    plan.browser_payload_token = darktide::connectionless::kBrowserEvent13Token;
    plan.browser_payload_lobby_id = descriptor_key;

    set_synthetic_browser_name(&plan, "Synthetic Solo Host");

    return plan;
}

static void set_synthetic_browser_name(SyntheticDiscoverReplyPlan *plan, const char *name)
{
    if (!plan) {
        return;
    }

    // The parser's accepted path wants exactly 36 non-zero bytes followed by a
    // final NUL byte at offset 0x24, so we pad with spaces instead of NULs.
    for (unsigned int i = 0; i < darktide::connectionless::kBrowserNameTextChars; ++i) {
        plan->browser_name_blob[i] = ' ';
    }

    plan->browser_name_blob[darktide::connectionless::kBrowserNameFieldSizeBytes - 1] = 0;

    if (!name) {
        return;
    }

    for (unsigned int i = 0; i < darktide::connectionless::kBrowserNameTextChars && name[i] != '\0'; ++i) {
        plan->browser_name_blob[i] = static_cast<BYTE>(name[i]);
    }
}

static unsigned long long compute_synthetic_lobby_id()
{
    if (g_validation.synthetic_lobby_id != 0) {
        return g_validation.synthetic_lobby_id;
    }

    const unsigned long long seed = reinterpret_cast<ULONG_PTR>(g_validation.lan_client)
        ^ reinterpret_cast<ULONG_PTR>(g_validation.browser_transport)
        ^ 0x534F4C4F504C4159ULL;

    g_validation.synthetic_lobby_id = seed ? seed : 0x534F4C4F504C4159ULL;
    return g_validation.synthetic_lobby_id;
}

static bool resolve_transport_peer_address(void *transport, unsigned long long source_id, darktide::connectionless::SockAddrIn6Like *out_address)
{
    if (!transport || !out_address) {
	return false;
}

    unsigned long count = read_u32_field(transport, darktide::transport::kPeerAddressCountOffset);
    void *entries = read_ptr_field(transport, darktide::transport::kPeerAddressEntriesOffset);

    if (!is_plausible_runtime_pointer(entries) || count == 0 || count > 0x1000) {
        return false;
    }

    for (unsigned long i = 0; i < count; ++i) {
        BYTE *entry = reinterpret_cast<BYTE *>(entries) + (i * darktide::transport::kPeerAddressEntrySize);
        unsigned long long candidate = *reinterpret_cast<unsigned long long *>(entry + darktide::transport::kPeerAddressEntryPeerIdOffset);

        if (candidate != source_id) {
            continue;
        }

        copy_bytes(reinterpret_cast<BYTE *>(out_address), entry + darktide::transport::kPeerAddressEntrySockAddrOffset, sizeof(darktide::connectionless::SockAddrIn6Like));
        return true;
    }

    return false;
}

static void *resolve_browser_transport_from_owner_chain()
{
	void *owner_root = read_global_pointer(darktide::wanclient::kWanClientOwnerGlobalRva);
	void *owner_nested = read_ptr_field(owner_root, darktide::wanclient::kWanClientOwnerNestedOffset);
	void *browser_transport = read_ptr_field(owner_nested, darktide::wanclient::kWanClientOwnerBrowserTransportOffset);

	if (is_plausible_runtime_pointer(browser_transport)) {
		write_hex_line("validation browser transport owner_root = ", reinterpret_cast<ULONG_PTR>(owner_root));
		write_hex_line("validation browser transport owner_nested = ", reinterpret_cast<ULONG_PTR>(owner_nested));
		write_hex_line("validation browser transport owner_path = ", reinterpret_cast<ULONG_PTR>(browser_transport));
		return browser_transport;
	}

	return nullptr;
}

static void *resolve_browser_transport_from_lan_client(void *lan_client)
{
	void *browser_transport = read_ptr_field(lan_client, darktide::lan::kLanClientBrowserTransportOffset);

	if (is_plausible_runtime_pointer(browser_transport)) {
		write_hex_line("validation browser transport lan_client_path = ", reinterpret_cast<ULONG_PTR>(browser_transport));
		return browser_transport;
	}

	return nullptr;
}

static void *resolve_browser_transport(void *lan_client)
{
	void *browser_transport = resolve_browser_transport_from_owner_chain();

	if (browser_transport) {
		return browser_transport;
	}

	return resolve_browser_transport_from_lan_client(lan_client);
}

static int synthetic_browser_event12_stub(void *browser, unsigned long long source_id, void *reader)
{
    if (!kEnableBrowserEvent12Hook) {
        return 0;
    }

    write_log_line("synthetic browser event12 stub reached");
    write_hex_line("synthetic browser source id = ", source_id);
    write_hex_line("synthetic browser reader = ", reinterpret_cast<ULONG_PTR>(reader));
    write_hex_line("synthetic browser handler target = ", reinterpret_cast<ULONG_PTR>(rva_to_ptr(g_runtime.patch_target_module, darktide::browser::kBrowserConnectionlessHandlerRva)));
    return 0;
}

static void initialize_bit_cursor(darktide::packet::BitCursor *cursor, void *buffer, unsigned int buffer_size)
{
    cursor->buffer_start = buffer;
    cursor->cursor = buffer;
    cursor->buffer_limit_or_size = buffer_size;
    cursor->error = 0;
    cursor->bit_state = 0;
    cursor->scratch = 0;
}

static void align_bit_cursor_to_byte(darktide::packet::BitCursor *cursor)
{
    if (!cursor || !g_runtime.write_bits32) {
        return;
    }

    if (cursor->bit_state != 0) {
        g_runtime.write_bits32(cursor, 0, 8 - cursor->bit_state);
    }
}

static unsigned int build_synthetic_discover_reply_packet(const SyntheticDiscoverReplyPlan &plan, BYTE *buffer, unsigned int buffer_size)
{
    if (!g_runtime.write_bits32 || !g_runtime.write_bits64 || !g_runtime.write_blob_bits || !buffer || buffer_size < 64) {
        return 0;
    }

    for (unsigned int i = 0; i < buffer_size; ++i) {
        buffer[i] = 0;
    }

    darktide::packet::BitCursor cursor = {};
    initialize_bit_cursor(&cursor, buffer, buffer_size);

    g_runtime.write_bits32(&cursor, plan.envelope_tag, 16);
    g_runtime.write_bits32(&cursor, plan.reserved24, 24);
    g_runtime.write_bits64(&cursor, plan.descriptor_key, darktide::connectionless::kDescriptorKeyBits);
    g_runtime.write_blob_bits(&cursor, plan.browser_name_blob, darktide::connectionless::kBrowserNameFieldSizeBytes * 8);
    g_runtime.write_bits32(&cursor, plan.browser_service_id, 4);
    g_runtime.write_bits32(&cursor, plan.browser_reserved8, 8);
    g_runtime.write_bits32(&cursor, plan.browser_payload_len_bits, 4);
    g_runtime.write_bits32(&cursor, plan.browser_payload_len_bytes, plan.browser_payload_len_bits);
    align_bit_cursor_to_byte(&cursor);

    BYTE payload[9] = {};
    payload[0] = plan.browser_payload_token;

    for (unsigned int i = 0; i < 8; ++i) {
        payload[1 + i] = static_cast<BYTE>((plan.browser_payload_lobby_id >> (i * 8)) & 0xFF);
    }

    g_runtime.write_blob_bits(&cursor, payload, sizeof(payload) * 8);

    const ULONG_PTR start = reinterpret_cast<ULONG_PTR>(cursor.buffer_start);
    const ULONG_PTR current = reinterpret_cast<ULONG_PTR>(cursor.cursor);
    unsigned int used = static_cast<unsigned int>(current - start);

    if (cursor.bit_state != 0) {
        used += 1;
    }

    return used;
}

static void log_byte_line(const char *prefix, const BYTE *data, unsigned int count)
{
    char buffer[256] = {};
    size_t index = 0;
    static const char HEX[] = "0123456789ABCDEF";

    for (size_t i = 0; prefix[i] != '\0'; ++i) {
        buffer[index++] = prefix[i];
    }

    for (unsigned int i = 0; i < count && index + 3 < sizeof(buffer); ++i) {
        if (i > 0) {
            buffer[index++] = ' ';
        }

        BYTE value = data[i];
        buffer[index++] = HEX[(value >> 4) & 0xF];
        buffer[index++] = HEX[value & 0xF];
    }

    buffer[index] = '\0';
    write_log_line(buffer);
}

static void *resolve_vfunc(void *object, unsigned long offset)
{
    if (!object) {
        return nullptr;
    }

    auto ***as_object = reinterpret_cast<void ***>(object);
    void **vtable = *as_object;

    if (!vtable) {
        return nullptr;
    }

    return vtable[offset / sizeof(void *)];
}

static bool is_plausible_runtime_pointer(void *pointer)
{
    const ULONG_PTR value = reinterpret_cast<ULONG_PTR>(pointer);

    if (value < 0x100000) {
        return false;
    }

    if ((value & 0x7) != 0) {
        return false;
    }

    return true;
}

static bool copy_bytes(BYTE *dst, const BYTE *src, unsigned int count)
{
    if (!dst || !src) {
        return false;
    }

    for (unsigned int i = 0; i < count; ++i) {
        dst[i] = src[i];
    }

    return true;
}

static void write_absolute_jump64(BYTE *dst, void *target)
{
    // mov rax, imm64 ; jmp rax
    dst[0] = 0x48;
    dst[1] = 0xB8;

    ULONG_PTR value = reinterpret_cast<ULONG_PTR>(target);

    for (unsigned int i = 0; i < 8; ++i) {
        dst[2 + i] = static_cast<BYTE>((value >> (i * 8)) & 0xFF);
    }

    dst[10] = 0xFF;
    dst[11] = 0xE0;
}

static bool install_inline_jump64(void *target, void *replacement, InlineHookPatch *out_patch)
{
    if (!g_runtime.virtual_protect || !target || !replacement || !out_patch) {
        return false;
    }

    static constexpr unsigned int kPatchLength = 12;
    DWORD old_protect = 0;

    if (!g_runtime.virtual_protect(target, kPatchLength, PAGE_EXECUTE_READWRITE, &old_protect)) {
        return false;
    }

    out_patch->target = target;
    out_patch->length = kPatchLength;
    copy_bytes(out_patch->original, reinterpret_cast<const BYTE *>(target), kPatchLength);

    BYTE patch_bytes[16] = {};
    write_absolute_jump64(patch_bytes, replacement);
    copy_bytes(reinterpret_cast<BYTE *>(target), patch_bytes, kPatchLength);

    DWORD ignored = 0;
    g_runtime.virtual_protect(target, kPatchLength, old_protect, &ignored);

    if (g_runtime.flush_instruction_cache) {
        g_runtime.flush_instruction_cache(nullptr, target, kPatchLength);
    }

    return true;
}

static bool remove_inline_jump64(const InlineHookPatch *patch)
{
    if (!g_runtime.virtual_protect || !patch || !patch->target || patch->length == 0) {
        return false;
    }

    DWORD old_protect = 0;

    if (!g_runtime.virtual_protect(patch->target, patch->length, PAGE_EXECUTE_READWRITE, &old_protect)) {
        return false;
    }

    copy_bytes(reinterpret_cast<BYTE *>(patch->target), patch->original, patch->length);

    DWORD ignored = 0;
    g_runtime.virtual_protect(patch->target, patch->length, old_protect, &ignored);

    if (g_runtime.flush_instruction_cache) {
        g_runtime.flush_instruction_cache(nullptr, patch->target, patch->length);
    }

    return true;
}

static void *resolve_vfunc_if_plausible(void *object, unsigned long offset)
{
    if (!is_plausible_runtime_pointer(object)) {
        return nullptr;
    }

    auto ***as_object = reinterpret_cast<void ***>(object);
    void **vtable = *as_object;

    if (!is_plausible_runtime_pointer(vtable)) {
        return nullptr;
    }

    void *candidate = vtable[offset / sizeof(void *)];

    if (!is_plausible_runtime_pointer(candidate)) {
        return nullptr;
    }

    return candidate;
}

using RegisterOuterCallbackFn = void *(*)(void *transport, void *out_handle, unsigned int registration_class, void *callback, void *context);
using UnregisterCallbackTokenFn = void (*)(void *transport, unsigned int token);

static int validation_control_callback(void *context, void *arg2, unsigned long long arg3, void *arg4, unsigned long long arg5, void *arg6)
{
    auto *state = reinterpret_cast<ValidationState *>(context);

    if (!state) {
        return 0;
    }

    state->control_callback_count += 1;
    state->last_control_arg2 = reinterpret_cast<ULONG_PTR>(arg2);
    state->last_control_arg3 = arg3;
    state->last_control_arg4 = reinterpret_cast<ULONG_PTR>(arg4);
    state->last_control_arg5 = arg5;

    if (state->control_callback_count <= 32) {
        write_hex_line("validation control callback count = ", state->control_callback_count);
        write_hex_line("validation control arg2 = ", state->last_control_arg2);
        write_hex_line("validation control arg3 = ", state->last_control_arg3);
        write_hex_line("validation control arg4 = ", state->last_control_arg4);
        write_hex_line("validation control arg5 = ", state->last_control_arg5);
        write_hex_line("validation control arg6 = ", reinterpret_cast<ULONG_PTR>(arg6));
    }

    return 0;
}

static int validation_browser_callback(void *context, void *arg2, unsigned long long source_id, void *arg4, unsigned long long arg5, unsigned long long event_code, void *reader)
{
    auto *state = reinterpret_cast<ValidationState *>(context);

    if (!state) {
        return 0;
    }

    state->browser_callback_count += 1;
    state->last_browser_arg2 = reinterpret_cast<ULONG_PTR>(arg2);
    state->last_browser_arg3 = source_id;
    state->last_browser_arg4 = reinterpret_cast<ULONG_PTR>(arg4);
    state->last_browser_arg5 = arg5;

    if (state->browser_callback_count <= 32) {
        write_hex_line("validation browser callback count = ", state->browser_callback_count);
        write_hex_line("validation browser arg2 = ", state->last_browser_arg2);
        write_hex_line("validation browser arg3 = ", state->last_browser_arg3);
        write_hex_line("validation browser arg4 = ", state->last_browser_arg4);
        write_hex_line("validation browser arg5 = ", state->last_browser_arg5);
        write_hex_line("validation browser event = ", event_code);
        write_hex_line("validation browser reader = ", reinterpret_cast<ULONG_PTR>(reader));
    }

    if (event_code == darktide::connectionless::ConnectionlessKind_DiscoverLobby && state->synthetic_discovery_feature_enabled) {
        darktide::connectionless::SockAddrIn6Like destination = {};

        if (!resolve_transport_peer_address(state->browser_transport, source_id, &destination)) {
            write_log_line("synthetic discover reply failed to resolve source address");
            write_hex_line("synthetic discover source id = ", source_id);
            return 0;
        }

        SyntheticDiscoverReplyPlan plan = make_synthetic_discover_reply_plan(compute_synthetic_lobby_id());

        if (send_synthetic_discover_reply_explicit(state->browser_transport, &destination, plan)) {
            write_log_line("synthetic discover reply sent");
            write_hex_line("synthetic discover source id = ", source_id);
            write_hex_line("synthetic discover lobby id = ", plan.descriptor_key);
        } else {
            write_log_line("synthetic discover reply send failed");
        }
    }

    return 0;
}

static int try_register_validation_callbacks()
{
    void *lan_client = read_global_pointer(darktide::lan::kLanClientGlobalRva);
    void *browser_transport = resolve_browser_transport(lan_client);
    void *control_transport = read_ptr_field(lan_client, darktide::lan::kLanClientTransportOffset);

    g_validation.lan_client = lan_client;
    g_validation.browser_transport = browser_transport;
    g_validation.control_transport = control_transport;

    if (!lan_client || !control_transport) {
        return 0;
    }

    auto *register_control = reinterpret_cast<RegisterOuterCallbackFn>(resolve_vfunc_if_plausible(control_transport, darktide::transport::kRegisterOuterCallbackVfuncOffset));

    write_hex_line("validation lan client = ", reinterpret_cast<ULONG_PTR>(lan_client));
    write_hex_line("validation control transport = ", reinterpret_cast<ULONG_PTR>(control_transport));

    if (register_control) {
        write_hex_line("validation register control vfunc = ", reinterpret_cast<ULONG_PTR>(register_control));
        g_validation.logged_control_not_ready = 0;
    } else if (!g_validation.logged_control_not_ready) {
        write_log_line("validation control transport not ready");
        g_validation.logged_control_not_ready = 1;
    }

    if (!g_validation.control_callback_registered && register_control) {
        for (size_t i = 0; i < sizeof(g_validation.control_registration_storage); ++i) {
            g_validation.control_registration_storage[i] = 0;
        }

        void *handle = register_control(
            control_transport,
            g_validation.control_registration_storage,
            1,
            reinterpret_cast<void *>(validation_control_callback),
            &g_validation);

        g_validation.control_registration_handle = handle;

        if (!handle) {
            write_log_line("validation control callback registration failed");
            return -1;
        }

        g_validation.control_registration_token = *reinterpret_cast<unsigned long *>(handle);
        g_validation.control_callback_registered = 1;

        write_log_line("validation control callback registered");
        write_hex_line("validation control handle = ", reinterpret_cast<ULONG_PTR>(handle));
        write_hex_line("validation control token = ", g_validation.control_registration_token);
    }

    if (g_validation.browser_callback_feature_enabled && g_validation.control_callback_registered) {
        auto *register_browser = reinterpret_cast<RegisterOuterCallbackFn>(resolve_vfunc_if_plausible(browser_transport, darktide::transport::kRegisterOuterCallbackVfuncOffset));

        if (register_browser) {
            write_hex_line("validation browser transport = ", reinterpret_cast<ULONG_PTR>(browser_transport));
            write_hex_line("validation register browser vfunc = ", reinterpret_cast<ULONG_PTR>(register_browser));
            if (g_validation.browser_transport_ready_observations < 0xFFFFFFFFu) {
                g_validation.browser_transport_ready_observations += 1;
            }
            g_validation.logged_browser_not_ready = 0;
            if (!g_validation.browser_callback_registered && g_validation.browser_transport_ready_observations < 3 && !g_validation.logged_browser_waiting_for_stability) {
                write_log_line("validation browser transport waiting for stability");
                g_validation.logged_browser_waiting_for_stability = 1;
            }
        } else if (!g_validation.logged_browser_not_ready) {
            write_log_line("validation browser transport not ready");
            g_validation.logged_browser_not_ready = 1;
            g_validation.browser_transport_ready_observations = 0;
            g_validation.logged_browser_waiting_for_stability = 0;
        }

        if (!g_validation.browser_callback_registered && register_browser && g_validation.browser_transport_ready_observations >= 3) {
            for (size_t i = 0; i < sizeof(g_validation.browser_registration_storage); ++i) {
                g_validation.browser_registration_storage[i] = 0;
            }

            void *handle = register_browser(
                browser_transport,
                g_validation.browser_registration_storage,
                1,
                reinterpret_cast<void *>(validation_browser_callback),
                &g_validation);

            g_validation.browser_registration_handle = handle;

            if (!handle) {
                write_log_line("validation browser callback registration failed");
                return -1;
            }

            g_validation.browser_registration_token = *reinterpret_cast<unsigned long *>(handle);
            g_validation.browser_callback_registered = 1;
            g_validation.logged_browser_waiting_for_stability = 0;

            write_log_line("validation browser callback registered");
            write_hex_line("validation browser handle = ", reinterpret_cast<ULONG_PTR>(handle));
            write_hex_line("validation browser token = ", g_validation.browser_registration_token);
        }
    }

    if (!g_validation.control_callback_registered) {
        return 0;
    }

    if (g_validation.browser_callback_feature_enabled && !g_validation.browser_callback_registered) {
        return 0;
    }

    return 1;
}

static bool send_synthetic_discover_reply_explicit(void *browser_transport, const darktide::connectionless::SockAddrIn6Like *destination, const SyntheticDiscoverReplyPlan &plan)
{
    if (!g_validation.synthetic_discovery_feature_enabled || !browser_transport || !destination) {
        return false;
    }

    auto *send_explicit = reinterpret_cast<darktide::connectionless::SendExplicitFn>(
        resolve_vfunc_if_plausible(browser_transport, darktide::transport::kSendExplicitDestinationVfuncOffset)
    );

    if (!send_explicit) {
        write_log_line("synthetic discover reply explicit send vfunc missing");
        return false;
    }

    BYTE packet[128] = {};
    unsigned int packet_size = build_synthetic_discover_reply_packet(plan, packet, sizeof(packet));

    if (packet_size == 0) {
        write_log_line("synthetic discover reply build failed");
        return false;
    }

    write_hex_line("synthetic discover reply size = ", packet_size);
    log_byte_line("synthetic discover reply bytes = ", packet, packet_size > 24 ? 24 : packet_size);
    write_hex_line("synthetic discover destination family = ", destination->family);
    write_hex_line("synthetic discover destination port_be = ", destination->port_be);
    log_byte_line("synthetic discover destination addr = ", destination->address, sizeof(destination->address));

    return send_explicit(browser_transport, destination, 0, packet, packet_size) != 0;
}

static void unregister_validation_control_callback()
{
    if (!g_validation.control_callback_registered || !g_validation.control_transport || !g_validation.control_registration_token) {
        return;
    }

    auto *unregister_callback = reinterpret_cast<UnregisterCallbackTokenFn>(
        resolve_vfunc_if_plausible(g_validation.control_transport, darktide::transport::kUnregisterCallbackTokenVfuncOffset)
    );

    if (!unregister_callback) {
        write_log_line("validation control unregister vfunc missing");
        return;
    }

    unregister_callback(g_validation.control_transport, g_validation.control_registration_token);
    write_log_line("validation control callback unregistered");
    g_validation.control_callback_registered = 0;
    g_validation.control_registration_token = 0;
    g_validation.control_registration_handle = nullptr;
}

static void unregister_validation_browser_callback()
{
    if (!g_validation.browser_callback_registered || !g_validation.browser_transport || !g_validation.browser_registration_token) {
        return;
    }

    auto *unregister_callback = reinterpret_cast<UnregisterCallbackTokenFn>(
        resolve_vfunc_if_plausible(g_validation.browser_transport, darktide::transport::kUnregisterCallbackTokenVfuncOffset)
    );

    if (!unregister_callback) {
        write_log_line("validation browser unregister vfunc missing");
        return;
    }

    unregister_callback(g_validation.browser_transport, g_validation.browser_registration_token);
    write_log_line("validation browser callback unregistered");
    g_validation.browser_callback_registered = 0;
    g_validation.browser_registration_token = 0;
    g_validation.browser_registration_handle = nullptr;
}

static void unregister_validation_callbacks()
{
    unregister_validation_browser_callback();
    unregister_validation_control_callback();
}

static DWORD validation_worker_thread(LPVOID)
{
    write_log_line("validation worker thread started");

    for (int attempt = 0; attempt < VALIDATION_WORKER_MAX_ATTEMPTS; ++attempt) {
        int result = try_register_validation_callbacks();

        if (result != 0) {
            g_validation.worker_finished = 1;
            return 0;
        }

        if (g_runtime.sleep) {
            g_runtime.sleep(1000);
        }
    }

    if (!g_validation.control_callback_registered) {
        write_log_line("validation worker timed out waiting for control transport");
    } else if (g_validation.browser_callback_feature_enabled && !g_validation.browser_callback_registered) {
        write_log_line("validation worker timed out waiting for browser transport");
    } else {
        write_log_line("validation worker timed out waiting for callback registration");
    }

    g_validation.worker_finished = 1;
    return 0;
}

static void start_validation_worker()
{
    if (g_validation.worker_started || !g_runtime.create_thread || !g_runtime.close_handle) {
        return;
    }

    g_validation.worker_started = 1;
    HANDLE thread = g_runtime.create_thread(nullptr, 0, validation_worker_thread, nullptr, 0, nullptr);

    if (!thread || thread == reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(-1))) {
        write_log_line("validation worker thread creation failed");
        return;
    }

    write_log_line("validation worker thread scheduled");
    g_runtime.close_handle(thread);
}

static FARPROC resolve_export(HMODULE module, const char *name);

static HMODULE find_loaded_module(const char *base_name)
{
    PEB *peb = current_peb();
    if (!peb || !peb->Ldr) {
        return nullptr;
    }

    LIST_ENTRY *head = &peb->Ldr->InLoadOrderModuleList;

    for (LIST_ENTRY *entry = head->Flink; entry && entry != head; entry = entry->Flink) {
        auto *module = reinterpret_cast<LDR_DATA_TABLE_ENTRY *>(entry);

        if (unicode_equals_ascii_ci(module->BaseDllName, base_name)) {
            return static_cast<HMODULE>(module->DllBase);
        }
    }

    return nullptr;
}

static FARPROC resolve_forwarder(const char *forwarder)
{
    char module_name[64] = {};
    char proc_name[128] = {};
    size_t split = 0;

    while (forwarder[split] && forwarder[split] != '.' && split + 1 < sizeof(module_name)) {
        module_name[split] = forwarder[split];
        ++split;
    }

    if (forwarder[split] != '.') {
        return nullptr;
    }

    size_t proc_index = 0;
    ++split;

    while (forwarder[split] && proc_index + 1 < sizeof(proc_name)) {
        proc_name[proc_index++] = forwarder[split++];
    }

    const size_t module_len = ascii_length(module_name);
    if (module_len + 4 >= sizeof(module_name)) {
        return nullptr;
    }

    module_name[module_len] = '.';
    module_name[module_len + 1] = 'D';
    module_name[module_len + 2] = 'L';
    module_name[module_len + 3] = 'L';
    module_name[module_len + 4] = '\0';

    HMODULE forwarded_module = find_loaded_module(module_name);
    if (!forwarded_module) {
        return nullptr;
    }

    return resolve_export(forwarded_module, proc_name);
}

static FARPROC resolve_export(HMODULE module, const char *name)
{
    if (!module) {
        return nullptr;
    }

    auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return nullptr;
    }

    auto *nt = reinterpret_cast<IMAGE_NT_HEADERS64 *>(reinterpret_cast<BYTE *>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return nullptr;
    }

    IMAGE_DATA_DIRECTORY export_dir_info = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!export_dir_info.VirtualAddress || !export_dir_info.Size) {
        return nullptr;
    }

    auto *export_dir = reinterpret_cast<IMAGE_EXPORT_DIRECTORY *>(rva_to_ptr(module, export_dir_info.VirtualAddress));
    auto *name_rvas = reinterpret_cast<DWORD *>(rva_to_ptr(module, export_dir->AddressOfNames));
    auto *ordinals = reinterpret_cast<WORD *>(rva_to_ptr(module, export_dir->AddressOfNameOrdinals));
    auto *functions = reinterpret_cast<DWORD *>(rva_to_ptr(module, export_dir->AddressOfFunctions));

    for (DWORD i = 0; i < export_dir->NumberOfNames; ++i) {
        const char *candidate = reinterpret_cast<const char *>(rva_to_ptr(module, name_rvas[i]));

        if (!ascii_equals_ci(candidate, name)) {
            continue;
        }

        const DWORD function_rva = functions[ordinals[i]];

        if (function_rva >= export_dir_info.VirtualAddress && function_rva < export_dir_info.VirtualAddress + export_dir_info.Size) {
            const char *forwarder = reinterpret_cast<const char *>(rva_to_ptr(module, function_rva));

            return resolve_forwarder(forwarder);
        }

        return reinterpret_cast<FARPROC>(rva_to_ptr(module, function_rva));
    }

    return nullptr;
}

static void append_wide_string(WCHAR *dst, size_t start, const WCHAR *src)
{
    size_t i = 0;

    while (src[i] != L'\0') {
        dst[start + i] = src[i];
        ++i;
    }

    dst[start + i] = L'\0';
}

static void build_sibling_path(WCHAR *path, DWORD capacity, HMODULE module, const WCHAR *file_name)
{
    if (!g_runtime.get_module_file_name_w || !module) {
        return;
    }

    for (DWORD i = 0; i < capacity; ++i) {
        path[i] = L'\0';
    }

    DWORD length = g_runtime.get_module_file_name_w(module, path, capacity);
    if (length == 0 || length >= capacity) {
        return;
    }

    size_t slash = length;

    while (slash > 0) {
        const WCHAR c = path[slash - 1];

        if (c == L'\\' || c == L'/') {
            break;
        }

        --slash;
    }

    if (slash == 0) {
        path[0] = L'\0';
        return;
    }

    path[slash] = L'\0';
    append_wide_string(path, slash, file_name);
}

static bool sibling_file_exists(HMODULE module, const WCHAR *file_name)
{
    if (!g_runtime.create_file_w || !g_runtime.close_handle) {
        return false;
    }

    WCHAR path[260];
    build_sibling_path(path, 260, module, file_name);

    void *handle = g_runtime.create_file_w(path, 0, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (!handle || handle == reinterpret_cast<void *>(static_cast<ULONG_PTR>(-1))) {
        return false;
    }

    g_runtime.close_handle(handle);
    return true;
}

static void load_runtime_feature_flags()
{
    g_validation.browser_callback_feature_enabled = kDefaultEnableBrowserValidation;
    g_validation.synthetic_discovery_feature_enabled = kDefaultEnableSyntheticDiscoverLobbyReply;

    if (!g_runtime.self_module) {
        return;
    }

    if (sibling_file_exists(g_runtime.self_module, L"SoloPlayNativeHostPatch.enable_browser_callback")) {
        g_validation.browser_callback_feature_enabled = 1;
    }

    if (sibling_file_exists(g_runtime.self_module, L"SoloPlayNativeHostPatch.enable_discovery_reply")) {
        g_validation.synthetic_discovery_feature_enabled = 1;
        g_validation.browser_callback_feature_enabled = 1;
    }

    write_hex_line("feature browser callback = ", g_validation.browser_callback_feature_enabled);
    write_hex_line("feature discovery reply = ", g_validation.synthetic_discovery_feature_enabled);
}

static void write_log_line(const char *message)
{
    if (!g_runtime.create_file_w || !g_runtime.write_file || !g_runtime.set_file_pointer || !g_runtime.close_handle || !g_runtime.self_module) {
        return;
    }

    WCHAR path[260];
    build_sibling_path(path, 260, g_runtime.self_module, L"SoloPlayNativeHostPatch.log");

    if (path[0] == L'\0') {
        return;
    }

    void *handle = g_runtime.create_file_w(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (!handle || handle == reinterpret_cast<void *>(static_cast<ULONG_PTR>(-1))) {
        return;
    }

    g_runtime.set_file_pointer(handle, 0, nullptr, FILE_END);

    DWORD written = 0;
    DWORD len = static_cast<DWORD>(ascii_length(message));

    g_runtime.write_file(handle, message, len, &written, nullptr);
    g_runtime.write_file(handle, "\r\n", 2, &written, nullptr);
    g_runtime.close_handle(handle);
}

static void initialize_runtime()
{
    if (g_runtime.initialized) {
        return;
    }

    g_runtime.initialized = 1;
    g_runtime.kernel32_module = find_loaded_module("KERNEL32.DLL");
    g_runtime.patch_target_module = find_loaded_module("Darktide.exe");

    g_runtime.load_library_exw = reinterpret_cast<LoadLibraryExWFn>(resolve_export(g_runtime.kernel32_module, "LoadLibraryExW"));
    g_runtime.get_proc_address = reinterpret_cast<GetProcAddressFn>(resolve_export(g_runtime.kernel32_module, "GetProcAddress"));
    g_runtime.get_module_file_name_w = reinterpret_cast<GetModuleFileNameWFn>(resolve_export(g_runtime.kernel32_module, "GetModuleFileNameW"));
    g_runtime.create_file_w = reinterpret_cast<CreateFileWFn>(resolve_export(g_runtime.kernel32_module, "CreateFileW"));
    g_runtime.write_file = reinterpret_cast<WriteFileFn>(resolve_export(g_runtime.kernel32_module, "WriteFile"));
    g_runtime.set_file_pointer = reinterpret_cast<SetFilePointerFn>(resolve_export(g_runtime.kernel32_module, "SetFilePointer"));
    g_runtime.close_handle = reinterpret_cast<CloseHandleFn>(resolve_export(g_runtime.kernel32_module, "CloseHandle"));
    g_runtime.sleep = reinterpret_cast<SleepFn>(resolve_export(g_runtime.kernel32_module, "Sleep"));
    g_runtime.create_thread = reinterpret_cast<CreateThreadFn>(resolve_export(g_runtime.kernel32_module, "CreateThread"));
    g_runtime.virtual_protect = reinterpret_cast<VirtualProtectFn>(resolve_export(g_runtime.kernel32_module, "VirtualProtect"));
    g_runtime.flush_instruction_cache = reinterpret_cast<FlushInstructionCacheFn>(resolve_export(g_runtime.kernel32_module, "FlushInstructionCache"));
    g_runtime.write_bits32 = reinterpret_cast<darktide::packet::WriteBits32Fn>(rva_to_ptr(g_runtime.patch_target_module, darktide::packet::kWriteBits32Rva));
    g_runtime.write_bits64 = reinterpret_cast<darktide::packet::WriteBits64Fn>(rva_to_ptr(g_runtime.patch_target_module, darktide::packet::kWriteBits64Rva));
    g_runtime.write_blob_bits = reinterpret_cast<darktide::packet::WriteBlobBitsFn>(rva_to_ptr(g_runtime.patch_target_module, darktide::packet::kWriteBlobBitsRva));
    g_runtime.read_bits32 = reinterpret_cast<darktide::packet::ReadBits32Fn>(rva_to_ptr(g_runtime.patch_target_module, darktide::packet::kReadBits32Rva));
    g_runtime.read_bits64 = reinterpret_cast<darktide::packet::ReadBits64Fn>(rva_to_ptr(g_runtime.patch_target_module, darktide::packet::kReadBits64Rva));

    write_log_line("companion patch runtime initialized");
    load_runtime_feature_flags();
}

static DWORD module_image_size(HMODULE module)
{
    auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(module);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }

    auto *nt = reinterpret_cast<IMAGE_NT_HEADERS64 *>(reinterpret_cast<BYTE *>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }

    return nt->OptionalHeader.SizeOfImage;
}

static ULONG_PTR module_find_ascii(HMODULE module, const char *needle)
{
    const size_t needle_len = ascii_length(needle);
    const DWORD image_size = module_image_size(module);

    if (!module || !needle_len || !image_size || image_size < needle_len) {
        return 0;
    }

    const BYTE *base = reinterpret_cast<const BYTE *>(module);

    for (DWORD i = 0; i <= image_size - needle_len; ++i) {
        bool matched = true;

        for (size_t j = 0; j < needle_len; ++j) {
            if (base[i + j] != static_cast<BYTE>(needle[j])) {
                matched = false;
                break;
            }
        }

        if (matched) {
            return reinterpret_cast<ULONG_PTR>(base + i);
        }
    }

    return 0;
}

static void log_string_probe(const char *name)
{
    if (!g_runtime.patch_target_module) {
        return;
    }

    const ULONG_PTR address = module_find_ascii(g_runtime.patch_target_module, name);

    if (address) {
        char prefix[160] = {};
        size_t index = 0;

        for (size_t i = 0; name[i] != '\0'; ++i) {
            prefix[index++] = name[i];
        }

        prefix[index++] = ' ';
        prefix[index++] = '@';
        prefix[index++] = ' ';
        prefix[index] = '\0';

        write_hex_line(prefix, address);
    } else {
        char buffer[192] = {};
        size_t index = 0;

        for (size_t i = 0; name[i] != '\0'; ++i) {
            buffer[index++] = name[i];
        }

        const char *suffix = " [absent]";

        for (size_t i = 0; suffix[i] != '\0'; ++i) {
            buffer[index++] = suffix[i];
        }

        buffer[index] = '\0';
        write_log_line(buffer);
    }
}

static void assign_known_symbols()
{
    if (!g_runtime.patch_target_module) {
        return;
    }

    g_symbols.network_join_lan_lobby = rva_to_ptr(g_runtime.patch_target_module, darktide::lan::kJoinLanLobbyWrapperRva);
    g_symbols.network_leave_lan_lobby = rva_to_ptr(g_runtime.patch_target_module, darktide::lan::kLeaveLanLobbyWrapperRva);
    g_symbols.network_create_game_session = rva_to_ptr(g_runtime.patch_target_module, darktide::lan::kLanConnectionlessSendDiscoveryRva);
    g_symbols.hidden_create_lan_lobby = nullptr;
    g_symbols.hidden_start_server_handshake = nullptr;
}

} // namespace patch

extern "C" DLL_EXPORT const patch::PatchStatus *SoloPlayHostPatch_GetStatus()
{
    return &patch::g_status;
}

extern "C" DLL_EXPORT const patch::HostBootstrapSymbols *SoloPlayHostPatch_GetSymbols()
{
    return &patch::g_symbols;
}

extern "C" DLL_EXPORT void SoloPlayHostPatch_SetBuildStage(int stage)
{
    patch::g_status.build_stage = stage;
}

extern "C" DLL_EXPORT int SoloPlayHostPatch_Version()
{
    return patch::g_status.version;
}

extern "C" DLL_EXPORT int SoloPlayHostPatch_Initialize()
{
    patch::initialize_runtime();
    patch::g_status.build_stage = 100;

    if (!patch::g_runtime.patch_target_module) {
        patch::write_log_line("darktide module not found");
        return patch::g_status.version;
    }

    patch::write_log_line("darktide module found");
    patch::write_hex_line("darktide base = ", reinterpret_cast<ULONG_PTR>(patch::g_runtime.patch_target_module));
    patch::write_hex_line("darktide image size = ", patch::module_image_size(patch::g_runtime.patch_target_module));

    patch::assign_known_symbols();

    patch::log_known_rva("join_lan_lobby wrapper", darktide::lan::kJoinLanLobbyWrapperRva);
    patch::log_known_rva("leave_lan_lobby wrapper", darktide::lan::kLeaveLanLobbyWrapperRva);
    patch::log_known_rva("create_lobby_browser wrapper", darktide::lan::kCreateLobbyBrowserWrapperRva);
    patch::log_known_rva("destroy_lobby_browser wrapper", darktide::lan::kDestroyLobbyBrowserWrapperRva);
    patch::log_known_rva("LanLobby live vtable", darktide::lan::kLanLobbyVtableRva);
    patch::log_known_rva("LanLobby update", darktide::lan::kLanLobbyUpdateRva);
    patch::log_known_rva("LanLobby channel dispatch", darktide::lan::kLanLobbyChannelDispatchRva);
    patch::log_known_rva("connectionless connect request", darktide::lan::kLanConnectionlessSendConnectRequestRva);
    patch::log_known_rva("lobby discovery send", darktide::lan::kLanConnectionlessSendDiscoveryRva);
    patch::log_known_rva("connectionless parser", darktide::lan::kLanConnectionlessParserRva);
    patch::log_known_rva("outer connect callback", darktide::lan::kLanLobbyOuterConnectCallbackRva);
    patch::log_known_rva("create channel callback", darktide::lan::kLanLobbyCreateChannelCallbackRva);
    patch::log_known_rva("channel envelope thunk", darktide::lan::kLanLobbyChannelEnvelopeThunkRva);

    patch::log_runtime_lan_state();
    patch::start_validation_worker();

    patch::log_string_probe("join_lan_lobby");
    patch::log_string_probe("create_lobby_browser");
    patch::log_string_probe("create_lan_lobby");
    patch::log_string_probe("host_lan_lobby");
    patch::log_string_probe("start_server_handshake");
    patch::log_string_probe("server_handshake_status");
    patch::log_string_probe("make_game_session_host");
    patch::log_string_probe("set_game_session_host");
    patch::log_string_probe("lobby_host");
    patch::log_string_probe("rpc_request_host_type");
    patch::log_string_probe("rpc_reserve_slots_reply");

    return patch::g_status.version;
}

extern "C" BOOL DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        patch::g_runtime.self_module = instance;
    }

    return 1;
}
