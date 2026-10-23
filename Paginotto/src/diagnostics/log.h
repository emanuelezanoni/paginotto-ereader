#ifndef PALA_DIAGNOSTICS_LOG_H
#define PALA_DIAGNOSTICS_LOG_H

namespace Diagnostics {

void begin();
void info(const char* event);
void warning(const char* event);
void error(const char* event);
void errorCode(const char* event, int code);

}  // namespace Diagnostics

#endif  // PALA_DIAGNOSTICS_LOG_H
