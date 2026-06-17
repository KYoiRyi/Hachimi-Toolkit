#include <jni.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

typedef void* (*HachimiGetApiFn)(const char* name);

typedef void* (*hachimi_instance_t)();
typedef void* (*hachimi_get_interceptor_t)(void* hachimi);
typedef void* (*interceptor_hook_t)(void* interceptor, void* orig_addr, void* hook_addr);

typedef void* (*il2cpp_get_assembly_image_t)(const char* assembly_name);
typedef void* (*il2cpp_get_class_t)(void* image, const char* namespaze, const char* name);
typedef void* (*il2cpp_get_method_t)(void* klass, const char* name, int argsCount);
typedef void* (*il2cpp_get_method_addr_t)(void* method);

typedef void* (*il2cpp_string_chars_t)(void* s);
typedef int32_t (*il2cpp_string_length_t)(void* s);

typedef void (*hachimi_register_on_game_initialized_t)(void (*callback)());

static hachimi_instance_t g_hachimi_instance = nullptr;
static hachimi_get_interceptor_t g_hachimi_get_interceptor = nullptr;
static interceptor_hook_t g_interceptor_hook = nullptr;

static il2cpp_get_assembly_image_t g_get_assembly_image = nullptr;
static il2cpp_get_class_t g_get_class = nullptr;
static il2cpp_get_method_t g_get_method = nullptr;
static il2cpp_get_method_addr_t g_get_method_addr = nullptr;

static il2cpp_string_chars_t g_string_chars = nullptr;
static il2cpp_string_length_t g_string_length = nullptr;

static std::string g_outputDir;

#include <sys/stat.h>

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
        std::string logPath = g_outputDir + "/error.log";
        std::ofstream ofs(logPath, std::ios::out | std::ios::app);
        if (ofs.is_open()) {
            ofs << msg << "\n";
            ofs.close();
        }
    }
}

std::string Utf16ToUtf8(uint16_t* chars, int32_t length) {
    std::string result;
    for (int i = 0; i < length; i++) {
        uint16_t c = chars[i];
        if (c <= 0x7F) {
            result += (char)c;
        } else if (c <= 0x7FF) {
            result += (char)(0xC0 | (c >> 6));
            result += (char)(0x80 | (c & 0x3F));
        } else {
            result += (char)(0xE0 | (c >> 12));
            result += (char)(0x80 | ((c >> 6) & 0x3F));
            result += (char)(0x80 | (c & 0x3F));
        }
    }
    return result;
}

std::string ReadIl2CppString(void* strObj) {
    if (!strObj) return "null";
    uint16_t* chars = (uint16_t*)g_string_chars(strObj);
    int32_t length = g_string_length(strObj);
    return Utf16ToUtf8(chars, length);
}

// Cyan.Downloader.Error Constructor Hook
typedef void* (*error_ctor_t)(void* this_ptr, int32_t type, int32_t errorFlag, int32_t errorCode, void* urlStr, void* method_info);
static error_ctor_t o_ErrorCtor = nullptr;

static void* h_ErrorCtor(void* this_ptr, int32_t type, int32_t errorFlag, int32_t errorCode, void* urlStr, void* method_info) {
    Log("--- INTERCEPTED DOWNLOAD ERROR (CTOR) ---");
    Log("ErrorType: " + std::to_string(type));
    Log("ErrorFlag: " + std::to_string(errorFlag));
    Log("ErrorCode: " + std::to_string(errorCode));
    
    std::string url = ReadIl2CppString(urlStr);
    Log("URL: " + url);
    Log("-----------------------------------------");
    
    return o_ErrorCtor(this_ptr, type, errorFlag, errorCode, urlStr, method_info);
}


void HookMethod(void* interceptor, void* klass, const char* methodName, int argsCount, void* hookFunc, void** origFuncOut) {
    void* method = g_get_method(klass, methodName, argsCount);
    if (!method) {
        Log("Failed to find method: " + std::string(methodName));
        return;
    }
    void* addr = g_get_method_addr(method);
    if (!addr) {
        Log("Failed to get address for: " + std::string(methodName));
        return;
    }
    *origFuncOut = g_interceptor_hook(interceptor, addr, hookFunc);
    if (*origFuncOut) {
        Log("Hooked " + std::string(methodName) + " successfully!");
    } else {
        Log("Failed to hook " + std::string(methodName));
    }
}

void OnGameInitialized() {
    Log("Game initialized, setting up error hooks...");
    
    void* image = g_get_assembly_image("umamusume.dll");
    if (!image) {
        Log("Failed to get umamusume.dll image");
        return;
    }
    
    void* hachimi = g_hachimi_instance();
    void* interceptor = g_hachimi_get_interceptor(hachimi);
    
    // Cyan.Downloader.Error
    void* cyanImage = g_get_assembly_image("_Cyan.dll");
    if (cyanImage) {
        void* errorKlass = g_get_class(cyanImage, "Cyan.Downloader", "Error");
        if (errorKlass) {
            HookMethod(interceptor, errorKlass, ".ctor", 4, (void*)h_ErrorCtor, (void**)&o_ErrorCtor);
        } else {
            Log("Failed to get Cyan.Downloader.Error");
        }
    } else {
        Log("Failed to get _Cyan.dll");
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
    // Add a 1-second delay to prevent concurrent hooking crashes with other plugins
    usleep(1000000);
    OnGameInitialized();
    return nullptr;
}

extern "C" __attribute__((visibility("default"))) bool hachimi_init_v3(HachimiGetApiFn get_api, int version) {
    g_hachimi_instance = (hachimi_instance_t)get_api("hachimi_instance");
    g_hachimi_get_interceptor = (hachimi_get_interceptor_t)get_api("hachimi_get_interceptor");
    g_interceptor_hook = (interceptor_hook_t)get_api("interceptor_hook");
    
    g_get_assembly_image = (il2cpp_get_assembly_image_t)get_api("il2cpp_get_assembly_image");
    g_get_class = (il2cpp_get_class_t)get_api("il2cpp_get_class");
    g_get_method = (il2cpp_get_method_t)get_api("il2cpp_get_method");
    g_get_method_addr = (il2cpp_get_method_addr_t)get_api("il2cpp_get_method_addr");
    
    g_string_chars = (il2cpp_string_chars_t)get_api("il2cpp_string_chars");
    g_string_length = (il2cpp_string_length_t)get_api("il2cpp_string_length");
    
    g_outputDir = "/sdcard/Android/media/jp.co.cygames.umamusume/hachimi/ErrorLog";
    EnsureDirectory(g_outputDir);
    
    Log("Error-Logger Plugin Initialized! Output Dir: " + g_outputDir);
    
    pthread_t t;
    pthread_create(&t, nullptr, HookThread, nullptr);
    pthread_detach(t);
    
    return true;
}
