#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include "json.hpp"

using json = nlohmann::json;

typedef void* (*HachimiGetApiFn)(const char* name);

typedef void* (*hachimi_instance_t)();
typedef void* (*hachimi_get_interceptor_t)(void* hachimi);
typedef void* (*interceptor_hook_t)(void* interceptor, void* orig_addr, void* hook_addr);

typedef void* (*il2cpp_get_assembly_image_t)(const char* assembly_name);
typedef void* (*il2cpp_get_class_t)(void* image, const char* namespaze, const char* name);
typedef void* (*il2cpp_get_method_t)(void* klass, const char* name, int argsCount);
typedef void* (*il2cpp_get_method_addr_t)(void* method);

typedef void (*hachimi_log_t)(int level, const char* tag, const char* message);
typedef void (*hachimi_register_on_game_initialized_t)(void (*callback)());

static hachimi_log_t g_log = nullptr;
static hachimi_instance_t g_hachimi_instance = nullptr;
static hachimi_get_interceptor_t g_hachimi_get_interceptor = nullptr;
static interceptor_hook_t g_interceptor_hook = nullptr;

static il2cpp_get_assembly_image_t g_get_assembly_image = nullptr;
static il2cpp_get_class_t g_get_class = nullptr;
static il2cpp_get_method_t g_get_method = nullptr;
static il2cpp_get_method_addr_t g_get_method_addr = nullptr;

static std::string g_outputDir;
static int g_seq = 0;

void Log(const std::string& msg) {
    if (g_log) g_log(2, "PacketCapture", msg.c_str());
}

std::string GetPackageName() {
    char cmdline[256] = {0};
    FILE* fp = fopen("/proc/self/cmdline", "r");
    if (fp) {
        fgets(cmdline, sizeof(cmdline), fp);
        fclose(fp);
        return std::string(cmdline);
    }
    return "jp.co.cygames.umamusume";
}

void EnsureDirectory(const std::string& path) {
    std::string cmd = "mkdir -p " + path;
    system(cmd.c_str());
}

void DumpByteArray(const std::string& prefix, void* arrayObj) {
    if (!arrayObj) return;
    size_t length = *(size_t*)((char*)arrayObj + 0x18);
    if (length == 0 || length > (64u * 1024 * 1024)) return;
    
    char* data = (char*)arrayObj + 0x20;
    
    g_seq++;
    
    try {
        std::vector<uint8_t> v((uint8_t*)data, (uint8_t*)data + length);
        json j = json::from_msgpack(v);
        
        char filename[256];
        snprintf(filename, sizeof(filename), "%s/%s_%05d.json", g_outputDir.c_str(), prefix.c_str(), g_seq);
        
        std::ofstream out(filename);
        if (out.is_open()) {
            out << j.dump(4);
            out.close();
        }
    } catch (const std::exception& e) {
        Log("Failed to parse msgpack: " + std::string(e.what()) + ". Falling back to .bin.");
        char filename[256];
        snprintf(filename, sizeof(filename), "%s/%s_%05d.bin", g_outputDir.c_str(), prefix.c_str(), g_seq);
        
        std::ofstream out(filename, std::ios::binary);
        if (out.is_open()) {
            out.write(data, length);
            out.close();
        }
    }
}

// Trampolines
typedef void* (*compress_req_t)(void* body, void* method_info);
typedef void* (*decompress_resp_t)(void* response, void* method_info);

static compress_req_t o_CompressRequest = nullptr;
static decompress_resp_t o_DecompressResponse = nullptr;

static void* h_CompressRequest(void* body, void* method_info) {
    DumpByteArray("compreq_in", body);
    void* ret = o_CompressRequest(body, method_info);
    DumpByteArray("compreq_out", ret);
    return ret;
}

static void* h_DecompressResponse(void* response, void* method_info) {
    DumpByteArray("decresp_in", response);
    void* ret = o_DecompressResponse(response, method_info);
    DumpByteArray("decresp_out", ret);
    return ret;
}

void OnGameInitialized() {
    Log("Game initialized, setting up network hooks...");
    
    void* image = g_get_assembly_image("umamusume.dll");
    if (!image) {
        Log("Failed to get umamusume.dll image");
        return;
    }
    
    void* klass = g_get_class(image, "Gallop", "HttpHelper");
    if (!klass) {
        Log("Failed to get Gallop.HttpHelper");
        return;
    }
    
    void* m_compress = g_get_method(klass, "CompressRequest", 1);
    void* m_decompress = g_get_method(klass, "DecompressResponse", 1);
    
    if (!m_compress || !m_decompress) {
        Log("Failed to find CompressRequest or DecompressResponse");
        return;
    }
    
    void* a_compress = g_get_method_addr(m_compress);
    void* a_decompress = g_get_method_addr(m_decompress);
    
    if (!a_compress || !a_decompress) {
        Log("Failed to get method addresses");
        return;
    }
    
    void* hachimi = g_hachimi_instance();
    void* interceptor = g_hachimi_get_interceptor(hachimi);
    
    o_CompressRequest = (compress_req_t)g_interceptor_hook(interceptor, a_compress, (void*)h_CompressRequest);
    o_DecompressResponse = (decompress_resp_t)g_interceptor_hook(interceptor, a_decompress, (void*)h_DecompressResponse);
    
    if (o_CompressRequest && o_DecompressResponse) {
        Log("Network Hooks installed successfully!");
    } else {
        Log("Failed to install network hooks via Hachimi Interceptor.");
    }
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
    
    hachimi_register_on_game_initialized_t register_init = 
        (hachimi_register_on_game_initialized_t)get_api("hachimi_register_on_game_initialized");

    std::string pkg = GetPackageName();
    g_outputDir = "/sdcard/Android/media/" + pkg + "/hachimi/PacketCapture";
    EnsureDirectory(g_outputDir);
    
    Log("Packet-Capture Plugin Initialized! Output Dir: " + g_outputDir);
    
    if (register_init) {
        register_init(OnGameInitialized);
    } else {
        Log("Failed to get hachimi_register_on_game_initialized");
    }
    
    return true;
}
