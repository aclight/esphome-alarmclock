#include "sd_card_proof.h"

#include <cinttypes>
#include <cstdio>

#include "driver/sdmmc_host.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/hal.h"
#include "sdmmc_cmd.h"

namespace esphome {
namespace sd_card_proof {

static const char *const TAG = "sd_card_proof";
static constexpr char kMountPoint[] = "/sdcard";
static constexpr char kProofFile[] = "/sdcard/sd-proof.txt";

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

  FILE *file = fopen(kProofFile, "r");
  if (file == nullptr) {
    this->result_ = ProofResult::kOpenFailed;
    this->log_result_();
    return;
  }

  this->bytes_read_ = fread(this->contents_, 1, sizeof(this->contents_) - 1, file);
  fclose(file);
  this->result_ = ProofResult::kReadSucceeded;
  this->log_result_();
}

void SdCardProof::setup() {
  ESP_LOGI(TAG, "Waiting for ESP-Hosted Wi-Fi before mounting SD card");
}

void SdCardProof::loop() {
  if (!this->mount_attempted_ && wifi::global_wifi_component != nullptr &&
      wifi::global_wifi_component->is_connected()) {
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
  if (this->result_ == ProofResult::kOpenFailed) {
    ESP_LOGE(TAG, "SD mounted (%" PRIu64 " MB at %" PRIu32
                  " kHz), but could not open %s",
             this->size_mb_, this->frequency_khz_, kProofFile);
    return;
  }
  if (this->result_ == ProofResult::kReadSucceeded) {
    ESP_LOGI(TAG, "SD mounted (%" PRIu64 " MB at %" PRIu32
                  " kHz); read %u bytes from %s: %s",
             this->size_mb_, this->frequency_khz_,
             static_cast<unsigned>(this->bytes_read_), kProofFile, this->contents_);
  }
}

}  // namespace sd_card_proof
}  // namespace esphome
