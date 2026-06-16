#include <string>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <dlfcn.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <sys/stat.h>

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
    if (!g_outputDir.empty()) {
        std::ofstream ofs(g_outputDir + "/dumper.log", std::ios::out | std::ios::app);
        if (ofs.is_open()) {
            ofs << msg << "\n";
            ofs.close();
        }
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
    std::string cmd = "mkdir -p " + path;
    system(cmd.c_str());
}

bool CheckConfig() {
    std::string configPath = g_outputDir + "/config.txt";
    std::ifstream ifs(configPath);
    if (!ifs.is_open()) {
        EnsureDirectory(g_outputDir);
        std::ofstream ofs(configPath);
        ofs << "dump=true\n";
        return true;
    }
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.find("dump=false") != std::string::npos) {
            return false;
        }
    }
    return true;
}

// IL2CPP APIs
typedef void* (*il2cpp_domain_get_t)();
typedef void* (*il2cpp_domain_get_assemblies_t)(const void* domain, size_t* size);
typedef void* (*il2cpp_assembly_get_image_t)(const void* assembly);
typedef const char* (*il2cpp_image_get_name_t)(const void* image);
typedef void* (*il2cpp_class_from_name_t)(const void* image, const char* namespaze, const char* name);
typedef void* (*il2cpp_class_get_method_from_name_t)(void* klass, const char* name, int argsCount);
typedef void* (*il2cpp_object_new_t)(void* klass);
typedef void (*il2cpp_runtime_object_init_t)(void* obj);
typedef void* (*il2cpp_string_new_t)(const char* str);
typedef void* (*il2cpp_runtime_invoke_t)(void* method, void* obj, void** params, void** exc);
typedef const uint16_t* (*il2cpp_string_chars_t)(void* str);
typedef int32_t (*il2cpp_string_length_t)(void* str);
typedef int (*il2cpp_thread_attach_t)(void* domain);
typedef void* (*il2cpp_object_unbox_t)(void* obj);

static il2cpp_domain_get_t domain_get = nullptr;
static il2cpp_domain_get_assemblies_t domain_get_assemblies = nullptr;
static il2cpp_assembly_get_image_t assembly_get_image = nullptr;
static il2cpp_image_get_name_t image_get_name = nullptr;
static il2cpp_class_from_name_t class_from_name = nullptr;
static il2cpp_class_get_method_from_name_t class_get_method_from_name = nullptr;
static il2cpp_object_new_t object_new = nullptr;
static il2cpp_runtime_object_init_t runtime_object_init = nullptr;
static il2cpp_string_new_t string_new = nullptr;
static il2cpp_runtime_invoke_t runtime_invoke = nullptr;
static il2cpp_string_chars_t string_chars = nullptr;
static il2cpp_string_length_t string_length = nullptr;
static il2cpp_thread_attach_t thread_attach = nullptr;
static il2cpp_object_unbox_t object_unbox = nullptr;

bool InitIl2Cpp() {
    void* handle = dlopen("libil2cpp.so", RTLD_NOW);
    if (!handle) return false;
    domain_get = (il2cpp_domain_get_t)dlsym(handle, "il2cpp_domain_get");
    domain_get_assemblies = (il2cpp_domain_get_assemblies_t)dlsym(handle, "il2cpp_domain_get_assemblies");
    assembly_get_image = (il2cpp_assembly_get_image_t)dlsym(handle, "il2cpp_assembly_get_image");
    image_get_name = (il2cpp_image_get_name_t)dlsym(handle, "il2cpp_image_get_name");
    class_from_name = (il2cpp_class_from_name_t)dlsym(handle, "il2cpp_class_from_name");
    class_get_method_from_name = (il2cpp_class_get_method_from_name_t)dlsym(handle, "il2cpp_class_get_method_from_name");
    object_new = (il2cpp_object_new_t)dlsym(handle, "il2cpp_object_new");
    runtime_object_init = (il2cpp_runtime_object_init_t)dlsym(handle, "il2cpp_runtime_object_init");
    string_new = (il2cpp_string_new_t)dlsym(handle, "il2cpp_string_new");
    runtime_invoke = (il2cpp_runtime_invoke_t)dlsym(handle, "il2cpp_runtime_invoke");
    string_chars = (il2cpp_string_chars_t)dlsym(handle, "il2cpp_string_chars");
    string_length = (il2cpp_string_length_t)dlsym(handle, "il2cpp_string_length");
    thread_attach = (il2cpp_thread_attach_t)dlsym(handle, "il2cpp_thread_attach");
    object_unbox = (il2cpp_object_unbox_t)dlsym(handle, "il2cpp_object_unbox");
    return true;
}

std::string UTF16ToUTF8(const uint16_t* wstr, int len) {
    std::string out;
    for (int i = 0; i < len; ++i) {
        uint16_t c = wstr[i];
        if (c < 0x80) out += (char)c;
        else if (c < 0x800) {
            out += (char)(0xC0 | (c >> 6));
            out += (char)(0x80 | (c & 0x3F));
        } else {
            out += (char)(0xE0 | (c >> 12));
            out += (char)(0x80 | ((c >> 6) & 0x3F));
            out += (char)(0x80 | (c & 0x3F));
        }
    }
    return out;
}

std::string GetIl2CppString(void* str) {
    if (!str) return "NULL";
    const uint16_t* chars = string_chars(str);
    int len = string_length(str);
    return UTF16ToUTF8(chars, len);
}

void EscapeCSV(std::string& str) {
    bool needsQuotes = false;
    for (char c : str) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) return;

    std::string escaped = "\"";
    for (char c : str) {
        if (c == '"') escaped += "\"\"";
        else escaped += c;
    }
    escaped += "\"";
    str = escaped;
}

void DumpDatabase(const std::string& dbPath, const std::string& outFolder) {
    size_t size = 0;
    void* domain = domain_get();
    void** assemblies = (void**)domain_get_assemblies(domain, &size);
    void* targetImage = nullptr;

    for (size_t i = 0; i < size; ++i) {
        void* img = assembly_get_image(assemblies[i]);
        if (std::string(image_get_name(img)) == "LibNative.Runtime.dll") {
            targetImage = img;
            break;
        }
    }

    if (!targetImage) {
        Log("Failed to find umamusume.dll image");
        return;
    }

    void* connClass = class_from_name(targetImage, "LibNative.Sqlite3", "Connection");
    void* queryClass = class_from_name(targetImage, "LibNative.Sqlite3", "Query");

    if (!connClass || !queryClass) {
        Log("Failed to find Sqlite3 classes");
        return;
    }

    void* mOpen = class_get_method_from_name(connClass, "Open", 4);
    void* mQuery = class_get_method_from_name(connClass, "Query", 1);
    void* mClose = class_get_method_from_name(connClass, "CloseDB", 0);

    void* mStep = class_get_method_from_name(queryClass, "Step", 0);
    void* mGetText = class_get_method_from_name(queryClass, "GetText", 1);
    void* mDispose = class_get_method_from_name(queryClass, "Dispose", 0);

    if (!mOpen || !mQuery || !mStep || !mGetText) {
        Log("Failed to find Sqlite3 methods");
        return;
    }

    void* conn = object_new(connClass);
    runtime_object_init(conn);

    void* pathStr = string_new(dbPath.c_str());
    int flags = 0; // SQLITE_OPEN_READONLY ? (Usually 1)
    
    // public bool Open(string path, string _vfs, string _password, int flags);
    // wait, the signature in rust is bool Open(string path, object flags, object vfs, int open_flags);
    // Let's pass 1 (READONLY) for open_flags
    void* argsOpen[4] = { pathStr, nullptr, nullptr, &flags };
    
    void* exc = nullptr;
    void* retOpen = runtime_invoke(mOpen, conn, argsOpen, &exc);
    if (exc) {
        Log("Exception calling Open!");
        return;
    }

    bool opened = *(bool*)object_unbox(retOpen);
    if (!opened) {
        Log("Failed to open database: " + dbPath);
        return;
    }

    Log("Database opened: " + dbPath);
    EnsureDirectory(outFolder);

    // Get all tables
    void* querySql = string_new("SELECT name FROM sqlite_master WHERE type='table'");
    void* argsQuery[1] = { querySql };
    void* qTables = runtime_invoke(mQuery, conn, argsQuery, &exc);

    std::vector<std::string> tables;
    if (!exc && qTables) {
        while (true) {
            void* stepRet = runtime_invoke(mStep, qTables, nullptr, nullptr);
            if (!*(bool*)object_unbox(stepRet)) break;

            int colIdx = 0;
            void* argsGet[1] = { &colIdx };
            void* txtRet = runtime_invoke(mGetText, qTables, argsGet, nullptr);
            std::string tName = GetIl2CppString(txtRet);
            if (!tName.empty() && tName != "sqlite_sequence" && tName != "android_metadata") {
                tables.push_back(tName);
            }
        }
        runtime_invoke(mDispose, qTables, nullptr, nullptr);
    }

    Log("Found " + std::to_string(tables.size()) + " tables");

    for (const auto& tName : tables) {
        Log("Dumping table: " + tName);
        
        // Get columns
        std::string pragmaStr = "PRAGMA table_info('" + tName + "')";
        void* pSql = string_new(pragmaStr.c_str());
        void* argsPragma[1] = { pSql };
        void* qPragma = runtime_invoke(mQuery, conn, argsPragma, nullptr);

        std::vector<std::string> cols;
        if (qPragma) {
            while (true) {
                if (!*(bool*)object_unbox(runtime_invoke(mStep, qPragma, nullptr, nullptr))) break;
                int nameIdx = 1;
                void* argsGetName[1] = { &nameIdx };
                cols.push_back(GetIl2CppString(runtime_invoke(mGetText, qPragma, argsGetName, nullptr)));
            }
            runtime_invoke(mDispose, qPragma, nullptr, nullptr);
        }

        if (cols.empty()) continue;

        std::ofstream out(outFolder + "/" + tName + ".csv");
        for (size_t i = 0; i < cols.size(); ++i) {
            std::string c = cols[i];
            EscapeCSV(c);
            out << c << (i == cols.size() - 1 ? "" : ",");
        }
        out << "\n";

        // Get data
        std::string selStr = "SELECT * FROM '" + tName + "'";
        void* selSql = string_new(selStr.c_str());
        void* argsSel[1] = { selSql };
        void* qData = runtime_invoke(mQuery, conn, argsSel, nullptr);

        if (qData) {
            while (true) {
                if (!*(bool*)object_unbox(runtime_invoke(mStep, qData, nullptr, nullptr))) break;

                for (size_t i = 0; i < cols.size(); ++i) {
                    int cIdx = i;
                    void* argsGetData[1] = { &cIdx };
                    void* retTxt = runtime_invoke(mGetText, qData, argsGetData, nullptr);
                    std::string val = GetIl2CppString(retTxt);
                    EscapeCSV(val);
                    out << val << (i == cols.size() - 1 ? "" : ",");
                }
                out << "\n";
            }
            runtime_invoke(mDispose, qData, nullptr, nullptr);
        }
    }

    runtime_invoke(mClose, conn, nullptr, nullptr);
    Log("Finished dumping " + dbPath);
}

void* DumpThread(void*) {
    usleep(5000000); 

    if (!InitIl2Cpp()) {
        Log("Failed to initialize IL2CPP");
        return nullptr;
    }

    thread_attach(domain_get());

    std::string pkg = GetPackageName();
    std::string baseDataPath = "/data/user/0/" + pkg + "/files";
    std::string altDataPath = "/data/data/" + pkg + "/files";
    
    std::vector<std::string> targets = {
        "/master/master.mdb",
        "/meta"
    };
    
    for (const auto& targetPath : targets) {
        std::string name = targetPath.substr(targetPath.find_last_of('/') + 1);
        std::string outFolder = g_outputDir + "/" + name + "_csv";
        
        std::string p1 = baseDataPath + targetPath;
        std::string p2 = altDataPath + targetPath;
        
        FILE* f = fopen(p1.c_str(), "rb");
        if (f) {
            fclose(f);
            DumpDatabase(p1, outFolder);
        } else {
            f = fopen(p2.c_str(), "rb");
            if (f) {
                fclose(f);
                DumpDatabase(p2, outFolder);
            } else {
                Log("File not found: " + p1);
            }
        }
    }
    
    if (g_gui_show_notification) {
        g_gui_show_notification("Data-Dumper: Successfully dumped databases to CSV!");
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
    EnsureDirectory(g_outputDir);
    
    if (!CheckConfig()) {
        Log("Data-Dumper is disabled via config.txt");
        return true;
    }
    
    Log("Data-Dumper Plugin Initialized! Output Dir: " + g_outputDir);

    pthread_t t;
    pthread_create(&t, nullptr, DumpThread, nullptr);
    pthread_detach(t);

    return true;
}
