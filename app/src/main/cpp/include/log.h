#ifndef LOG_H
#define LOG_H

#include <cstdarg>
#include <cstdio>
#include <jni.h>

#define ADLOG(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

void logMessage(const char *level, const char *format, ...);
void initializeJNIReferences(JNIEnv *env);
void releaseJNIReferences();

#define LOGE(format, ...) logMessage("ERROR", format, ##__VA_ARGS__)
#define LOG(format, ...) logMessage("INFO", format, ##__VA_ARGS__)

#endif // LOG_H
