#ifndef LOGGER_CONFIG_H
#define LOGGER_CONFIG_H
/* printf arguments: timestamp (unsigned long long), level (char), module, message.
 * Keep argument types/order compatible. Use Log_SetFormatter for other layouts.
 * CRLF is appended by the logger; do not include line endings here.
 */
#ifndef LOG_DEFAULT_FORMAT
#define LOG_DEFAULT_FORMAT "[%llu][%c][%.16s] %s"
#endif
#endif
