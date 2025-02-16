// Copyright 2019 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "iree/base/logging.h"

#include <string>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "iree/base/internal/flags.h"
#include "iree/base/tracing.h"

IREE_FLAG(int32_t, iree_minloglevel, 0,
          "Minimum logging level. 0 = INFO and above.");
IREE_FLAG(int32_t, iree_v, 0,
          "Verbosity level maximum. 1 = IREE_VLOG(0-1), 2 = IREE_VLOG(0-2).");

namespace iree {
namespace internal {

namespace {

// Parse log level (int64_t) from environment variable (char*).
// Returns true if the value was present and parsed successfully.
bool LogLevelStrToInt(const char* iree_env_var_val, int64_t* out_level) {
  *out_level = 0;
  if (iree_env_var_val == nullptr) {
    return false;
  }

  std::string min_log_level(iree_env_var_val);
  std::istringstream ss(min_log_level);
  int64_t level;
  if (!(ss >> level)) {
    // Invalid vlog level setting, set level to default (0).
    return false;
  }

  *out_level = level;
  return true;
}

int64_t MinLogLevelFromEnv() {
  const char* iree_env_var_val = getenv("IREE_MIN_LOG_LEVEL");
  int64_t level = 0;
  if (LogLevelStrToInt(iree_env_var_val, &level)) {
    return level;
  }
  return FLAG_iree_minloglevel;
}

int64_t MinVLogLevelFromEnv() {
  const char* iree_env_var_val = getenv("IREE_MIN_VLOG_LEVEL");
  int64_t level = 0;
  if (LogLevelStrToInt(iree_env_var_val, &level)) {
    return level;
  }
  return FLAG_iree_v;
}

}  // namespace

LogMessage::LogMessage(const char* file_name, int line, int severity)
    : file_name_(file_name), line_(line), severity_(severity) {}

LogMessage::~LogMessage() {
  // Read the min log level once during the first call to logging.
  static int64_t min_log_level = MinLogLevelFromEnv();
  if (IREE_LIKELY(severity_ >= min_log_level)) {
    EmitLogMessage();
  }
}

int64_t LogMessage::MinVLogLevel() {
  static int64_t min_vlog_level = MinVLogLevelFromEnv();
  return min_vlog_level;
}

void LogMessage::EmitLogMessage() {
  // TODO(scotttodd): Include current system time
  fprintf(stderr, "%c %s:%d] %s\n", "IWEF"[severity_], file_name_, line_,
          str().c_str());

#if defined(__ANDROID__)
  // Define equivalent android log levels to map to IREE.
  constexpr int kStatusToAndroidLevel[4] = {
      4,  // Android info
      5,  // Android waring
      6,  // Android error
      6   // Android fatal (doesn't exist, so reusing error)
  };

  // NOTE: this truncates. That's fine for now and stderr is still usable.
  int android_severity = kStatusToAndroidLevel[severity_];
  {
    // NOTE: this truncates. That's fine for now and stderr is still usable.
    char str_buffer[512];
    snprintf(str_buffer, sizeof(str_buffer), "%s:%d] %s\n", file_name_, line_,
             str().c_str());
    __android_log_write(android_severity, "native", str_buffer);
  }
#endif  // !defined(__ANDROID__)

#if IREE_TRACING_FEATURES & IREE_TRACING_FEATURE_LOG_MESSAGES
  constexpr int kLevelColors[4] = {
      IREE_TRACING_MESSAGE_LEVEL_INFO,     // INFO
      IREE_TRACING_MESSAGE_LEVEL_WARNING,  // WARNING
      IREE_TRACING_MESSAGE_LEVEL_ERROR,    // ERROR
      IREE_TRACING_MESSAGE_LEVEL_ERROR,    // FATAL
  };
  {
    // NOTE: this truncates. That's fine for now and stderr is still usable.
    char str_buffer[512];
    int str_length = snprintf(str_buffer, sizeof(str_buffer), "%s:%d] %s\n",
                              file_name_, line_, str().c_str());
    IREE_TRACE_MESSAGE_DYNAMIC_COLORED(kLevelColors[severity_], str_buffer,
                                       str_length);
  }
#endif  // IREE_TRACING_FEATURES& IREE_TRACING_FEATURE_LOG_MESSAGES
}

LogMessageFatal::LogMessageFatal(const char* file, int line)
    : LogMessage(file, line, FATAL) {}

LogMessageFatal::~LogMessageFatal() {
  EmitLogMessage();

  // abort() ensures we don't return (as promised via ATTRIBUTE_NORETURN).
  abort();
}

template <>
void MakeCheckOpValueString(std::ostream* os, const char& v) {
  if (v >= 32 && v <= 126) {
    (*os) << "'" << v << "'";
  } else {
    (*os) << "char value " << static_cast<int16_t>(v);
  }
}

template <>
void MakeCheckOpValueString(std::ostream* os, const int8_t& v) {
  if (v >= 32 && v <= 126) {
    (*os) << "'" << v << "'";
  } else {
    (*os) << "signed char value " << static_cast<int16_t>(v);
  }
}

template <>
void MakeCheckOpValueString(std::ostream* os, const uint8_t& v) {
  if (v >= 32 && v <= 126) {
    (*os) << "'" << v << "'";
  } else {
    (*os) << "unsigned char value " << static_cast<uint16_t>(v);
  }
}

template <>
void MakeCheckOpValueString(std::ostream* os, const std::nullptr_t& v) {
  (*os) << "nullptr";
}

CheckOpMessageBuilder::CheckOpMessageBuilder(const char* exprtext)
    : stream_(new std::ostringstream) {
  *stream_ << "Check failed: " << exprtext << " (";
}

CheckOpMessageBuilder::~CheckOpMessageBuilder() { delete stream_; }

std::ostream* CheckOpMessageBuilder::ForVar2() {
  *stream_ << " vs. ";
  return stream_;
}

std::string* CheckOpMessageBuilder::NewString() {
  *stream_ << ")";
  return new std::string(stream_->str());
}

}  // namespace internal
}  // namespace iree
