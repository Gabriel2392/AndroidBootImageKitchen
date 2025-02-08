#include "log.h"

#include <android/log.h>
#include <jni.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#define LOG_TAG "ABIK"
JNIEnv *savedEnv = nullptr;
jclass dataHelperClass = nullptr;
jmethodID updateConsoleTextMethod = nullptr;
std::string CURRENT_LEVEL = std::string(LEVEL_INFO);

void initializeJNIReferences(JNIEnv *env, const std::string_view &level) {
  savedEnv = env;

  dataHelperClass = env->FindClass("com/oops/abik/DataHelper");
  if (dataHelperClass == nullptr) {
    ADLOG("DataHelper class not found");
    return;
  }

  updateConsoleTextMethod = env->GetStaticMethodID(
      dataHelperClass, "updateConsoleText", "(Ljava/lang/String;)V");
  if (updateConsoleTextMethod == nullptr) {
    ADLOG("updateConsoleText method not found");
    env->DeleteLocalRef(dataHelperClass);
    dataHelperClass = nullptr;
  }

  CURRENT_LEVEL = level;
}

void releaseJNIReferences() {
  if (dataHelperClass != nullptr && savedEnv != nullptr) {
    savedEnv->DeleteLocalRef(dataHelperClass);
  }
  savedEnv = nullptr;
  dataHelperClass = nullptr;
  updateConsoleTextMethod = nullptr;
  CURRENT_LEVEL = LEVEL_INFO;
}

void logConsole(const char *message) {
  if (savedEnv == nullptr || dataHelperClass == nullptr ||
      updateConsoleTextMethod == nullptr) {
    ADLOG("JNI environment or class references not initialized");
    return;
  }

  jstring jString = savedEnv->NewStringUTF(message);
  if (jString == nullptr) {
    ADLOG("Failed to create new jstring");
    return;
  }

  savedEnv->CallStaticVoidMethod(dataHelperClass, updateConsoleTextMethod,
                                 jString);
  savedEnv->DeleteLocalRef(jString);
}

void logMessage(const char *level, const char *format, ...) {
  va_list args;
  va_start(args, format);

  int size =
      vsnprintf(nullptr, 0, format, args) + 1;  // +1 for the null terminator
  va_end(args);

  if (size <= 0) {
    __android_log_print(ANDROID_LOG_ERROR, "ErosX", "Formatting error");
    return;
  }

  std::unique_ptr<char[]> buffer(new char[size]);

  va_start(args, format);
  vsnprintf(buffer.get(), size, format, args);
  va_end(args);

  std::string finalMessage = "[" + std::string(level) + "] " + buffer.get();

  logConsole(finalMessage.c_str());
}
