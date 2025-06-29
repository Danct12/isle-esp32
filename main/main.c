/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include <pthread.h>
#include "sdkconfig.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_pthread.h"
#include "esp_system.h"

#include "isleapp.h"

// Just in case if someone accidentally enabled this and wiped everything by accident.
#ifdef CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL
#error "CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL is enabled. Please manually format your SD card as FAT32."
#endif

static const char *TAG = "main";

void app_main(void)
{
    esp_err_t eret;
    pthread_t lego_pthread;
    pthread_attr_t attr;
    int ret;

    ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    eret = bsp_sdcard_mount();
    if (eret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(eret));
        return;
    }

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8192);  // Set the stack size for the thread

    ret = pthread_create(&lego_pthread, &attr, isle_init, NULL);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to create lego thread: %d\n", ret);
        goto done;
    }

    ret = pthread_join(lego_pthread, NULL);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to join lego thread: %d\n", ret);
        goto done;
    }

done:
    /*
     * After the game has finished (or failed), we'll unmount everything and
     * do nothing..
     */
    eret = bsp_sdcard_unmount();
    if (eret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount SD card %s", esp_err_to_name(eret));
    } else {
        ESP_LOGI(TAG, "It is now safe to turn off your MCU.");
    }
}
