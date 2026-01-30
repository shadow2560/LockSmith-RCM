#ifndef _FUSE_CHECK_H_
#define _FUSE_CHECK_H_

#include <utils/types.h>

bool detect_firmware_from_nca(u8 *major, u8 *minor, u8 *patch);
void fuse_check();

#endif