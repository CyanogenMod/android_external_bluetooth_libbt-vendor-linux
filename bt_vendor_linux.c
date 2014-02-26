/******************************************************************************
 *
 *  Copyright (C) 2013 Intel Corporation
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

#define LOG_TAG "bt_vendor"

#include <errno.h>

#include "bt_vendor_lib.h"
#include "bt_tm.h"
#include <utils/Log.h>
#include <sys/socket.h>
#include <cutils/properties.h>

#define BTPROTO_HCI	1

struct sockaddr_hci {
	sa_family_t	hci_family;
	unsigned short	hci_dev;
	unsigned short  hci_channel;
};

#define HCI_CHANNEL_USER	1

static const bt_vendor_callbacks_t *bt_vendor_callbacks = NULL;
static unsigned char bt_vendor_local_bdaddr[6] = { 0x00, };
static int bt_vendor_fd = -1;
static int hci_interface = 0;

static int bt_vendor_init(const bt_vendor_callbacks_t *p_cb, unsigned char *local_bdaddr)
{
	char prop_value[PROPERTY_VALUE_MAX];

	ALOGI("%s", __func__);

	bt_tm_set_intf(hci_interface);

	if (p_cb == NULL) {
		ALOGE("init failed with no user callbacks!");
		BTTM_REPORT("init_no_callback");
		return -1;
	}

	bt_vendor_callbacks = p_cb;

	memcpy(bt_vendor_local_bdaddr, local_bdaddr, sizeof(bt_vendor_local_bdaddr));

	property_get("bluetooth.interface", prop_value, "0");

	errno = 0;
	if (memcmp(prop_value, "hci", 3))
		hci_interface = strtol(prop_value, (char **)NULL, 10);
	else
		hci_interface = strtol(prop_value + 3, (char **)NULL, 10);
	if (errno)
		hci_interface = 0;

	ALOGI("Using interface hci%d", hci_interface);

	return 0;
}

static int bt_vendor_open(void *param)
{
	int (*fd_array)[] = (int (*) []) param;
	struct sockaddr_hci addr;
	int fd;

	ALOGI("%s", __func__);

	fd = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
	if (fd < 0) {
		ALOGE("socket create error %s", strerror(errno));
		BTTM_REPORT("socket_create");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.hci_family = AF_BLUETOOTH;
	addr.hci_dev = hci_interface;
	addr.hci_channel = HCI_CHANNEL_USER;

	if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		ALOGE("socket bind error %s", strerror(errno));
		BTTM_REPORT("socket_bind");
		close(fd);
		return -1;
	}

	(*fd_array)[CH_CMD] = fd;
	(*fd_array)[CH_EVT] = fd;
	(*fd_array)[CH_ACL_OUT] = fd;
	(*fd_array)[CH_ACL_IN] = fd;

	bt_vendor_fd = fd;

	ALOGI("%s returning %d", __func__, bt_vendor_fd);

	return 1;
}

static int bt_vendor_close(void *param)
{
	close(bt_vendor_fd);
	bt_vendor_fd = -1;

	return 0;
}

static int bt_vendor_op(bt_vendor_opcode_t opcode, void *param)
{
	int retval = 0;

	ALOGI("%s op %d", __func__, opcode);

	switch (opcode) {
	case BT_VND_OP_POWER_CTRL:
		break;

	case BT_VND_OP_FW_CFG:
		bt_vendor_callbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);
		break;

	case BT_VND_OP_SCO_CFG:
		bt_vendor_callbacks->scocfg_cb(BT_VND_OP_RESULT_SUCCESS);
		break;

	case BT_VND_OP_USERIAL_OPEN:
		retval = bt_vendor_open(param);
		break;

	case BT_VND_OP_USERIAL_CLOSE:
		retval = bt_vendor_close(param);
		break;

        case BT_VND_OP_GET_LPM_IDLE_TIMEOUT:
		*((uint32_t *)param) = 3000;
		retval = 0;
		break;

	case BT_VND_OP_LPM_SET_MODE:
		bt_vendor_callbacks->lpm_cb(BT_VND_OP_RESULT_SUCCESS);
		break;

	case BT_VND_OP_LPM_WAKE_SET_STATE:
		break;

	case BT_VND_OP_EPILOG:
		bt_vendor_callbacks->epilog_cb(BT_VND_OP_RESULT_SUCCESS);
		break;
	}

	ALOGI("%s op %d retval %d", __func__, opcode, retval);

	return retval;
}

static void bt_vendor_cleanup( void )
{
	ALOGI("%s", __func__);

	bt_vendor_callbacks = NULL;
}

const bt_vendor_interface_t BLUETOOTH_VENDOR_LIB_INTERFACE = {
	sizeof(bt_vendor_interface_t),
	bt_vendor_init,
	bt_vendor_op,
	bt_vendor_cleanup,
};
