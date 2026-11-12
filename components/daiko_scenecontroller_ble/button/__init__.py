import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID, CONF_TYPE

from .. import (
    CONF_DAIKO_SCENECONTROLLER_BLE_ID,
    DaikoMeshComponent,
    daiko_scenecontroller_ble_ns,
)

DEPENDENCIES = ["daiko_scenecontroller_ble"]

DaikoMeshButton = daiko_scenecontroller_ble_ns.class_(
    "DaikoMeshButton", button.Button, cg.Component
)
DaikoButtonType = daiko_scenecontroller_ble_ns.enum("DaikoButtonType", is_class=True)

CONF_SCENE_ID = "scene_id"

BUTTON_TYPES = {
    "scene": DaikoButtonType.SCENE,
    "all_on": DaikoButtonType.ALL_ON,
    "all_off": DaikoButtonType.ALL_OFF,
    "query_status": DaikoButtonType.QUERY_STATUS,
    "rotate_address": DaikoButtonType.ROTATE_ADDRESS,
    "reset_sequence": DaikoButtonType.RESET_SEQUENCE,
    "start_pairing": DaikoButtonType.START_PAIRING,
    "cancel_pairing": DaikoButtonType.CANCEL_PAIRING,
}

CONFIG_SCHEMA = (
    button.button_schema(DaikoMeshButton)
    .extend(
        {
            cv.GenerateID(CONF_DAIKO_SCENECONTROLLER_BLE_ID): cv.use_id(
                DaikoMeshComponent
            ),
            cv.Required(CONF_TYPE): cv.enum(BUTTON_TYPES, lower=True),
            cv.Optional(CONF_SCENE_ID, default=1): cv.int_range(min=1, max=6),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await button.register_button(var, config)

    parent = await cg.get_variable(config[CONF_DAIKO_SCENECONTROLLER_BLE_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_button_type(config[CONF_TYPE]))
    if config[CONF_TYPE] == "scene":
        cg.add(var.set_scene_id(config[CONF_SCENE_ID]))
