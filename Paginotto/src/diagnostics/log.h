#ifndef PALA_DIAGNOSTICS_LOG_H
#define PALA_DIAGNOSTICS_LOG_H

#include <Arduino.h>

namespace Diagnostics {

void begin();
void trace(const char* event);
void checkpoint(const char* phase);
void info(const char* event);
void warning(const char* event);
void error(const char* event);
void errorCode(const char* event, int code);
String persistentText();
String runtimeText();
void clearRuntime();

}  // namespace Diagnostics

#endif  // PALA_DIAGNOSTICS_LOG_H
