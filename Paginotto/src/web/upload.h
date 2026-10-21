#ifndef PALA_WEB_UPLOAD_H
#define PALA_WEB_UPLOAD_H

#include <stdint.h>

// Mounts /upload (book upload, multipart) and /upload-sleep (custom sleep
// image upload). Both register a stream handler (chunk receiver) and a done
// handler (final response).
void registerUploadRoutes();

// Close any open tmp file and clear all per-session fields. Called by the
// upload screen at session start/stop so a stale field from a prior session
// can't leak through. Storage lives file-static inside upload.cpp.
void resetBookUpload();
void resetSleepUpload();

// Activity watchdog for the device WebUI session. Web pages and upload
// handlers call this whenever the browser is actually using the session.
void notifyUploadActivity();
uint32_t uploadLastActivityMs();

#endif  // PALA_WEB_UPLOAD_H
