#include "../include/dumper.hxx"
#include "../include/dumper_config.hxx"
#include "../include/il2cpp_api.hxx"
#include "../include/scene_dumper.hxx"
#include "../include/utils.hxx"
#include <cstring>
#include <string>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>

static void * g_il2cppThread = nullptr;
static bool g_gcRegistered = false;

typedef void* (*HachimiGetApiFn)(const char* name);
static bool (*g_gui_show_notification)(const char* message) = nullptr;

static bool RegisterThreadWithGC() {
    if (g_gcRegistered) return true;
    if (!api::gc_register_my_thread) return false;

    pthread_attr_t attr;
    pthread_getattr_np(pthread_self(), &attr);
    void* stack_addr;
    size_t stack_size;
    pthread_attr_getstack(&attr, &stack_addr, &stack_size);
    
    struct GCStackBase {
        void * mem_base;
        void * reg_base;
    } sb;
    sb.mem_base = (void*)((char*)stack_addr + stack_size); // top of stack
    sb.reg_base = nullptr;

    int rc = api::gc_register_my_thread(&sb);

    if (rc == 0 || rc == 1) {
        g_gcRegistered = true;
        Log("thread registered with GC");
        return true;
    }
    Log("GC_register_my_thread failed");
    return false;
}

static bool CheckIl2CppReady() {
    void* handle = dlopen("libil2cpp.so", RTLD_NOLOAD | RTLD_LAZY);
    if (!handle) {
        Log("[error] libil2cpp.so not loaded");
        return false;
    }

    if (!api::initialized)
        api::init();
    if (!api::initialized) {
        Log("[error] api init failed");
        return false;
    }

    if (!(api::get_domain && api::get_domain())) {
        Log("[error] domain not ready");
        return false;
    }

    return true;
}

static std::string GetPackageName() {
    char cmdline[256];
    FILE* f = fopen("/proc/self/cmdline", "r");
    if (f) {
        if (fgets(cmdline, sizeof(cmdline), f)) {
            fclose(f);
            return std::string(cmdline);
        }
        fclose(f);
    }
    return "unknown";
}

void* DumpThread(void* arg) {
    Log("DumpThread started. IL2CPP runtime should be ready.");

    // Minor delay to let the game settle after initialization
    usleep(3000000);

    if (CheckIl2CppReady()) {
        if (!g_il2cppThread && api::thread_attach) {
            void* domain = api::get_domain ? api::get_domain() : nullptr;
            if (domain) {
                g_il2cppThread = api::thread_attach(domain);
            }
            Log(g_il2cppThread ? "thread attached to runtime" : "attach failed");
        }

        {
            Dumper dumper;
            if (dumper.images.empty()) {
                Log("no assemblies found");
            } else {
                Log(std::to_string(dumper.images.size()) + " assemblies");
                dumper.DumpAllToFiles();
                
                if (g_gui_show_notification) {
                    g_gui_show_notification("IL2CPP Dump completed successfully!");
                }
            }
        }

        if (g_il2cppThread && api::thread_detach) {
            api::thread_detach(g_il2cppThread);
            g_il2cppThread = nullptr;
        }
    }

    Log("DumpThread finished.");
    return nullptr;
}

extern "C" __attribute__((visibility("default"))) bool hachimi_init_v3(HachimiGetApiFn get_api, int version) {
    if (get_api) {
        auto log_fn = (void (*)(int, const char*, const char*))get_api("log");
        SetHachimiLog(log_fn);
        g_gui_show_notification = (bool (*)(const char*))get_api("gui_show_notification");
    }

    std::string pkg = GetPackageName();
    g_outputDir = "/sdcard/Android/media/" + pkg + "/hachimi/DumpOutput";
    EnsureDirectory("/sdcard/Android/media/" + pkg + "/hachimi");
    EnsureDirectory(g_outputDir);

    Log("IL2CPP-Dumper Plugin Initialized! Output Dir: " + g_outputDir);

    auto register_init = (bool (*)(void (*)(void*), void*))get_api("hachimi_register_on_game_initialized");
    if (register_init) {
        Log("Using GameInitialized callback to spawn dump thread...");
        register_init([](void* userdata) {
            pthread_t t;
            pthread_create(&t, nullptr, DumpThread, nullptr);
            pthread_detach(t);
        }, nullptr);
    } else {
        Log("Fallback: Spawning dump thread immediately (might be unsafe)...");
        pthread_t t;
        pthread_create(&t, nullptr, DumpThread, nullptr);
        pthread_detach(t);
    }

    return true;
}
