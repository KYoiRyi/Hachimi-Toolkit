#include <string>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <iostream>
#include <vector>

typedef void* (*HachimiGetApiFn)(const char* name);
static bool (*g_gui_show_notification)(const char* message) = nullptr;
static void (*g_hachimi_log)(int level, const char* target, const char* message) = nullptr;

std::string g_outputDir;

void Log(const std::string& msg) {
    if (g_hachimi_log) {
        g_hachimi_log(2, "DataDumper", msg.c_str());
    } else {
        std::cout << "[DataDumper] " << msg << "\n";
    }
}

std::string GetPackageName() {
    std::ifstream cmdline("/proc/self/cmdline");
    std::string name;
    if (cmdline.is_open()) {
        std::getline(cmdline, name, '\0');
    }
    return name.empty() ? "jp.co.cygames.umamusume" : name;
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

bool CopyFile(const std::string& src, const std::string& dst) {
    FILE* fi = fopen(src.c_str(), "rb");
    if (!fi) return false;
    FILE* fo = fopen(dst.c_str(), "wb");
    if (!fo) {
        fclose(fi);
        return false;
    }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fi)) > 0) {
        fwrite(buf, 1, n, fo);
    }
    fclose(fi);
    fclose(fo);
    return true;
}

void* DumpThread(void*) {
    // Wait a bit for the game to initialize files (master.mdb might be decrypted/copied from APK on first start)
    usleep(5000000); 
    
    std::string pkg = GetPackageName();
    std::string baseDataPath = "/data/user/0/" + pkg + "/files";
    std::string altDataPath = "/data/data/" + pkg + "/files";
    
    std::vector<std::string> targets = {
        "/master/master.mdb",
        "/meta"
    };
    
    int successCount = 0;
    
    for (const auto& targetPath : targets) {
        std::string filename = targetPath.substr(targetPath.find_last_of('/') + 1);
        std::string outPath = g_outputDir + "/" + filename;
        
        std::string target = baseDataPath + targetPath;
        Log("Trying to dump: " + target);
        
        if (CopyFile(target, outPath)) {
            Log("Successfully dumped to: " + outPath);
            successCount++;
        } else {
            // Try fallback path
            std::string altTarget = altDataPath + targetPath;
            Log("Fallback trying: " + altTarget);
            if (CopyFile(altTarget, outPath)) {
                Log("Successfully dumped to: " + outPath);
                successCount++;
            } else {
                Log("Failed to dump " + filename);
            }
        }
    }
    
    if (g_gui_show_notification) {
        if (successCount > 0) {
            std::string notif = "Data-Dumper: Successfully dumped " + std::to_string(successCount) + " file(s)!";
            g_gui_show_notification(notif.c_str());
        } else {
            g_gui_show_notification("Data-Dumper Error: Could not find master.mdb or meta!");
        }
    }
    
    return nullptr;
}

extern "C" __attribute__((visibility("default"))) bool hachimi_init_v3(HachimiGetApiFn get_api, int version) {
    if (get_api) {
        g_hachimi_log = (void (*)(int, const char*, const char*))get_api("log");
        g_gui_show_notification = (bool (*)(const char*))get_api("gui_show_notification");
    }

    std::string pkg = GetPackageName();
    g_outputDir = "/sdcard/Android/media/" + pkg + "/hachimi/DataDump";
    EnsureDirectory("/sdcard/Android/media/" + pkg + "/hachimi");
    EnsureDirectory(g_outputDir);

    Log("Data-Dumper Plugin Initialized! Output Dir: " + g_outputDir);

    pthread_t t;
    pthread_create(&t, nullptr, DumpThread, nullptr);
    pthread_detach(t);

    return true;
}
