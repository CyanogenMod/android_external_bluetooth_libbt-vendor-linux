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

#ifndef __BT_TM_H__
#define __BT_TM_H__

void bt_tm_report_error(const char *component, const char *error);
void bt_tm_set_intf(int hci_interface);

#ifndef LOG_TAG
#define LOG_TAG "default"
#endif

#define BTTM_REPORT(error) \
	bt_tm_report_error(LOG_TAG, error);

#endif
