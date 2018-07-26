/*
 * Copyright (c) 2006-2018 RT-Thread Development Team. All rights reserved.
 * License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "iot_import.h"
#include "iot_export.h"

#include "exports/iot_export_ota.h"

#include <rtthread.h>

#if defined(MQTT_ID2_AUTH) && defined(TEST_ID2_DAILY)
    #define PRODUCT_KEY             "OvNmiEYRDSY"
    #define DEVICE_NAME             "sh_online_sample_mqtt"
    #define DEVICE_SECRET           "v9mqGzepKEphLhXmAoiaUIR2HZ7XwTky"
#elif defined(TEST_OTA_PRE)
    #define PRODUCT_KEY             "6RcIOUafDOm"
    #define DEVICE_NAME             "sh_pre_sample_mqtt"
    #define DEVICE_SECRET           "R0OTtD46DSalSpGW7SFzFDIA6fksTC2c"
#elif defined(TEST_MQTT_DAILY)
    #define PRODUCT_KEY             "fR9zCD4oT72"
    #define DEVICE_NAME             "ota_test"
    #define DEVICE_SECRET           "67szT5tQNMIu3sbrd3UwLhs7M73wTHXQ"
#else
#ifdef PKG_USING_ALI_IOTKIT_PRODUCT_KEY
    #define PRODUCT_KEY             PKG_USING_ALI_IOTKIT_PRODUCT_KEY
#else
    #define PRODUCT_KEY             "a1dSQSGZ77X"
#endif

#ifdef PKG_USING_ALI_IOTKIT_DEVICE_NAME
    #define DEVICE_NAME             PKG_USING_ALI_IOTKIT_DEVICE_NAME
#else
    #define DEVICE_NAME             "z1QKew7MvTmPllgQkDLt"
#endif

#ifdef PKG_USING_ALI_IOTKIT_DEVICE_SECRET
    #define DEVICE_SECRET           PKG_USING_ALI_IOTKIT_DEVICE_SECRET
#else
    #define DEVICE_SECRET           "cP7Ml9XYCNL3zMBwPYHXPCa5TPlmMMJt"
#endif
#endif

static char g_product_key[PRODUCT_KEY_LEN + 1];
static char g_device_name[DEVICE_NAME_LEN + 1];
static char g_device_secret[DEVICE_SECRET_LEN + 1];

/* These are pre-defined topics */
#define TOPIC_UPDATE            "/"PRODUCT_KEY"/"DEVICE_NAME"/update"
#define TOPIC_ERROR             "/"PRODUCT_KEY"/"DEVICE_NAME"/update/error"
#define TOPIC_GET               "/"PRODUCT_KEY"/"DEVICE_NAME"/get"
#define TOPIC_DATA              "/"PRODUCT_KEY"/"DEVICE_NAME"/data"

#define OTA_MQTT_MSGLEN         (2048)

#define EXAMPLE_TRACE(fmt, ...)  \
    do { \
        HAL_Printf("%s|%03d :: ", __func__, __LINE__); \
        HAL_Printf(fmt, ##__VA_ARGS__); \
        HAL_Printf("%s", "\r\n"); \
    } while(0)

static int      user_argc;
static uint8_t is_running = 0;

static void event_handle(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    uintptr_t packet_id = (uintptr_t)msg->msg;
    iotx_mqtt_topic_info_pt topic_info = (iotx_mqtt_topic_info_pt)msg->msg;

    switch (msg->event_type) {
        case IOTX_MQTT_EVENT_UNDEF:
            EXAMPLE_TRACE("undefined event occur.");
            break;

        case IOTX_MQTT_EVENT_DISCONNECT:
            EXAMPLE_TRACE("MQTT disconnect.");
            break;

        case IOTX_MQTT_EVENT_RECONNECT:
            EXAMPLE_TRACE("MQTT reconnect.");
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_SUCCESS:
            EXAMPLE_TRACE("subscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_TIMEOUT:
            EXAMPLE_TRACE("subscribe wait ack timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_NACK:
            EXAMPLE_TRACE("subscribe nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_SUCCESS:
            EXAMPLE_TRACE("unsubscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_TIMEOUT:
            EXAMPLE_TRACE("unsubscribe timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_NACK:
            EXAMPLE_TRACE("unsubscribe nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_SUCCESS:
            EXAMPLE_TRACE("publish success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_TIMEOUT:
            EXAMPLE_TRACE("publish timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_NACK:
            EXAMPLE_TRACE("publish nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_RECVEIVED:
            EXAMPLE_TRACE("topic message arrived but without any related handle: topic=%.*s, topic_msg=%.*s",
                          topic_info->topic_len,
                          topic_info->ptopic,
                          topic_info->payload_len,
                          topic_info->payload);
            break;

        default:
            EXAMPLE_TRACE("Should NOT arrive here.");
            break;
    }
}

static int mqtt_client(void)
{
#define OTA_BUF_LEN        (5000)

    int rc = 0;
    void *pclient = NULL, *h_ota = NULL;
    iotx_conn_info_pt pconn_info;
    iotx_mqtt_param_t mqtt_params;
    char *msg_buf = NULL, *msg_readbuf = NULL;
    char buf_ota[OTA_BUF_LEN];

    if (NULL == (msg_buf = (char *)HAL_Malloc(OTA_MQTT_MSGLEN))) {
        EXAMPLE_TRACE("not enough memory");
        rc = -1;
        goto do_exit;
    }

    if (NULL == (msg_readbuf = (char *)HAL_Malloc(OTA_MQTT_MSGLEN))) {
        EXAMPLE_TRACE("not enough memory");
        rc = -1;
        goto do_exit;
    }

    /**< get device info*/
    HAL_GetProductKey(g_product_key);
    HAL_GetDeviceName(g_device_name);
    HAL_GetDeviceSecret(g_device_secret);
    /**< end*/

    /* Device AUTH */
    if (0 != IOT_SetupConnInfo(g_product_key, g_device_name, g_device_secret, (void **)&pconn_info)) {
        EXAMPLE_TRACE("AUTH request failed!");
        rc = -1;
        goto do_exit;
    }

    /* Initialize MQTT parameter */
    memset(&mqtt_params, 0x0, sizeof(mqtt_params));

    mqtt_params.port = pconn_info->port;
    mqtt_params.host = pconn_info->host_name;
    mqtt_params.client_id = pconn_info->client_id;
    mqtt_params.username = pconn_info->username;
    mqtt_params.password = pconn_info->password;
    mqtt_params.pub_key = pconn_info->pub_key;

    mqtt_params.request_timeout_ms = 2000;
    mqtt_params.clean_session = 0;
    mqtt_params.keepalive_interval_ms = 60000;
    mqtt_params.pread_buf = msg_readbuf;
    mqtt_params.read_buf_size = OTA_MQTT_MSGLEN;
    mqtt_params.pwrite_buf = msg_buf;
    mqtt_params.write_buf_size = OTA_MQTT_MSGLEN;

    mqtt_params.handle_event.h_fp = event_handle;
    mqtt_params.handle_event.pcontext = NULL;

    /* Convert uppercase letters in host to lowercase */
	rt_kprintf("host: %s\r\n", strlwr((char*)mqtt_params.host));

    /* Construct a MQTT client with specify parameter */
    pclient = IOT_MQTT_Construct(&mqtt_params);
    if (NULL == pclient) {
        EXAMPLE_TRACE("MQTT construct failed");
        rc = -1;
        goto do_exit;
    }
    h_ota = IOT_OTA_Init(PRODUCT_KEY, DEVICE_NAME, pclient);
    if (NULL == h_ota) {
        rc = -1;
        EXAMPLE_TRACE("initialize OTA failed");
        goto do_exit;
    }

    if (0 != IOT_OTA_ReportVersion(h_ota, "iotx_ver_1.0.0")) {
        rc = -1;
        EXAMPLE_TRACE("report OTA version failed");
        goto do_exit;
    }


    HAL_SleepMs(1000);

    do {
        uint32_t firmware_valid;

        EXAMPLE_TRACE("wait ota upgrade command....");

        /* handle the MQTT packet received from TCP or SSL connection */
        IOT_MQTT_Yield(pclient, 200);

        if (IOT_OTA_IsFetching(h_ota)) {
            uint32_t last_percent = 0, percent = 0;
            char version[128], md5sum[33];
            uint32_t len, size_downloaded, size_file;
            do {

                len = IOT_OTA_FetchYield(h_ota, buf_ota, OTA_BUF_LEN, 1);
                if (len > 0) {
                    EXAMPLE_TRACE("Here write OTA data to file....");
                } else {
                    IOT_OTA_ReportProgress(h_ota, IOT_OTAP_FETCH_FAILED, NULL);
                    EXAMPLE_TRACE("ota fetch fail");
                    HAL_SleepMs(2000);
                }

                /* get OTA information */
                IOT_OTA_Ioctl(h_ota, IOT_OTAG_FETCHED_SIZE, &size_downloaded, 4);
                IOT_OTA_Ioctl(h_ota, IOT_OTAG_FILE_SIZE, &size_file, 4);
                IOT_OTA_Ioctl(h_ota, IOT_OTAG_MD5SUM, md5sum, 33);
                IOT_OTA_Ioctl(h_ota, IOT_OTAG_VERSION, version, 128);

                last_percent = percent;
                percent = (size_downloaded * 100) / size_file;
                if (percent - last_percent > 0) {
                    IOT_OTA_ReportProgress(h_ota, percent, NULL);
                    IOT_OTA_ReportProgress(h_ota, percent, "hello");
                }
                IOT_MQTT_Yield(pclient, 100);
            } while (!IOT_OTA_IsFetchFinish(h_ota));

            IOT_OTA_Ioctl(h_ota, IOT_OTAG_CHECK_FIRMWARE, &firmware_valid, 4);
            if (0 == firmware_valid) {
                EXAMPLE_TRACE("The firmware is invalid! Download firmware failed.");
                goto do_exit;
            } else {
                EXAMPLE_TRACE("The firmware is valid!  Download firmware successfully.");

                /* For test, here report new firmware version. 
                 * In actual application, please report the new version number after the firmware is successfully updated. 
                 */
                if (0 != IOT_OTA_ReportVersion(h_ota, version)) {
                    rc = -1;
                    EXAMPLE_TRACE("report OTA version failed");
                    goto do_exit;
                }

                EXAMPLE_TRACE("OTA FW version: %s", version);

                /* handle the MQTT packet received from TCP or SSL connection */
                IOT_MQTT_Yield(pclient, 200);

                goto do_exit;
            }
        }
        HAL_SleepMs(2000);
    } while (is_running);

    HAL_SleepMs(200);

do_exit:

    if (NULL != h_ota) {
        IOT_OTA_Deinit(h_ota);
    }

    if (NULL != pclient) {
        IOT_MQTT_Destroy(&pclient);
    }

    if (NULL != msg_buf) {
        HAL_Free(msg_buf);
    }

    if (NULL != msg_readbuf) {
        HAL_Free(msg_readbuf);
    }

    /**< end*/
    IOT_DumpMemoryStats(IOT_LOG_DEBUG);
    IOT_CloseLog();

    is_running = 0;

    EXAMPLE_TRACE("out of sample!");

    return rc;
}

static int ali_ota_main(int argc, char **argv)
{
    rt_thread_t tid;
    IOT_OpenLog("mqtt");
    IOT_SetLogLevel(IOT_LOG_DEBUG);

    user_argc = argc;
    if (2 == user_argc)
    {
        if (!strcmp("start", argv[1]))
        {
            if (1 == is_running)
            {
                HAL_Printf("OTA test is already running! Please stop running first by using the \"ali_ota_test stop\" command\n");
                return 0;
            }
            is_running = 1;
        }
        else if (!strcmp("stop", argv[1]))
        {
            if (0 == is_running)
            {
                HAL_Printf("OTA test is already stopped!\n");
                return 0;
            }
            is_running = 0;
            // stop ota test
            return 0;
        }
        else
        {
            HAL_Printf("Input param error! Example: ali_ota_test start/stop\n");
            return 0;
        }
    }
    else
    {
        HAL_Printf("Input param error! Example: ali_ota_test start/stop\n");
        return 0;
    }

#ifdef IOTX_PRJ_VERSION
    EXAMPLE_TRACE("iotkit-embedded sdk version: %s", IOTX_PRJ_VERSION);
#endif

    /**< set device info*/
    HAL_SetProductKey(PRODUCT_KEY);
    HAL_SetDeviceName(DEVICE_NAME);
    HAL_SetDeviceSecret(DEVICE_SECRET);

    tid = rt_thread_create("ali-ota",
        (void*)mqtt_client, NULL,
        12 * 1024, RT_THREAD_PRIORITY_MAX / 2 - 1, 10);

    if (tid != RT_NULL)
        rt_thread_startup(tid);

    return 0;
}
#ifdef RT_USING_FINSH
#include <finsh.h>

MSH_CMD_EXPORT_ALIAS(ali_ota_main, ali_ota_test, Example: ali_ota_test);
#endif
