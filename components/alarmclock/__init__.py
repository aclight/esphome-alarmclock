"""ESPHome custom component for the alarm clock."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c", "api"]
AUTO_LOAD = ["sensor", "switch", "button", "number"]

CONF_RTTTL_ID = "rtttl_id"

alarmclock_ns = cg.esphome_ns.namespace("alarmclock")
AlarmClockComponent = alarmclock_ns.class_(
    "AlarmClockComponent", cg.Component, i2c.I2CDevice
)

# Reference the RTTTL class from its namespace.
rtttl_ns = cg.esphome_ns.namespace("rtttl")
Rtttl = rtttl_ns.class_("Rtttl", cg.Component)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AlarmClockComponent),
            cv.Optional(CONF_RTTTL_ID): cv.use_id(Rtttl),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x30))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_RTTTL_ID in config:
        rtttl = await cg.get_variable(config[CONF_RTTTL_ID])
        cg.add(var.set_rtttl(rtttl))
