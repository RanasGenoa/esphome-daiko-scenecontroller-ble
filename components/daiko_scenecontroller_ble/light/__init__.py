import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_ID, CONF_NAME, CONF_OUTPUT_ID, CONF_TYPE
from esphome.helpers import sanitize, snake_case

from .. import (
    CONF_CIRCUIT,
    CONF_DAIKO_SCENECONTROLLER_BLE_ID,
    CONF_MAX_MIREDS,
    CONF_MIN_MIREDS,
    LIGHT_TYPES,
    DaikoMeshComponent,
    DaikoMeshLight,
)

DEPENDENCIES = ["daiko_scenecontroller_ble"]

CONFIG_SCHEMA = light.LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(DaikoMeshLight),
        cv.GenerateID(CONF_DAIKO_SCENECONTROLLER_BLE_ID): cv.use_id(DaikoMeshComponent),
        cv.Optional(CONF_CIRCUIT, default=1): cv.int_range(min=1, max=6),
        cv.Optional(CONF_TYPE, default="auto"): cv.enum(LIGHT_TYPES, lower=True),
        cv.Optional(CONF_MIN_MIREDS, default=200.0): cv.float_range(min=100.0, max=500.0),
        cv.Optional(CONF_MAX_MIREDS, default=370.0): cv.float_range(min=100.0, max=500.0),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    # Ensure deterministic ASCII object_id when entity name is non-ASCII (e.g. Japanese)
    name_str = config.get(CONF_NAME, "")
    sanitized = sanitize(snake_case(name_str)).strip("_")
    if not sanitized or not any(c.isalpha() for c in sanitized):
        light_state_var = await cg.get_variable(config[CONF_ID])
        fallback_obj_id = (
            "master_power"
            if config[CONF_TYPE] == LIGHT_TYPES["master"]
            else f"circuit_{config[CONF_CIRCUIT]}"
        )
        cg.add(light_state_var.set_object_id(fallback_obj_id))

    parent = await cg.get_variable(config[CONF_DAIKO_SCENECONTROLLER_BLE_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_circuit(config[CONF_CIRCUIT]))
    cg.add(var.set_light_type(config[CONF_TYPE]))
    cg.add(var.set_min_mireds(config[CONF_MIN_MIREDS]))
    cg.add(var.set_max_mireds(config[CONF_MAX_MIREDS]))
