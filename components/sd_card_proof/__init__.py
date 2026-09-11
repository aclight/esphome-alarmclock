"""Standalone SDMMC mount-and-read proof for the ESP32-P4 board."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32
from esphome.const import CONF_ID

CONF_CLK_PIN = "clk_pin"
CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"

sd_card_proof_ns = cg.esphome_ns.namespace("sd_card_proof")
SdCardProof = sd_card_proof_ns.class_("SdCardProof", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SdCardProof),
        cv.Required(CONF_CLK_PIN): cv.int_range(min=0),
        cv.Required(CONF_CMD_PIN): cv.int_range(min=0),
        cv.Required(CONF_DATA0_PIN): cv.int_range(min=0),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    esp32.include_builtin_idf_component("fatfs")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_clk_pin(config[CONF_CLK_PIN]))
    cg.add(var.set_cmd_pin(config[CONF_CMD_PIN]))
    cg.add(var.set_data0_pin(config[CONF_DATA0_PIN]))
