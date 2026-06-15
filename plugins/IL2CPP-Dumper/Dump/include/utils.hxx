#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <stdint.h>

void SetHachimiLog(void (*log_fn)(int, const char*, const char*));

extern std::string g_outputDir;

extern std::size_t g_rvaTotal;
extern std::size_t g_rvaResolved;

void Log( const std::string & msg );

void EnsureDirectory( const std::string & path );

std::string GetAccessModifier( uint32_t flags );

std::string GetDumpDirNormal( );
std::string GetDumpDirAi( );

#endif // UTILS_H
