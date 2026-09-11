"""SD-card wallpaper display proof for the ESP32-P4 board."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32
from esphome.components.lvgl import defines as lv_defines
from esphome.const import CONF_ID

DEPENDENCIES = ["wifi", "lvgl"]

CONF_CLK_PIN = "clk_pin"
CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"

sd_card_proof_ns = cg.esphome_ns.namespace("sd_card_proof")
SdCardProof = sd_card_proof_ns.class_("SdCardProof", cg.Component)


def _add_lvgl_defines(config):
    # Runs during schema validation, which always precedes every component's
    # to_code(), so these land in lv_conf.h regardless of to_code ordering.
    lv_defines.add_define("LV_USE_IMAGE", "1")
    # lv_image.h hard-requires the label widget even though our page has none.
    lv_defines.add_define("LV_USE_LABEL", "1")
    lv_defines.add_define("LV_USE_TJPGD", "1")
    lv_defines.add_define("LV_USE_FS_STDIO", "1")
    lv_defines.add_define("LV_FS_STDIO_LETTER", "'S'")
    lv_defines.add_define("LV_FS_STDIO_PATH", '"/sdcard/"')
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SdCardProof),
            cv.Required(CONF_CLK_PIN): cv.int_range(min=0),
            cv.Required(CONF_CMD_PIN): cv.int_range(min=0),
            cv.Required(CONF_DATA0_PIN): cv.int_range(min=0),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _add_lvgl_defines,
)


async def to_code(config):
    esp32.include_builtin_idf_component("fatfs")
    # Both default to disabled; without these, FATFS falls back to 8.3-only
    # short filenames and opendir()/readdir() are compiled out entirely.
    esp32.require_fatfs()
    esp32.require_vfs_dir()

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_clk_pin(config[CONF_CLK_PIN]))
    cg.add(var.set_cmd_pin(config[CONF_CMD_PIN]))
    cg.add(var.set_data0_pin(config[CONF_DATA0_PIN]))
