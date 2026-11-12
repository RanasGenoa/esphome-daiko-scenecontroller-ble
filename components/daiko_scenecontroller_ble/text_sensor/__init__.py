import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_TYPE

from .. import (
    CONF_DAIKO_SCENECONTROLLER_BLE_ID,
    DaikoMeshComponent,
    daiko_scenecontroller_ble_ns,
)

DEPENDENCIES = ["daiko_scenecontroller_ble"]

DaikoMeshTextSensor = daiko_scenecontroller_ble_ns.class_(
    "DaikoMeshTextSensor", text_sensor.TextSensor, cg.Component
)
DaikoTextSensorType = daiko_scenecontroller_ble_ns.enum("DaikoTextSensorType", is_class=True)

TEXT_SENSOR_TYPES = {
    "self_address": DaikoTextSensorType.SELF_ADDRESS,
    "connection_status": DaikoTextSensorType.CONNECTION_STATUS,
    "pairing_state": DaikoTextSensorType.PAIRING_STATUS,
    "pairing_status": DaikoTextSensorType.PAIRING_STATUS,
}

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(DaikoMeshTextSensor)
    .extend(
        {
            cv.GenerateID(CONF_DAIKO_SCENECONTROLLER_BLE_ID): cv.use_id(DaikoMeshComponent),
            cv.Required(CONF_TYPE): cv.enum(TEXT_SENSOR_TYPES, lower=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await text_sensor.register_text_sensor(var, config)

    parent = await cg.get_variable(config[CONF_DAIKO_SCENECONTROLLER_BLE_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_type(config[CONF_TYPE]))
