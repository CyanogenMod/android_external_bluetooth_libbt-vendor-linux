/******************************************************************************
 *
 *  Copyright (C) 2014 Intel Corporation
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

#include <telemetry.h>
#include <utils/Log.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>

#define LOG_TAG "bt_tm"

#include "bt_tm.h"


static int bt_hci_id;

static const char *const sys_prop[] = {
	"bus",
	"class",
	"features",
	"hci_revision",
	"hci_version",
	"manufacturer",
	"name",
	"sniff_max_interval",
	"sniff_min_interval",
	"type",
	"uevent",
	"power/control",
	"power/runtime_status",
	"device/modalias",
	"device/power/runtime_status",
	"device/power/runtime_enabled"
};

/* Record Bluetooth properties from sysfs */
static void bt_tm_record_prop(struct tm_record *record) {
	char dev_path[] = "/sys/class/bluetooth/hciXX/";
	char prop_path[100];
	char prop_val[100];
	unsigned int i = 0;
	int n = 0;
	int fd = -1;

	snprintf(dev_path, sizeof(dev_path), "/sys/class/bluetooth/hci%d/", bt_hci_id);

	tm_record_appendf(record, "<device> : hci%d\n", bt_hci_id);

	tm_record_appendf(record, "<stack> : bluedroid\n");

	for (i = 0; i < sizeof(sys_prop) / sizeof(sys_prop[0]); i++) {
		strncpy(prop_path, dev_path, sizeof(prop_path));
		snprintf(prop_path, sizeof(prop_path), "%s%s", prop_path, sys_prop[i]);

		fd = open(prop_path, O_RDONLY);
		if (fd < 0) {
			tm_record_appendf(record, "<%s> no prop\n", sys_prop[i]);
			continue;
		}

		n = read(fd, prop_val, sizeof(prop_val));

		close(fd);

		if (n < 0) {
			tm_record_appendf(record, "<%s> read error\n", sys_prop[i]);
			continue;
		}

		if (n == 0) {
			tm_record_appendf(record, "<%s> is empty\n", sys_prop[i]);
			continue;
		}

		if (n == sizeof(prop_val))
			n -= 1;

		prop_val[n] = '\0';

		tm_record_appendf(record, "<%s> : %s", sys_prop[i], prop_val);
	}
}

static void bt_tm_report(const char *component, const char *error, int SEVERITY)
{
	struct tm_record *record = NULL;
	char class[100];

	snprintf(class, sizeof(class), "bluetooth/%s/%s", component, error);

	record = tm_record_create(SEVERITY, class);
	if (record == NULL) {
		ALOGE("bt_tm_report unable to create record");
		return;
	}

	/* Add hci sysfs info */
	bt_tm_record_prop(record);

	if (tm_record_send(record) < 0) {
		ALOGE("bt_tm_report send error");
	}
}

void bt_tm_set_intf(int hci_interface) {
	bt_hci_id = hci_interface;
}

void bt_tm_report_error(const char *component, const char *error)
{
	bt_tm_report(component, error, TM_ERROR);
}
