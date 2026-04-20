#include "bsp_sdcard.h"
#include <string.h>
#include <sys/stat.h>

static sdmmc_card_t *sd_card = NULL;
static bool mounted = false;

esp_err_t sdcard_init(void)
{
    esp_err_t err = ESP_OK;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = 10000;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = SD_GPIO_CLK;
    slot_config.cmd = SD_GPIO_CMD;
    slot_config.d0  = SD_GPIO_D0;
    slot_config.width = 1;  /* 1-line SDIO */
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    SD_INFO("Mounting SD card filesystem...");
    err = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &sd_card);
    if (err != ESP_OK) {
        if (err == ESP_FAIL) {
            SD_ERROR("Failed to mount filesystem");
        } else {
            SD_ERROR("Failed to initialize SD card (%s)", esp_err_to_name(err));
        }
        mounted = false;
        return err;
    }

    mounted = true;
    sdmmc_card_print_info(stdout, sd_card);
    SD_INFO("SD card mounted at %s", SD_MOUNT_POINT);
    return ESP_OK;
}

bool sdcard_is_mounted(void)
{
    return mounted;
}

esp_err_t sdcard_append_line(const char *filepath, const char *line)
{
    if (filepath == NULL || line == NULL)
        return ESP_ERR_INVALID_ARG;
    if (!mounted)
        return ESP_ERR_INVALID_STATE;

    FILE *f = fopen(filepath, "a");
    if (f == NULL) {
        SD_ERROR("Failed to open %s for appending", filepath);
        return ESP_FAIL;
    }
    fprintf(f, "%s\n", line);
    fclose(f);
    return ESP_OK;
}

esp_err_t sdcard_ensure_file_with_header(const char *filepath, const char *header)
{
    if (filepath == NULL || header == NULL)
        return ESP_ERR_INVALID_ARG;
    if (!mounted)
        return ESP_ERR_INVALID_STATE;

    struct stat st;
    if (stat(filepath, &st) == 0) {
        /* File already exists */
        return ESP_OK;
    }

    /* Create new file with header */
    FILE *f = fopen(filepath, "w");
    if (f == NULL) {
        SD_ERROR("Failed to create %s", filepath);
        return ESP_FAIL;
    }
    fprintf(f, "%s\n", header);
    fclose(f);
    SD_INFO("Created %s with header", filepath);
    return ESP_OK;
}

esp_err_t sdcard_read_lines(const char *filepath,
                            void (*line_cb)(const char *line, void *user_data),
                            void *user_data)
{
    if (filepath == NULL || line_cb == NULL)
        return ESP_ERR_INVALID_ARG;
    if (!mounted)
        return ESP_ERR_INVALID_STATE;

    FILE *f = fopen(filepath, "r");
    if (f == NULL) {
        SD_ERROR("Failed to open %s for reading", filepath);
        return ESP_FAIL;
    }

    char line_buf[512];
    while (fgets(line_buf, sizeof(line_buf), f) != NULL) {
        /* Strip trailing newline */
        size_t len = strlen(line_buf);
        while (len > 0 && (line_buf[len - 1] == '\n' || line_buf[len - 1] == '\r')) {
            line_buf[--len] = '\0';
        }
        line_cb(line_buf, user_data);
    }

    fclose(f);
    return ESP_OK;
}

bool sdcard_file_exists(const char *filepath)
{
    if (filepath == NULL || !mounted)
        return false;
    struct stat st;
    return (stat(filepath, &st) == 0);
}

esp_err_t sdcard_get_space(uint64_t *total_bytes, uint64_t *free_bytes)
{
    if (!mounted)
        return ESP_ERR_INVALID_STATE;

    FATFS *fs;
    DWORD fre_clust;
    FRESULT res = f_getfree("0:", &fre_clust, &fs);
    if (res != FR_OK) {
        SD_ERROR("Failed to get free space (%d)", res);
        return ESP_FAIL;
    }

    uint64_t total = (uint64_t)(fs->n_fatent - 2) * fs->csize * 512;
    uint64_t free_space = (uint64_t)fre_clust * fs->csize * 512;

    if (total_bytes != NULL)
        *total_bytes = total;
    if (free_bytes != NULL)
        *free_bytes = free_space;

    return ESP_OK;
}
