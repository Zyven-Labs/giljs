/* load.h -- gil_load / gil_load_file entry points (internal) */

#ifndef GIL_LOAD_H
#define GIL_LOAD_H

#include "script.h"

GilScript* load_from_source(const char *source, const char **error);
GilScript* load_from_file(const char *path, const char **error);

#endif /* GIL_LOAD_H */