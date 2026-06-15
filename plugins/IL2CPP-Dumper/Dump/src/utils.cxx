#include "../include/utils.hxx"
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <iostream>
#include <string>

std::string g_outputDir;

std::size_t g_rvaTotal = 0;
std::size_t g_rvaResolved = 0;

static std::string BaseDir( ) {
    return g_outputDir.empty( ) ? std::string( "/sdcard/" ) : ( g_outputDir + "/" );
}

std::string GetDumpDirNormal( ) { return BaseDir( ) + "IL2CPP_Dump_Normal/"; }
std::string GetDumpDirAi( ) { return BaseDir( ) + "IL2CPP_Dump_AI/"; }

static void (*g_hachimi_log)(int level, const char* target, const char* message) = nullptr;

void SetHachimiLog(void (*log_fn)(int, const char*, const char*)) {
    g_hachimi_log = log_fn;
}

void Log( const std::string & msg ) {
    if (g_hachimi_log) {
        g_hachimi_log(2, "IL2CPPDumper", msg.c_str()); // 2 = INFO
    } else {
        std::cout << msg << "\n";
    }
}

void EnsureDirectory( const std::string & path ) {
    mkdir(path.c_str(), 0777);
}

std::string GetAccessModifier( uint32_t flags ) {
    uint32_t access = flags & 0x0007;

    switch ( access ) {
    case 0x0006:
        return "public";
    case 0x0001:
        return "private";
    case 0x0004:
        return "protected";
    case 0x0005:
        return "protected internal";
    default:
        return "private";
    }
}
