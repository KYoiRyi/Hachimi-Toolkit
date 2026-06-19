#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sys/stat.h>

typedef void* (*HachimiGetApiFn)(const char* name);

typedef void* (*hachimi_instance_t)();
typedef void* (*hachimi_get_interceptor_t)(void* hachimi);
typedef void* (*interceptor_hook_t)(void* interceptor, void* orig_addr, void* hook_addr);

typedef void* (*il2cpp_get_assembly_image_t)(const char* assembly_name);
typedef void* (*il2cpp_get_class_t)(void* image, const char* namespaze, const char* name);
typedef void* (*il2cpp_get_method_addr_t)(void* klass, const char* name, int argsCount);

typedef uint16_t* (*il2cpp_string_chars_t)(void* str);
typedef int32_t (*il2cpp_string_length_t)(void* str);

static hachimi_instance_t g_hachimi_instance = nullptr;
static hachimi_get_interceptor_t g_hachimi_get_interceptor = nullptr;
static interceptor_hook_t g_interceptor_hook = nullptr;

static il2cpp_get_assembly_image_t g_get_assembly_image = nullptr;
static il2cpp_get_class_t g_get_class = nullptr;
static il2cpp_get_method_addr_t g_get_method_addr = nullptr;

static il2cpp_string_chars_t g_string_chars = nullptr;
static il2cpp_string_length_t g_string_length = nullptr;

static std::string g_outputDir;

void EnsureDirectory(const std::string& path) {
    std::string current = "";
    for (char c : path) {
        current += c;
        if (c == '/') {
            mkdir(current.c_str(), 0777);
        }
    }
    mkdir(current.c_str(), 0777);
}

void Log(const std::string& msg) {
    if (!g_outputDir.empty()) {
        std::string logPath = g_outputDir + "/hook.log";
        std::ofstream ofs(logPath, std::ios::out | std::ios::app);
        if (ofs.is_open()) {
            ofs << msg << "\n";
            ofs.close();
        }
    }
}

std::string GetIl2CppString(void* str) {
    if (!str || !g_string_chars || !g_string_length) return "null";
    std::string s;
    int32_t len = g_string_length(str);
    uint16_t* chars = g_string_chars(str);
    for (int i = 0; i < len; ++i) if (chars[i] < 0x80) s += (char)chars[i];
    return s;
}

uintptr_t GetModuleBase(const char* name) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    char line[512];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, name)) {
            base = strtoull(line, nullptr, 16);
            break;
        }
    }
    fclose(f);
    return base;
}

// Hook connection open
typedef bool (*connection_open_t)(void* this_ptr, void* path, void* vfs, void* pwd, int32_t flags, void* method_info);
static connection_open_t o_ConnectionOpen = nullptr;

static bool h_ConnectionOpen(void* this_ptr, void* path, void* vfs, void* pwd, int32_t flags, void* method_info) {
    std::string s_path = GetIl2CppString(path);
    std::string s_pwd = GetIl2CppString(pwd);
    std::string s_vfs = GetIl2CppString(vfs);
    
    Log("Sqlite3::Open -> Path: " + s_path + " | Pwd: " + s_pwd + " | VFS: " + s_vfs + " | Flags: " + std::to_string(flags));
    
    return o_ConnectionOpen(this_ptr, path, vfs, pwd, flags, method_info);
}

// Hook MakeMd5
typedef void* (*make_md5_t)(void* input, void* method_info);
static make_md5_t o_MakeMd5 = nullptr;

static void* h_MakeMd5(void* input, void* method_info) {
    std::string s_input = GetIl2CppString(input);
    Log("Cryptographer::MakeMd5 -> Input: " + s_input);
    
    uintptr_t base = GetModuleBase("libil2cpp.so");
    if (base) {
        void** slot = (void**)(base + 0x09AFD178);
        if (slot && *slot) {
            void* str_ptr = *slot;
            int32_t len = g_string_length ? g_string_length(str_ptr) : -1;
            uint16_t* chars = g_string_chars ? g_string_chars(str_ptr) : nullptr;
            std::string raw_hex = "";
            std::string printable = "";
            if (chars) {
                for (int i = 0; i < (len > 64 ? 64 : len); ++i) {
                    char tmp[8];
                    sprintf(tmp, "%04X ", chars[i]);
                    raw_hex += tmp;
                    if (chars[i] >= 0x20 && chars[i] < 0x7F) {
                        printable += (char)chars[i];
                    } else {
                        printable += ".";
                    }
                }
            }
            Log("Cryptographer::MakeMd5 -> Salt ptr: " + std::to_string((uintptr_t)str_ptr) + ", len: " + std::to_string(len) + ", hex: " + raw_hex + ", printable: " + printable);
        } else {
            Log("Cryptographer::MakeMd5 -> Salt slot is null or empty");
        }
    }
    
    void* res = o_MakeMd5(input, method_info);
    std::string s_res = GetIl2CppString(res);
    Log("Cryptographer::MakeMd5 -> Result: " + s_res);
    
    return res;
}

// Hook ComputeHash
typedef void* (*compute_hash_t)(void* data, void* method_info);
static compute_hash_t o_ComputeHash = nullptr;

static void* h_ComputeHash(void* data, void* method_info) {
    std::string s_data = GetIl2CppString(data);
    Log("Cryptographer::ComputeHash -> Data: " + s_data);
    
    void* res = o_ComputeHash(data, method_info);
    std::string s_res = GetIl2CppString(res);
    Log("Cryptographer::ComputeHash -> Result: " + s_res);
    
    return res;
}

void OnGameInitialized() {
    Log("Game initialized, setting up Tester-Hooker hooks...");
    
    void* handle = dlopen("libil2cpp.so", RTLD_LAZY);
    if (handle) {
        g_string_chars = (il2cpp_string_chars_t)dlsym(handle, "il2cpp_string_chars");
        g_string_length = (il2cpp_string_length_t)dlsym(handle, "il2cpp_string_length");
    }

    void* hachimi = g_hachimi_instance();
    void* interceptor = g_hachimi_get_interceptor(hachimi);

    void* image_libnative = g_get_assembly_image("LibNative.Runtime.dll");
    if (image_libnative) {
        void* klass_conn = g_get_class(image_libnative, "LibNative.Sqlite3", "Connection");
        if (klass_conn) {
            void* a_open = g_get_method_addr(klass_conn, "Open", 4);
            if (a_open) {
                o_ConnectionOpen = (connection_open_t)g_interceptor_hook(interceptor, a_open, (void*)h_ConnectionOpen);
                Log("LibNative.Sqlite3.Connection.Open hook installed.");
            } else {
                Log("Failed to find Connection.Open.");
            }
        } else {
            Log("Failed to find Connection class.");
        }
    }

    void* image_uma = g_get_assembly_image("umamusume.dll");
    if (image_uma) {
        void* klass_crypt = g_get_class(image_uma, "Gallop", "Cryptographer");
        if (klass_crypt) {
            void* a_makemd5 = g_get_method_addr(klass_crypt, "MakeMd5", 1);
            if (a_makemd5) {
                o_MakeMd5 = (make_md5_t)g_interceptor_hook(interceptor, a_makemd5, (void*)h_MakeMd5);
                Log("Gallop.Cryptographer.MakeMd5 hook installed.");
            } else {
                Log("Failed to find Cryptographer.MakeMd5.");
            }
            
            void* a_computehash = g_get_method_addr(klass_crypt, "ComputeHash", 1);
            if (a_computehash) {
                o_ComputeHash = (compute_hash_t)g_interceptor_hook(interceptor, a_computehash, (void*)h_ComputeHash);
                Log("Gallop.Cryptographer.ComputeHash hook installed.");
            } else {
                Log("Failed to find Cryptographer.ComputeHash.");
            }
        } else {
            Log("Failed to find Gallop.Cryptographer class.");
        }
    }
}

void* HookThread(void*) {
    while (true) {
        if (g_get_assembly_image) {
            void* image_uma = g_get_assembly_image("umamusume.dll");
            if (image_uma) break;
        }
        usleep(1000);
    }
    OnGameInitialized();
    return nullptr;
}

extern "C" __attribute__((visibility("default"))) bool hachimi_init_v3(HachimiGetApiFn get_api, int version) {
    g_hachimi_instance = (hachimi_instance_t)get_api("hachimi_instance");
    g_hachimi_get_interceptor = (hachimi_get_interceptor_t)get_api("hachimi_get_interceptor");
    g_interceptor_hook = (interceptor_hook_t)get_api("interceptor_hook");
    
    g_get_assembly_image = (il2cpp_get_assembly_image_t)get_api("il2cpp_get_assembly_image");
    g_get_class = (il2cpp_get_class_t)get_api("il2cpp_get_class");
    g_get_method_addr = (il2cpp_get_method_addr_t)get_api("il2cpp_get_method_addr");
    
    g_outputDir = "/sdcard/Android/media/jp.co.cygames.umamusume/hachimi/TesterHooker";
    EnsureDirectory(g_outputDir);
    
    Log("Tester-Hooker Plugin Initialized!");
    
    pthread_t t;
    pthread_create(&t, nullptr, HookThread, nullptr);
    pthread_detach(t);
    
    return true;
}
