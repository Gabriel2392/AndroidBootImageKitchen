#ifndef LOG_H
#define LOG_H

#include <jni.h>

#include <cstdarg>
#include <cstdio>
#include <string>

#define ADLOG(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

void logMessage(const char *level, const char *format, ...);
void initializeJNIReferences(JNIEnv *env, const std::string_view &level);
void releaseJNIReferences();

constexpr std::string_view LEVEL_ERROR = "ERROR";
constexpr std::string_view LEVEL_BUILD = "MAKE";
constexpr std::string_view LEVEL_EXTRACT = "UNPACK";
constexpr std::string_view LEVEL_INFO = "INFO";
extern std::string CURRENT_LEVEL;

#define LOGE(format, ...) logMessage(std::string(LEVEL_ERROR).c_str(), format, ##__VA_ARGS__)
#define LOG(format, ...) logMessage(CURRENT_LEVEL.c_str(), format, ##__VA_ARGS__)

#endif  // LOG_H
