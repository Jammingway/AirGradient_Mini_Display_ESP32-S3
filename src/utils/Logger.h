/**
 * @file Logger.h
 * Serial logger with levels and an optional sink so the boot terminal
 * can mirror status lines on screen.
 */
#pragma once
#include <Arduino.h>

namespace Logger {

enum class Level : uint8_t { Debug, Info, Warn, Error };

using Sink = void (*)(Level level, const char* tag, const char* message);

void setSink(Sink sink);
void log(Level level, const char* tag, const char* fmt, ...);

}  // namespace Logger

#define LOG_D(tag, ...) Logger::log(Logger::Level::Debug, tag, __VA_ARGS__)
#define LOG_I(tag, ...) Logger::log(Logger::Level::Info,  tag, __VA_ARGS__)
#define LOG_W(tag, ...) Logger::log(Logger::Level::Warn,  tag, __VA_ARGS__)
#define LOG_E(tag, ...) Logger::log(Logger::Level::Error, tag, __VA_ARGS__)
