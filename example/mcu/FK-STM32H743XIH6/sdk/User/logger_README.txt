Logger: C++ implementation, C ABI. Task context only; no heap allocation.
32 records, 224 bytes per record. INFO default. Output OFF default.
Log_Write(LOG_INFO, "IMU", "samples=%lu", (unsigned long)count);
Wire: [64-bit milliseconds][D/I/W/E][module up to 16 chars] message CRLF
Commands: log on / off / status / debug / info / warn / error / quiet / test
log on selects the command UART. Do not enable text output during binary file transfers.
Main_Task polls every 50 ms, maximum 2 records/call (40 records/s).
Full queue drops newest record. Busy TX retains oldest record for retry.
Statistics: queued, peak, written (accepted), dropped, truncated, filtered, sinkBusy.
Log_Init is startup-only. Log_Poll has exactly one consumer.
No ISR formatting; ISR attempts are ignored. Use counters/events and log from tasks.
Future RTOS: route all UART producers through one TX owner or protect the whole
space-check/enqueue operation with the same UART mutex. C library formatting
reentrancy must be configured/audited before concurrent task logging.
RAM buffer only; no W25/FatFs persistence or fault-time synchronous flushing.
Existing command replies and legacy UART_Printf remain unchanged.

Firmware format customization:
1. Edit LOG_DEFAULT_FORMAT in logger_config.h. Arguments in order are timestamp
   (unsigned long long), level (char), module (const char*), message (const char*).
2. For arbitrary field order/layout, call Log_SetFormatter(callback) after Log_Init.
   Callback follows snprintf length semantics; negative return discards the record.
   NULL restores the compiled default. Register startup-only; callback code must
   remain valid, bounded, nonblocking, and must not call Log_Write recursively.
   Message is already formatted/truncated to 223 bytes. Final output is at most
   221 content bytes + CRLF. The logger appends CRLF and sanitizes embedded CR/LF.
   Already queued records retain the format used when written.
   Larger/custom binary records require a separate transport API.
