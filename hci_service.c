/******************************************************************************
 *
 *  Copyright (C) Intel 2014
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/******************************************************************************
 *
 *  Filename:   hci_service.c
 *
 *  Description:    HCI service gateway to offer native bindable service
 *      that can be accessed through Binder
 *
 ******************************************************************************/

#define LOG_TAG "bt_bind_service"

#include <pthread.h>
#include <utils/Log.h>

#include "libbtcellcoex-client.h"
#include "bt_hci_bdroid.h"
#include "bt_vendor_lib.h"
#include "hardware/bluetooth.h"

/******************************************************************************
**  Constants & Macros
******************************************************************************/

#ifndef BTHCISERVICE_DBG
#define BTHCISERVICE_DBG FALSE
#endif

#ifndef BTHCISERVICE_VERB
#define BTHCISERVICE_VERB FALSE
#endif

#if (BTHCISERVICE_DBG == TRUE)
#define BTHSDBG(param, ...) {ALOGD(param, ## __VA_ARGS__);}
#else
#define BTHSDBG(param, ...) {}
#endif

#if (BTHCISERVICE_VERB == TRUE)
#define BTHSVERB(param, ...) {ALOGD(param, ## __VA_ARGS__);}
#else
#define BTHSVERB(param, ...) {}
#endif

#define BTHSERR(param, ...) {ALOGE(param, ## __VA_ARGS__);}
#define BTHSWARN(param, ...) {ALOGW(param, ## __VA_ARGS__);}

/* Duplicated definitions from ./hardware.h */
#define HCI_CMD_PREAMBLE_SIZE                   3
#define HCI_EVT_CMD_CMPL_STATUS_RET_BYTE        5
#define HCI_EVT_CMD_CMPL_OPCODE                 3

#define STREAM_TO_UINT8(u8, p) {u8 = (uint8_t)(*(p)); (p) += 1;}
#define STREAM_TO_UINT16(u16, p) {u16 = ((uint16_t)(*(p)) + (((uint16_t)(*((p) + 1))) << 8)); (p) += 2;}

// Time to wait the HCI command complete event after that HCI command is sent.
// CAUTION: Must be < 1000. On stress tests, 60ms has been measured between
// the hci_cmd_send and the hci_cmd_cback calls.
#define WAIT_TIME_MS       500

/******************************************************************************
**  Extern variables and functions
******************************************************************************/
extern const bt_vendor_callbacks_t *bt_vendor_cbacks;

/******************************************************************************
**  Static Variables
******************************************************************************/
static uint8_t status = -1;
static pthread_cond_t thread_cond;
static pthread_mutex_t mutex;
static bool predicate = false;
static volatile bool hci_service_stopped = false;
static pthread_t init_thread;

/******************************************************************************
**  Functions
******************************************************************************/
static int hci_cmd_send(const size_t cmdLen, const void* cmdBuf);
void hci_bind_client_cleanup(void);
static void print_xmit(HC_BT_HDR *p_msg);
static void *retry_init_thread(void* param);

/*******************************************************************************
**
** Function         hci_bind_client_init
**
** Description     Initialization of the client to be able to send HCI commands
** from the bound interface.
**
** Returns          None
**
*******************************************************************************/
void hci_bind_client_init(void)
{
    int ret = -1;
    int bind_state = BTCELLCOEX_STATUS_NO_INIT;
    pthread_attr_t thread_attr;

    BTHSVERB("%s enter", __FUNCTION__);

    if ((ret = pthread_cond_init(&thread_cond, NULL)) != 0) {
        BTHSERR("%s: pthread_cond_init failed: %s", __FUNCTION__, strerror(ret));
        hci_service_stopped = true;
        return;
    }
    if ((ret = pthread_mutex_init(&mutex, NULL)) != 0) {
        BTHSERR("%s: pthread_mutex_init failed: %s", __FUNCTION__, strerror(ret));
        hci_service_stopped = true;
        if ((ret = pthread_cond_destroy(&thread_cond)) != 0)
            BTHSWARN("%s: pthread_cond_destroy failed: %s", __FUNCTION__, strerror(ret));
        return;
    }

    hci_service_stopped = false;

    bind_state = bindToCoexService(&hci_cmd_send);
    if(bind_state != BTCELLCOEX_STATUS_OK) {
        BTHSDBG("%s: bindToCoexService failure, planning to retry later", __FUNCTION__);

        BTHSDBG("%s: Create a thread on service", __FUNCTION__);
        if ((ret = pthread_attr_init(&thread_attr)) != 0) {
            BTHSERR("%s: pthread_attr_init failed: %s", __FUNCTION__, strerror(ret));
            return;
        }
        if ((ret = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED)) != 0) {
            BTHSERR("%s: pthread_attr_setdetachstate failed: %s", __FUNCTION__, strerror(ret));
            pthread_attr_destroy(&thread_attr);
            return;
        }
        if ((ret = pthread_create(&init_thread, &thread_attr, retry_init_thread, NULL)) != 0) {
            BTHSERR("%s: pthread_create failed: %s", __FUNCTION__, strerror(ret));
            pthread_attr_destroy(&thread_attr);
            return;
        }
        if ((ret = pthread_attr_destroy(&thread_attr)) != 0)
            BTHSWARN("%s: pthread_attr_destroy failed: %s", __FUNCTION__, strerror(ret));
    }

    BTHSVERB("%s exit", __FUNCTION__);
}

/*******************************************************************************
**
** Function         retry_init_thread
**
** Description     Thread handling the binding retry in case the modem is not
** ready and so the BT handler and its binder doesn't exist.
** param not necessary but compiler friendly.
**
** Returns          None
**
*******************************************************************************/
static void *retry_init_thread(void* param)
{
    int seconds = 1;
    int bind_retry = BTCELLCOEX_STATUS_NO_INIT;

    BTHSDBG("%s", __FUNCTION__);

    for(;;) {
        if (hci_service_stopped) {
            BTHSDBG("%s: hci_service_stopped, retry_init_thread exit", __FUNCTION__);
            pthread_exit(NULL);
        }
        BTHSVERB("%s: Wait before retrying to bind", __FUNCTION__);
        sleep(seconds);
        if (seconds < 10)
            seconds++;
        bind_retry = bindToCoexService(&hci_cmd_send);
        if(bind_retry != BTCELLCOEX_STATUS_OK) {
            BTHSDBG("%s: bindToCoexService failure, retry in %d seconds", __FUNCTION__, seconds);
        } else {
            BTHSDBG("%s: bindToCoexService success", __FUNCTION__);
            break;
        }
    }

    return NULL; // Not necessary but compiler friendly
}

/*******************************************************************************
**
** Function         hci_bind_client_cleanup
**
** Description     Function called to stop the thread, forbids the service use
**                 and clean service ressources
**
** Returns          None
**
*******************************************************************************/
void hci_bind_client_cleanup(void)
{
    int ret = -1;
    BTHSDBG("%s", __FUNCTION__);

    hci_service_stopped = true;

    if ((ret = pthread_mutex_destroy(&mutex)) != 0)
        BTHSWARN("%s: pthread_mutex_destroy failed: %s", __FUNCTION__, strerror(ret));
    if ((ret = pthread_cond_destroy(&thread_cond)) != 0)
        BTHSWARN("%s: pthread_cond_destroy failed: %s", __FUNCTION__, strerror(ret));

    BTHSDBG("%s done.", __FUNCTION__);
}

/*******************************************************************************
**
** Function         hci_cmd_cback
**
** Description     Callback invoked on completion of the HCI command
**
** Returns          None
**
*******************************************************************************/
static void hci_cmd_cback(void *p_mem)
{
    int ret = -1;
    HC_BT_HDR *p_evt_buf = (HC_BT_HDR *) p_mem;
    uint8_t *p;
    uint16_t opcode;

    // Get the HCI command complete event status
    status = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE);
    p = (uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPCODE;
    STREAM_TO_UINT16(opcode, p);

    if (status == 0) {
        BTHSDBG("%s: HCI with opcode: 0x%04X success", __FUNCTION__, opcode);
    } else {
        BTHSERR("%s: HCI with opcode: 0x%04X failure", __FUNCTION__, opcode);
    }

    // We need to deallocate the received buffer
    if (bt_vendor_cbacks)
        bt_vendor_cbacks->dealloc(p_evt_buf);

    if ((ret = pthread_mutex_lock(&mutex)) != 0) {
        BTHSERR("%s: pthread_mutex_lock failed: %s", __FUNCTION__, strerror(ret));
        return;
    }
    predicate = true;
    // Wakeup the socket server thread so it can send the status to the sender
    if ((ret = pthread_cond_signal(&thread_cond)) != 0)
        BTHSERR("%s: pthread_cond_signal failed: %s", __FUNCTION__, strerror(ret));
    if ((ret = pthread_mutex_unlock(&mutex)) != 0)
        BTHSERR("%s: pthread_mutex_unlock failed: %s", __FUNCTION__, strerror(ret));
}

/*******************************************************************************
**
** Function         hci_cmd_send
**
** Returns          BTCELLCOEX_STATUS_OK on success
**                  BTCELLCOEX_STATUS_INVALID_OPERATION if the service if not ready
**                  BTCELLCOEX_STATUS_BAD_VALUE on invalid parameters
**                  BTCELLCOEX_STATUS_UNKNOWN_ERROR on internal software issues
**                  BTCELLCOEX_STATUS_CMD_FAILED on HCI transmission failures
**
*******************************************************************************/
int hci_cmd_send(const size_t cmdLen, const void* cmdBuf)
{
    uint8_t *p;
    struct timespec ts;
    struct timeval currentTime;
    int ret = 0;
    int retVal = BTCELLCOEX_STATUS_OK;
    HC_BT_HDR *p_msg = NULL;
    uint8_t *pcmdBuf = (uint8_t *)cmdBuf;

    BTHSDBG("%s", __FUNCTION__);

    if(hci_service_stopped) {
        BTHSWARN("%s: HCI service is stopped!", __FUNCTION__);
        return BTCELLCOEX_STATUS_INVALID_OPERATION;
    }

    if(NULL == cmdBuf) {
        BTHSERR("%s: null cmd pointer passed!", __FUNCTION__);
        return BTCELLCOEX_STATUS_BAD_VALUE;
    }

    uint16_t opcode = (uint16_t)(*(pcmdBuf)) + (((uint16_t)(*((pcmdBuf) + 1))) << 8);
    uint8_t length = (uint8_t)(*((pcmdBuf) + 2)) + HCI_CMD_PREAMBLE_SIZE;

    if (length != (uint8_t) cmdLen) {
        BTHSERR("%s: wrong cmd length parameter!", __FUNCTION__);
        return BTCELLCOEX_STATUS_BAD_VALUE;
    }
    if (!bt_vendor_cbacks) {
        BTHSERR("%s: bt_vendor_cbacks not initialized.", __FUNCTION__);
        return BTCELLCOEX_STATUS_UNKNOWN_ERROR;
    }
    // Transmitted buffers are automatically deallocated
    if ((p_msg = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + length)) == NULL) {
        BTHSERR("%s: failed to allocate buffer.", __FUNCTION__);
        return BTCELLCOEX_STATUS_UNKNOWN_ERROR;
    }

    p_msg->event = MSG_STACK_TO_HC_HCI_CMD;
    p_msg->len = length;
    p_msg->offset = 0;

    p = (uint8_t *)(p_msg + 1);
    memcpy(p, cmdBuf, length);

    // Only if BTHCISERVICE_VERB == TRUE
    print_xmit(p_msg);

    // HCI send / cback mechanism is made to send 1 cmd at a time.
    if ((ret = pthread_mutex_lock(&mutex)) != 0) {
        BTHSERR("%s: pthread_mutex_lock failed: %s", __FUNCTION__, strerror(ret));
        bt_vendor_cbacks->dealloc(p_msg);
        return BTCELLCOEX_STATUS_UNKNOWN_ERROR;
    }

    // Send the HCI command
    if (bt_vendor_cbacks->xmit_cb(opcode, p_msg, hci_cmd_cback) == FALSE) {
        BTHSERR("%s: failed to xmit buffer.", __FUNCTION__);
        bt_vendor_cbacks->dealloc(p_msg);
        retVal = BTCELLCOEX_STATUS_UNKNOWN_ERROR;
        goto exit_unlock;
    }

    gettimeofday(&currentTime, NULL);
    ts.tv_nsec = (currentTime.tv_usec * 1000) + (WAIT_TIME_MS % 1000) * 1000000;
    ts.tv_sec = currentTime.tv_sec + (WAIT_TIME_MS / 1000) + (ts.tv_nsec / 1000000000);
    ts.tv_nsec %= 1000000000;

    ret = 0;
    while (!predicate && ret == 0)
        ret = pthread_cond_timedwait(&thread_cond, &mutex, &ts);
    if (ret == 0) {
        BTHSVERB("%s: pthread_cond_timedwait succeed", __FUNCTION__);
        predicate = false;
    } else {
        BTHSERR("%s: pthread_cond_timedwait failed: %s", __FUNCTION__, strerror(ret));
        retVal = BTCELLCOEX_STATUS_UNKNOWN_ERROR;
        goto exit_unlock;
    }

    if (status == 0) {
        BTHSVERB("%s: HCI command succeed", __FUNCTION__);
        retVal = BTCELLCOEX_STATUS_OK;
    } else {
        BTHSERR("%s: HCI command failed", __FUNCTION__);
        retVal = BTCELLCOEX_STATUS_CMD_FAILED;
    }

exit_unlock:
    if ((ret = pthread_mutex_unlock(&mutex)) != 0) {
        BTHSERR("%s: pthread_mutex_unlock failed: %s", __FUNCTION__, strerror(ret));
        retVal = BTCELLCOEX_STATUS_UNKNOWN_ERROR;
    }
    return retVal;
}


#if (BTHCISERVICE_VERB == TRUE)
/*******************************************************************************
**
** Function         print_xmit
**
** Description      Debug function to print the HCI command length, opcode and parameters
**
** Returns          none
**
*******************************************************************************/
static void print_xmit(HC_BT_HDR *p_msg) {
    uint16_t opcode;
    uint8_t length, i;
    uint8_t *p = (uint8_t *)(p_msg + 1);

    STREAM_TO_UINT16(opcode, p);
    STREAM_TO_UINT8(length, p);
    BTHSVERB("%s: Send a %d bytes long packet. opcode = 0x%04X", __FUNCTION__, length, opcode);
    for(i = 0; i < length; i++) BTHSVERB("0x%02X", *p++);
}
#else
static void print_xmit(HC_BT_HDR *p_msg) {}
#endif
