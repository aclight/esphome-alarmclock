"""ESPHome custom component for the alarm clock."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import i2c
from esphome.const import CONF_ENTITY_ID, CONF_ID, CONF_NAME, CONF_ON_PRESS, CONF_TYPE

DEPENDENCIES = ["i2c", "api"]
AUTO_LOAD = ["sensor", "switch", "button", "number", "adaptive_brightness"]

CONF_RTTTL_ID = "rtttl_id"
CONF_SPEAKER_ID = "speaker_id"
CONF_BACKLIGHT_MODE = "backlight_mode"
CONF_SPEAKER_AMP_MODE = "speaker_amp_mode"
CONF_CONTROLS_PAGE = "controls_page"
CONF_CONTROLS = "controls"
CONF_MINISPLIT_PAGE = "minisplit_page"
CONF_CLIMATE_ENTITY_ID = "climate_entity_id"
CONF_HORIZONTAL_VANE_ENTITY_ID = "horizontal_vane_entity_id"
CONF_VERTICAL_VANE_ENTITY_ID = "vertical_vane_entity_id"
CONF_FAN_MODES = "fan_modes"
CONF_HVAC_MODES = "hvac_modes"
CONF_MIN_TEMPERATURE = "min_temperature"
CONF_MAX_TEMPERATURE = "max_temperature"
CONF_TEMPERATURE_STEP = "temperature_step"
CONF_TEMPERATURE_UNIT = "temperature_unit"

CONTROL_TYPES = {
    "toggle": 0,
    "action": 1,
    "dimmer": 2,
}

BACKLIGHT_MODES = {
    "legacy_raw": 0,
    "stc8_register": 1,
}

SPEAKER_AMP_MODES = {
    "legacy_command": 0,
    "stc8_register": 1,
}

alarmclock_ns = cg.esphome_ns.namespace("alarmclock")
AlarmClockComponent = alarmclock_ns.class_(
    "AlarmClockComponent", cg.Component, i2c.I2CDevice
)

# Reference the RTTTL class from its namespace.
rtttl_ns = cg.esphome_ns.namespace("rtttl")
Rtttl = rtttl_ns.class_("Rtttl", cg.Component)
speaker_ns = cg.esphome_ns.namespace("speaker")
Speaker = speaker_ns.class_("Speaker", cg.Component)

CONTROL_COMMON_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NAME): cv.string_strict,
    }
)

CONTROL_SCHEMA = cv.typed_schema(
    {
        "toggle": CONTROL_COMMON_SCHEMA.extend(
            {cv.Required(CONF_ENTITY_ID): cv.entity_id}
        ),
        "action": CONTROL_COMMON_SCHEMA.extend(
            {
                cv.Required(CONF_ON_PRESS): automation.validate_automation(single=True),
            }
        ),
        "dimmer": CONTROL_COMMON_SCHEMA.extend(
            {cv.Required(CONF_ENTITY_ID): cv.entity_id}
        ),
    },
    key=CONF_TYPE,
)

CONTROLS_PAGE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_CONTROLS, default=[]): cv.All(
            cv.ensure_list(CONTROL_SCHEMA), cv.Length(max=6)
        ),
    }
)

def _climate_option(value):
    # YAML 1.1 parses an unquoted `off`/`on` as a boolean.
    if isinstance(value, bool):
        return "on" if value else "off"
    return cv.string_strict(value)


MINISPLIT_PAGE_SCHEMA = cv.Schema(
    {
        cv.Optional(
            CONF_CLIMATE_ENTITY_ID, default="climate.master_bedroom_climate"
        ): cv.entity_id,
        cv.Optional(
            CONF_HORIZONTAL_VANE_ENTITY_ID,
            default="select.mitsubishi_heat_pump_horizontal_vane",
        ): cv.entity_id,
        cv.Optional(
            CONF_VERTICAL_VANE_ENTITY_ID,
            default="select.mitsubishi_heat_pump_vertical_vane",
        ): cv.entity_id,
        cv.Optional(
            CONF_HVAC_MODES,
            default=["off", "heat", "cool", "heat_cool", "dry", "fan_only"],
        ): cv.All(cv.ensure_list(_climate_option), cv.Length(min=1, max=8)),
        cv.Optional(
            CONF_FAN_MODES,
            default=["auto", "quiet", "low", "medium", "high"],
        ): cv.All(cv.ensure_list(_climate_option), cv.Length(min=1, max=8)),
        cv.Optional(CONF_MIN_TEMPERATURE, default=60.0): cv.float_,
        cv.Optional(CONF_MAX_TEMPERATURE, default=88.0): cv.float_,
        cv.Optional(CONF_TEMPERATURE_STEP, default=1.0): cv.positive_float,
        cv.Optional(CONF_TEMPERATURE_UNIT, default="°F"): cv.string_strict,
    }
)


def _validate_minisplit(config):
    if CONF_MINISPLIT_PAGE not in config:
        return config
    minisplit = config[CONF_MINISPLIT_PAGE]
    if minisplit[CONF_MIN_TEMPERATURE] >= minisplit[CONF_MAX_TEMPERATURE]:
        raise cv.Invalid("min_temperature must be less than max_temperature")
    return config


CONFIG_SCHEMA = (
    cv.All(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(AlarmClockComponent),
                cv.Optional(CONF_RTTTL_ID): cv.use_id(Rtttl),
                cv.Optional(CONF_SPEAKER_ID): cv.use_id(Speaker),
                cv.Optional(CONF_BACKLIGHT_MODE, default="legacy_raw"): cv.enum(
                    BACKLIGHT_MODES, lower=True
                ),
                cv.Optional(
                    CONF_SPEAKER_AMP_MODE, default="legacy_command"
                ): cv.enum(SPEAKER_AMP_MODES, lower=True),
                cv.Optional(CONF_CONTROLS_PAGE): CONTROLS_PAGE_SCHEMA,
                cv.Optional(CONF_MINISPLIT_PAGE): MINISPLIT_PAGE_SCHEMA,
            }
        )
        .extend(cv.COMPONENT_SCHEMA)
        .extend(i2c.i2c_device_schema(0x30)),
        _validate_minisplit,
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_RTTTL_ID in config:
        rtttl = await cg.get_variable(config[CONF_RTTTL_ID])
        cg.add(var.set_rtttl(rtttl))
    if CONF_SPEAKER_ID in config:
        speaker = await cg.get_variable(config[CONF_SPEAKER_ID])
        cg.add(var.set_speaker(speaker))

    cg.add(var.set_backlight_mode(config[CONF_BACKLIGHT_MODE]))
    cg.add(var.set_speaker_amp_mode(config[CONF_SPEAKER_AMP_MODE]))

    if controls_page := config.get(CONF_CONTROLS_PAGE):
        cg.add(var.enable_controls_page())
        for index, control in enumerate(controls_page[CONF_CONTROLS]):
            entity_id = control.get(CONF_ENTITY_ID, "")
            cg.add(
                var.add_control(
                    CONTROL_TYPES[control[CONF_TYPE]],
                    control[CONF_NAME],
                    entity_id,
                )
            )
            if control[CONF_TYPE] == "action":
                await automation.build_automation(
                    var.get_control_action_trigger(index),
                    [],
                    control[CONF_ON_PRESS],
                )

    if minisplit := config.get(CONF_MINISPLIT_PAGE):
        cg.add(var.enable_minisplit_page())
        cg.add(var.set_climate_entity_id(minisplit[CONF_CLIMATE_ENTITY_ID]))
        cg.add(
            var.set_horizontal_vane_entity_id(
                minisplit[CONF_HORIZONTAL_VANE_ENTITY_ID]
            )
        )
        cg.add(
            var.set_vertical_vane_entity_id(
                minisplit[CONF_VERTICAL_VANE_ENTITY_ID]
            )
        )
        cg.add(
            var.set_temperature_limits(
                minisplit[CONF_MIN_TEMPERATURE],
                minisplit[CONF_MAX_TEMPERATURE],
                minisplit[CONF_TEMPERATURE_STEP],
            )
        )
        cg.add(var.set_temperature_unit(minisplit[CONF_TEMPERATURE_UNIT]))
        for mode in minisplit[CONF_HVAC_MODES]:
            cg.add(var.add_hvac_mode(mode))
        for mode in minisplit[CONF_FAN_MODES]:
            cg.add(var.add_fan_mode(mode))
