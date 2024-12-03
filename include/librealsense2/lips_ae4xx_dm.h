/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2024 LIPS Corporation. All Rights Reserved. */

#ifndef LIPS_AE4XX_DM_H
#define LIPS_AE4XX_DM_H

// you have to link LIPS library libbackend-ethernet
//
// input args:
// - model_name : AE4xx model name, now we support "ae400" for AE400/450 and "ae430" for AE430/470
// - ip : camera's ip address
// return:
// - 0 : device reboot ok
// - not 0 (-1 or 1) : device reboot failed, device not found or reboot operation failed
//
int ae4xx_restart(const char* model_name, const char* ip);

#endif //LIPS_AE4XX_DM_H