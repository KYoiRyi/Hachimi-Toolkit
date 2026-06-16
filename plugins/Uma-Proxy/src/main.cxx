#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <sys/stat.h>
#include "json.hpp"

using json = nlohmann::json;

typedef void* (*HachimiGetApiFn)(const char* name);

typedef void* (*hachimi_instance_t)();
typedef void* (*hachimi_get_interceptor_t)(void* hachimi);
typedef void* (*interceptor_hook_t)(void* interceptor, void* orig_addr, void* hook_addr);

typedef void* (*il2cpp_get_assembly_image_t)(const char* assembly_name);
typedef void* (*il2cpp_get_class_t)(void* image, const char* namespaze, const char* name);
typedef void* (*il2cpp_get_method_t)(void* klass, const char* name, int argsCount);
typedef void* (*il2cpp_get_method_addr_t)(void* klass, const char* name, int argsCount);

typedef void* (*il2cpp_string_new_t)(const char* text);
typedef uint16_t* (*il2cpp_string_chars_t)(void* str);
typedef int32_t (*il2cpp_string_length_t)(void* str);

typedef void (*hachimi_log_t)(int level, const char* tag, const char* message);

static hachimi_log_t g_log = nullptr;
static hachimi_instance_t g_hachimi_instance = nullptr;
static hachimi_get_interceptor_t g_hachimi_get_interceptor = nullptr;
static interceptor_hook_t g_interceptor_hook = nullptr;

static il2cpp_get_assembly_image_t g_get_assembly_image = nullptr;
static il2cpp_get_class_t g_get_class = nullptr;
static il2cpp_get_method_t g_get_method = nullptr;
static il2cpp_get_method_addr_t g_get_method_addr = nullptr;

static il2cpp_string_new_t g_string_new = nullptr;
static il2cpp_string_chars_t g_string_chars = nullptr;
static il2cpp_string_length_t g_string_length = nullptr;

static std::string g_outputDir;

// Configs
static bool g_proxy_enabled = false;
static std::string g_proxy_url = "http://127.0.0.1:5090";
void Log(const std::string& msg) {
    if (!g_outputDir.empty()) {
        std::string logPath = g_outputDir + "/proxy.log";
        std::ofstream ofs(logPath, std::ios::out | std::ios::app);
        if (ofs.is_open()) {
            ofs << msg << "\n";
            ofs.close();
        }
    }
}

std::string GetPackageName() {
    return "jp.co.cygames.umamusume";
}

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

void LoadConfig() {
    std::string configPath = g_outputDir + "/config.json";
    std::ifstream ifs(configPath);
    if (ifs.is_open()) {
        try {
            json j;
            ifs >> j;
            g_proxy_enabled = j.value("proxy_enabled", false);
            g_proxy_url = j.value("proxy_url", "http://127.0.0.1:5090");
            Log("Config loaded. Proxy Enabled: " + std::to_string(g_proxy_enabled) + ", Target: " + g_proxy_url);
        } catch (const std::exception& e) {
            Log("Failed to parse config: " + std::string(e.what()));
        }
    } else {
        json j;
        j["proxy_enabled"] = false;
        j["proxy_url"] = "http://127.0.0.1:5090";
        std::ofstream ofs(configPath);
        if (ofs.is_open()) {
            ofs << j.dump(4);
            ofs.close();
            Log("Default config created.");
        }
    }
}

// Trampolines
typedef void* (*compress_req_t)(void* body, void* method_info);
typedef void* (*decompress_resp_t)(void* response, void* method_info);
typedef void* (*post_t)(void* this_ptr, void* url, void* postData, void* headers, void* method_info);

static compress_req_t o_CompressRequest = nullptr;
static decompress_resp_t o_DecompressResponse = nullptr;
static post_t o_Post = nullptr;

static void* h_CompressRequest(void* body, void* method_info) {
    if (g_proxy_enabled) {
        return body; // Bypass compression, return plaintext MsgPack
    }
    return o_CompressRequest(body, method_info);
}

static void* h_DecompressResponse(void* response, void* method_info) {
    if (g_proxy_enabled) {
        return response; // Bypass decompression, response is already plaintext MsgPack
    }
    return o_DecompressResponse(response, method_info);
}

typedef void* (*get_url_t)(void* method_info);
static get_url_t o_GetApplicationServerUrl = nullptr;

static void* h_GetApplicationServerUrl(void* method_info) {
    if (g_proxy_enabled && g_string_new) {
        return g_string_new(g_proxy_url.c_str());
    }
    return o_GetApplicationServerUrl(method_info);
}

void OnGameInitialized() {
    Log("Game initialized, setting up proxy hooks...");
    
    LoadConfig();

    void* handle = dlopen("libil2cpp.so", RTLD_LAZY);
    if (handle) {
        g_string_new = (il2cpp_string_new_t)dlsym(handle, "il2cpp_string_new");
        g_string_chars = (il2cpp_string_chars_t)dlsym(handle, "il2cpp_string_chars");
        g_string_length = (il2cpp_string_length_t)dlsym(handle, "il2cpp_string_length");
        dlclose(handle);
    }

    if (!g_string_new || !g_string_chars || !g_string_length) {
        Log("Failed to resolve string manipulation functions from libil2cpp.so");
        return;
    }
    
    // 1. Hook Compress/Decompress
    void* image_uma = g_get_assembly_image("umamusume.dll");
    if (image_uma) {
        void* klass_http = g_get_class(image_uma, "Gallop", "HttpHelper");
        if (klass_http) {
            void* a_compress = g_get_method_addr(klass_http, "CompressRequest", 1);
            void* a_decompress = g_get_method_addr(klass_http, "DecompressResponse", 1);
            
            if (a_compress && a_decompress) {
                void* hachimi = g_hachimi_instance();
                void* interceptor = g_hachimi_get_interceptor(hachimi);
                o_CompressRequest = (compress_req_t)g_interceptor_hook(interceptor, a_compress, (void*)h_CompressRequest);
                o_DecompressResponse = (decompress_resp_t)g_interceptor_hook(interceptor, a_decompress, (void*)h_DecompressResponse);
                Log("Gallop.HttpHelper hooks installed.");
            } else {
                Log("Failed to find CompressRequest or DecompressResponse addresses.");
            }
        }
    }
    
    // 2. Hook GameDefine.get_ApplicationServerUrl
    if (image_uma) {
        void* klass_gamedef = g_get_class(image_uma, "Gallop", "GameDefine");
        if (klass_gamedef) {
            void* a_get_url = g_get_method_addr(klass_gamedef, "get_ApplicationServerUrl", 0);
            if (a_get_url) {
                void* hachimi = g_hachimi_instance();
                void* interceptor = g_hachimi_get_interceptor(hachimi);
                o_GetApplicationServerUrl = (get_url_t)g_interceptor_hook(interceptor, a_get_url, (void*)h_GetApplicationServerUrl);
                Log("Gallop.GameDefine.get_ApplicationServerUrl hook installed.");
            } else {
                Log("Failed to find get_ApplicationServerUrl address.");
            }
        } else {
            Log("Failed to find Gallop.GameDefine.");
        }
    }
}

void* HookThread(void*) {
    usleep(8000000); // 8 seconds brute force wait
    OnGameInitialized();
    return nullptr;
}

extern "C" bool hachimi_init_v3(HachimiGetApiFn get_api, int version) {
    g_log = (hachimi_log_t)get_api("log");
    g_hachimi_instance = (hachimi_instance_t)get_api("hachimi_instance");
    g_hachimi_get_interceptor = (hachimi_get_interceptor_t)get_api("hachimi_get_interceptor");
    g_interceptor_hook = (interceptor_hook_t)get_api("interceptor_hook");
    
    g_get_assembly_image = (il2cpp_get_assembly_image_t)get_api("il2cpp_get_assembly_image");
    g_get_class = (il2cpp_get_class_t)get_api("il2cpp_get_class");
    g_get_method = (il2cpp_get_method_t)get_api("il2cpp_get_method");
    g_get_method_addr = (il2cpp_get_method_addr_t)get_api("il2cpp_get_method_addr");
    
    std::string pkg = GetPackageName();
    g_outputDir = "/sdcard/Android/media/" + pkg + "/hachimi/UmaProxy";
    EnsureDirectory(g_outputDir);
    
    Log("Uma-Proxy Plugin Initialized! Directory: " + g_outputDir);
    
    pthread_t t;
    pthread_create(&t, nullptr, HookThread, nullptr);
    pthread_detach(t);
    
    return true;
}
