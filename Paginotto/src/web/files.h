#ifndef PALA_WEB_FILES_H
#define PALA_WEB_FILES_H

// Mounts the home page (`/`) and the book/folder browser (`/books`) plus the
// mutating endpoints they use: /del, /mkdir, /rmdir, /move, /rename, /jumppage.
void registerFilesRoutes();

#endif  // PALA_WEB_FILES_H
