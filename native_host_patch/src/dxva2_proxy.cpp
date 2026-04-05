// Minimal no-CRT dxva2 proxy for Darktide native patching.

extern "C" unsigned long long __readgsqword(unsigned long);

extern "C" int _fltused = 0;

using BOOL = int;
using BYTE = unsigned char;
using WORD = unsigned short;
using DWORD = unsigned long;
using ULONG = unsigned long;
using ULONG_PTR = unsigned long long;
using LONG = long;
using size_t = decltype(sizeof(0));
using HINSTANCE = void *;
using HMODULE = void *;
using LPVOID = void *;
using FARPROC = void *;
using LPCSTR = const char *;
using LPCWSTR = const wchar_t *;
using WCHAR = wchar_t;
using HANDLE = void *;

#define DLL_EXPORT __attribute__((dllexport))

static constexpr DWORD DLL_PROCESS_ATTACH = 1;
static constexpr DWORD GENERIC_WRITE = 0x40000000;
static constexpr DWORD FILE_SHARE_READ = 0x00000001;
static constexpr DWORD OPEN_ALWAYS = 4;
static constexpr DWORD FILE_ATTRIBUTE_NORMAL = 0x00000080;
static constexpr DWORD FILE_END = 2;
static constexpr DWORD AUTODELAY_LOAD_MS = 20000;
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
using SoloPlayHostPatchInitializeFn = int (*)();
using GetNumberOfPhysicalMonitorsFromHMONITORFn = BOOL (*)(void *, DWORD *);
using GetPhysicalMonitorsFromHMONITORFn = BOOL (*)(void *, DWORD, void *);

namespace proxy {

struct State {
    HMODULE self_module;
    HMODULE real_dxva2_module;
    HMODULE kernel32_module;
    HMODULE kernelbase_module;
    HMODULE patch_module;
    LoadLibraryExWFn load_library_exw;
    GetProcAddressFn get_proc_address;
    GetModuleFileNameWFn get_module_file_name_w;
    CreateFileWFn create_file_w;
    WriteFileFn write_file;
    SetFilePointerFn set_file_pointer;
    CloseHandleFn close_handle;
    SleepFn sleep;
    CreateThreadFn create_thread;
    GetNumberOfPhysicalMonitorsFromHMONITORFn get_number_of_physical_monitors_from_hmonitor;
    GetPhysicalMonitorsFromHMONITORFn get_physical_monitors_from_hmonitor;
    int initialized;
    int delayed_loader_started;
};

static State g_state = {};

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

static bool unicode_equals_ascii_ci(const UNICODE_STRING &value, const char *ascii)
{
    const size_t wchar_count = value.Length / sizeof(WCHAR);
    const size_t ascii_len = ascii_length(ascii);

    if (wchar_count != ascii_len || !value.Buffer) {
        return false;
    }

    for (size_t i = 0; i < ascii_len; ++i) {
        const char lhs = ascii_upper(static_cast<char>(value.Buffer[i] & 0xFF));
        const char rhs = ascii_upper(ascii[i]);

        if (lhs != rhs) {
            return false;
        }
    }

    return true;
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

static void append_wide_string(WCHAR *dst, size_t start, const WCHAR *src)
{
    size_t i = 0;

    while (src[i] != L'\0') {
        dst[start + i] = src[i];
        ++i;
    }

    dst[start + i] = L'\0';
}

static void build_sibling_path(WCHAR *path, DWORD path_capacity, HMODULE module, const WCHAR *file_name)
{
    if (!g_state.get_module_file_name_w || !module) {
        return;
    }

    for (DWORD i = 0; i < path_capacity; ++i) {
        path[i] = L'\0';
    }

    DWORD length = g_state.get_module_file_name_w(module, path, path_capacity);

    if (length == 0 || length >= path_capacity) {
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

static void write_log_line(const char *message)
{
    if (!g_state.create_file_w || !g_state.write_file || !g_state.set_file_pointer || !g_state.close_handle || !g_state.self_module) {
        return;
    }

    WCHAR path[260];
    build_sibling_path(path, 260, g_state.self_module, L"SoloPlayNativeHostPatch.log");

    if (path[0] == L'\0') {
        return;
    }

    void *handle = g_state.create_file_w(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (!handle || handle == reinterpret_cast<void *>(static_cast<ULONG_PTR>(-1))) {
        return;
    }

    g_state.set_file_pointer(handle, 0, nullptr, FILE_END);

    DWORD written = 0;
    DWORD message_length = static_cast<DWORD>(ascii_length(message));

    g_state.write_file(handle, message, message_length, &written, nullptr);
    g_state.write_file(handle, "\r\n", 2, &written, nullptr);
    g_state.close_handle(handle);
}

static void load_companion_patch()
{
    if (g_state.patch_module || !g_state.get_module_file_name_w || !g_state.load_library_exw || !g_state.self_module) {
        return;
    }

    WCHAR path[260];
    build_sibling_path(path, 260, g_state.self_module, L"SoloPlayNativeHostPatch.dll");

    g_state.patch_module = g_state.load_library_exw(path, nullptr, 0);

    if (!g_state.patch_module) {
        write_log_line("companion patch failed to load");
        return;
    }

    write_log_line("companion patch loaded");

    auto *initialize = reinterpret_cast<SoloPlayHostPatchInitializeFn>(g_state.get_proc_address(g_state.patch_module, "SoloPlayHostPatch_Initialize"));

    if (!initialize) {
        write_log_line("companion patch init export missing");
        return;
    }

    const int version = initialize();

    if (version > 0) {
        write_log_line("companion patch initialized");
    } else {
        write_log_line("companion patch init returned failure");
    }
}

static DWORD delayed_load_thread(LPVOID)
{
    if (g_state.sleep) {
        g_state.sleep(AUTODELAY_LOAD_MS);
    }

    write_log_line("dxva2 proxy attempting delayed patch load");
    load_companion_patch();

    return 0;
}

static void ensure_delayed_patch_load()
{
    if (g_state.delayed_loader_started || !g_state.create_thread || !g_state.close_handle) {
        return;
    }

    g_state.delayed_loader_started = 1;
    HANDLE thread = g_state.create_thread(nullptr, 0, delayed_load_thread, nullptr, 0, nullptr);

    if (!thread || thread == reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(-1))) {
        write_log_line("dxva2 proxy delayed loader thread failed");
        return;
    }

    write_log_line("dxva2 proxy scheduled delayed patch load");
    g_state.close_handle(thread);
}

static HMODULE load_real_dxva2_module()
{
    static const WCHAR SYSTEM_PATH[] = L"C:\\windows\\system32\\dxva2.dll";

    HMODULE module = g_state.load_library_exw(SYSTEM_PATH, nullptr, 0);

    if (module && module != g_state.self_module) {
        return module;
    }

    return nullptr;
}

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

static void *rva_to_ptr(HMODULE module, DWORD rva)
{
    return reinterpret_cast<void *>(reinterpret_cast<ULONG_PTR>(module) + rva);
}

static FARPROC resolve_export(HMODULE module, const char *name);

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

static void initialize_runtime()
{
    if (g_state.initialized) {
        return;
    }

    g_state.initialized = 1;
    g_state.kernel32_module = find_loaded_module("KERNEL32.DLL");
    g_state.kernelbase_module = find_loaded_module("KERNELBASE.DLL");

    auto *load_library_exw = reinterpret_cast<LoadLibraryExWFn>(resolve_export(g_state.kernel32_module, "LoadLibraryExW"));
    auto *get_proc_address = reinterpret_cast<GetProcAddressFn>(resolve_export(g_state.kernel32_module, "GetProcAddress"));
    auto *get_module_file_name_w = reinterpret_cast<GetModuleFileNameWFn>(resolve_export(g_state.kernel32_module, "GetModuleFileNameW"));
    auto *create_file_w = reinterpret_cast<CreateFileWFn>(resolve_export(g_state.kernel32_module, "CreateFileW"));
    auto *write_file = reinterpret_cast<WriteFileFn>(resolve_export(g_state.kernel32_module, "WriteFile"));
    auto *set_file_pointer = reinterpret_cast<SetFilePointerFn>(resolve_export(g_state.kernel32_module, "SetFilePointer"));
    auto *close_handle = reinterpret_cast<CloseHandleFn>(resolve_export(g_state.kernel32_module, "CloseHandle"));
    auto *sleep = reinterpret_cast<SleepFn>(resolve_export(g_state.kernel32_module, "Sleep"));
    auto *create_thread = reinterpret_cast<CreateThreadFn>(resolve_export(g_state.kernel32_module, "CreateThread"));

    g_state.load_library_exw = load_library_exw;
    g_state.get_proc_address = get_proc_address;
    g_state.get_module_file_name_w = get_module_file_name_w;
    g_state.create_file_w = create_file_w;
    g_state.write_file = write_file;
    g_state.set_file_pointer = set_file_pointer;
    g_state.close_handle = close_handle;
    g_state.sleep = sleep;
    g_state.create_thread = create_thread;

    write_log_line("dxva2 proxy initialized");

    if (!load_library_exw || !get_proc_address) {
        write_log_line("kernel32 resolution failed");
        return;
    }

    g_state.real_dxva2_module = load_real_dxva2_module();

    if (!g_state.real_dxva2_module) {
        write_log_line("system dxva2 failed to load");
        return;
    }

    g_state.get_number_of_physical_monitors_from_hmonitor = reinterpret_cast<GetNumberOfPhysicalMonitorsFromHMONITORFn>(get_proc_address(g_state.real_dxva2_module, "GetNumberOfPhysicalMonitorsFromHMONITOR"));
    g_state.get_physical_monitors_from_hmonitor = reinterpret_cast<GetPhysicalMonitorsFromHMONITORFn>(get_proc_address(g_state.real_dxva2_module, "GetPhysicalMonitorsFromHMONITOR"));

    write_log_line(g_state.get_number_of_physical_monitors_from_hmonitor && g_state.get_physical_monitors_from_hmonitor ? "system dxva2 exports resolved" : "system dxva2 exports missing");

    ensure_delayed_patch_load();
}

} // namespace proxy

extern "C" DLL_EXPORT BOOL GetNumberOfPhysicalMonitorsFromHMONITOR(void *monitor, DWORD *count)
{
    proxy::initialize_runtime();

    if (!proxy::g_state.get_number_of_physical_monitors_from_hmonitor) {
        return 0;
    }

    return proxy::g_state.get_number_of_physical_monitors_from_hmonitor(monitor, count);
}

extern "C" DLL_EXPORT BOOL GetPhysicalMonitorsFromHMONITOR(void *monitor, DWORD number_of_physical_monitors, void *physical_monitor_array)
{
    proxy::initialize_runtime();

    if (!proxy::g_state.get_physical_monitors_from_hmonitor) {
        return 0;
    }

    return proxy::g_state.get_physical_monitors_from_hmonitor(monitor, number_of_physical_monitors, physical_monitor_array);
}

extern "C" DLL_EXPORT int SoloPlayDxva2Proxy_IsReady()
{
    proxy::initialize_runtime();
    return proxy::g_state.real_dxva2_module != nullptr
        && proxy::g_state.get_number_of_physical_monitors_from_hmonitor != nullptr
        && proxy::g_state.get_physical_monitors_from_hmonitor != nullptr;
}

extern "C" DLL_EXPORT int SoloPlayDxva2Proxy_LoadPatch()
{
    proxy::initialize_runtime();
    proxy::load_companion_patch();
    return proxy::g_state.patch_module != nullptr;
}

extern "C" DLL_EXPORT int SoloPlayDxva2Proxy_PatchLoaded()
{
    proxy::initialize_runtime();
    return proxy::g_state.patch_module != nullptr;
}

extern "C" BOOL DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        proxy::g_state.self_module = instance;
    }

    return 1;
}
