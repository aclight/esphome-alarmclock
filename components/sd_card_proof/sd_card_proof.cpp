#include "sd_card_proof.h"

#include <cinttypes>
#include <dirent.h>

#include "driver/sdmmc_host.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include "esphome/core/hal.h"
#include "sdmmc_cmd.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

namespace esphome {
namespace sd_card_proof {

static const char *const TAG = "sd_card_proof";
static constexpr char kMountPoint[] = "/sdcard";
// No leading '/' after the drive letter: LV_FS_STDIO_PATH already ends in
// '/', and LVGL docs warn against doubling the separator.
static constexpr char kWallpaperPath[] = "S:wallpaper.jpg";

void SdCardProof::mount_() {
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.slot = SDMMC_HOST_SLOT_0;
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
    this->result_ = ProofResult::kMountFailed;
    this->mount_error_ = mount_result;
    this->log_result_();
    return;
  }

  this->size_mb_ =
      static_cast<uint64_t>(this->card_->csd.capacity) * this->card_->csd.sector_size /
      (1024U * 1024U);
  this->frequency_khz_ = this->card_->real_freq_khz;
  this->list_root_();
  this->show_wallpaper_();
  this->log_result_();
}

void SdCardProof::list_root_() const {
  DIR *dir = opendir(kMountPoint);
  if (dir == nullptr) {
    ESP_LOGW(TAG, "Could not list %s", kMountPoint);
    return;
  }
  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    ESP_LOGI(TAG, "SD card root entry: %s", entry->d_name);
  }
  closedir(dir);
}

void SdCardProof::show_wallpaper_() {
  lv_image_header_t header;
  if (lv_image_decoder_get_info(kWallpaperPath, &header) != LV_RESULT_OK) {
    this->result_ = ProofResult::kWallpaperFailed;
    return;
  }

  this->wallpaper_width_ = header.w;
  this->wallpaper_height_ = header.h;

  lv_obj_t *screen = lv_screen_active();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
  lv_obj_t *wallpaper = lv_image_create(screen);
  lv_image_set_src(wallpaper, kWallpaperPath);
  lv_obj_center(wallpaper);
  this->result_ = ProofResult::kWallpaperReady;
}

void SdCardProof::setup() {
  ESP_LOGI(TAG, "Waiting for ESP-Hosted Wi-Fi before mounting SD card");
}

bool SdCardProof::wifi_ready_() const {
#ifdef USE_WIFI
  if (this->wait_for_wifi_) {
    return wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_connected();
  }
#endif
  return true;
}

void SdCardProof::loop() {
  if (!this->mount_attempted_ && this->wifi_ready_()) {
    this->mount_attempted_ = true;
    this->mount_();
    this->last_log_ms_ = millis();
  }

  const uint32_t now = millis();
  if (this->result_ != ProofResult::kNotRun && now - this->last_log_ms_ >= 10000) {
    this->last_log_ms_ = now;
    this->log_result_();
  }
}

void SdCardProof::log_result_() const {
  if (this->result_ == ProofResult::kMountFailed) {
    ESP_LOGE(TAG, "SD mount failed: %s (0x%x)", esp_err_to_name(this->mount_error_),
             static_cast<unsigned>(this->mount_error_));
    return;
  }
  if (this->result_ == ProofResult::kWallpaperFailed) {
    ESP_LOGE(TAG, "SD mounted (%" PRIu64 " MB at %" PRIu32
                  " kHz), but LVGL could not decode %s",
             this->size_mb_, this->frequency_khz_, kWallpaperPath);
    return;
  }
  if (this->result_ == ProofResult::kWallpaperReady) {
    ESP_LOGI(TAG, "SD mounted (%" PRIu64 " MB at %" PRIu32
                  " kHz); displaying %" PRIu32 "x%" PRIu32 " JPEG from %s",
             this->size_mb_, this->frequency_khz_, this->wallpaper_width_,
             this->wallpaper_height_, kWallpaperPath);
  }
}

}  // namespace sd_card_proof
}  // namespace esphome
