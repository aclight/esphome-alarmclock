#include "sd_card_proof.h"

#include <cinttypes>
#include <cstdio>

#include "driver/sdmmc_host.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

namespace esphome {
namespace sd_card_proof {

static const char *const TAG = "sd_card_proof";
static constexpr char kMountPoint[] = "/sdcard";
static constexpr char kProofFile[] = "/sdcard/sd-proof.txt";

void SdCardProof::setup() {
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = 10000;

  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.clk = static_cast<gpio_num_t>(this->clk_pin_);
  slot_config.cmd = static_cast<gpio_num_t>(this->cmd_pin_);
  slot_config.d0 = static_cast<gpio_num_t>(this->data0_pin_);
  slot_config.d1 = GPIO_NUM_NC;
  slot_config.d2 = GPIO_NUM_NC;
  slot_config.d3 = GPIO_NUM_NC;
  slot_config.width = 1;
  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 2,
      .allocation_unit_size = 16 * 1024,
  };

  const esp_err_t mount_result = esp_vfs_fat_sdmmc_mount(
      kMountPoint, &host, &slot_config, &mount_config, &this->card_);
  if (mount_result != ESP_OK) {
    ESP_LOGE(TAG, "SD mount failed: %s (0x%x)", esp_err_to_name(mount_result),
             static_cast<unsigned>(mount_result));
    this->mark_failed();
    return;
  }

  const uint64_t size_mb =
      static_cast<uint64_t>(this->card_->csd.capacity) * this->card_->csd.sector_size /
      (1024U * 1024U);
  ESP_LOGI(TAG, "SD mounted: %" PRIu64 " MB at %" PRIu32 " kHz", size_mb,
           this->card_->real_freq_khz);

  FILE *file = fopen(kProofFile, "r");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Mounted, but could not open %s", kProofFile);
    this->mark_failed();
    return;
  }

  char contents[129] = {};
  const size_t bytes_read = fread(contents, 1, sizeof(contents) - 1, file);
  fclose(file);
  ESP_LOGI(TAG, "Read %u bytes from %s: %s", static_cast<unsigned>(bytes_read),
           kProofFile, contents);
}

}  // namespace sd_card_proof
}  // namespace esphome
