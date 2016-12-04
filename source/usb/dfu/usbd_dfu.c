/**
 * @file    usbd_dfu.c
 * @brief   Device Firmware Upgrade class driver
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2009-2016, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "string.h"

#include "RTL.h"
#include "rl_usb.h"
#include "usb_for_lib.h"

static volatile U8  dfu_current_state;
static volatile U8  dfu_current_status;
static U16 dfu_current_transfer_size;

void DFU_Reset(void) {
    dfu_current_state = DFU_STATE_DFU_IDLE;
    dfu_current_status = DFU_STATUS_OK;
    dfu_current_transfer_size = 0;
}

void DFU_SetState(U8 state) {
    dfu_current_state = state;
}

void DFU_SetStatus(U8 status) {
    if (status == DFU_STATUS_OK) {
        if (dfu_current_state == DFU_STATE_DFU_ERROR) {
            DFU_SetState(DFU_STATE_DFU_IDLE);
        }
    } else if (dfu_current_state != DFU_STATE_DFU_ERROR) {
        DFU_SetState(DFU_STATE_DFU_ERROR);
        USBD_DFU_SignalAbort(__TRUE);
    }
    dfu_current_status = status;
}

BOOL USBD_DFU_GetStatus(void) {
    DFU_GETSTATUS_RESPONSE *resp = (DFU_GETSTATUS_RESPONSE *)(USBD_EP0Buf);
    U32 bwPollTimeout = 0;
    static BOOL started = __FALSE;

    if (dfu_current_state == DFU_STATE_DFU_DNLOAD_SYNC) {
        DFU_SetState(DFU_STATE_DFU_DNBUSY);
        bwPollTimeout = 100;
        started = __FALSE;
    } else if (dfu_current_state == DFU_STATE_DFU_DNBUSY) {
        // User is responsible for setting state to DFU_STATE_DNLOAD_IDLE on success
        if (!started) {
            USBD_DFU_WriteBlock(USBD_DFU_TransferBuf, dfu_current_transfer_size);
            started = __TRUE;
        }
        bwPollTimeout = 100;
    } else if (dfu_current_state == DFU_STATE_DFU_MANIFEST_SYNC) {
        DFU_SetState(DFU_STATE_DFU_MANIFEST);
        started = __FALSE;
        bwPollTimeout = 100;
    } else if (dfu_current_state == DFU_STATE_DFU_MANIFEST) {
        // User is responsible for setting state to DFU_STATE_DFU_IDLE on success
        bwPollTimeout = 100;
        if (!started) {
            USBD_DFU_FinishUpgrade();
            started = __TRUE;
        }
    }

    resp->bStatus = dfu_current_status;
    resp->bwPollTimeout[0] = (uint8_t)(bwPollTimeout & 0xFF);
    resp->bwPollTimeout[1] = (uint8_t)((bwPollTimeout >> 8) & 0xFF);
    resp->bwPollTimeout[2] = (uint8_t)((bwPollTimeout >> 16) & 0xFF);
    resp->bState = dfu_current_state;
    resp->iString = 0;

    return (__TRUE);
}

BOOL USBD_DFU_StartDnload(U16 transferSize) {
    if (transferSize > usbd_dfu_transfersize) {
        DFU_SetStatus(DFU_STATUS_ERR_STALLEDPKT);
        return (__FALSE);
    } else {
        if (dfu_current_state == DFU_STATE_DFU_IDLE) {
            if (transferSize > 0) {
                USBD_DFU_StartUpgrade();
                dfu_current_transfer_size = transferSize;
                return (__TRUE);
            } else {
                // First block can't be empty
                DFU_SetStatus(DFU_STATUS_ERR_STALLEDPKT);
                return (__FALSE);
            }
        } else if (dfu_current_state == DFU_STATE_DFU_DNLOAD_IDLE) {
            if (transferSize > 0) {
                dfu_current_transfer_size = transferSize;
            } else {
                // Last block
                DFU_SetState(DFU_STATE_DFU_MANIFEST_SYNC);
            }
            return (__TRUE);
        } else {
            return (__FALSE);
        }
    }
}

BOOL USBD_DFU_SyncDnload(void) {
    DFU_SetState(DFU_STATE_DFU_DNLOAD_SYNC);
    return (__TRUE);
}

BOOL USBD_DFU_GetState(void) {
    DFU_GETSTATE_RESPONSE *resp = (DFU_GETSTATE_RESPONSE *)(USBD_EP0Buf);
    resp->bState = dfu_current_state;
    return (__TRUE);
}

BOOL USBD_DFU_ClrStatus(void) {
    DFU_SetStatus(DFU_STATUS_OK);
    return (__TRUE);
}

BOOL USBD_DFU_Abort(void) {
    switch (dfu_current_state) {
        case DFU_STATE_DFU_IDLE:
        case DFU_STATE_DFU_DNLOAD_SYNC:
        case DFU_STATE_DFU_DNLOAD_IDLE:
        case DFU_STATE_DFU_MANIFEST_SYNC:
        case DFU_STATE_DFU_UPLOAD_IDLE:
            USBD_DFU_SignalAbort(__FALSE);
            break;

        default:
            break;
    }

    return (__TRUE);
}

__weak BOOL USBD_DFU_StartUpgrade(void) {
    return (__TRUE);
}

__weak BOOL USBD_DFU_FinishUpgrade(void) {
    DFU_SetState(DFU_STATE_DFU_IDLE);
    return (__TRUE);
}

__weak BOOL USBD_DFU_WriteBlock(const U8 *buffer, U16 blockSize) {
    DFU_SetState(DFU_STATE_DFU_DNLOAD_IDLE);
    return (__TRUE);
}

__weak void usbd_dfu_init(void) {
    DFU_Reset();
}

__weak void USBD_DFU_SignalAbort(BOOL error) {

}
