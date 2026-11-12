import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID, CONF_TYPE, DEVICE_CLASS_CONNECTIVITY

from .. import (
    CONF_DAIKO_SCENECONTROLLER_BLE_ID,
    DaikoMeshComponent,
    daiko_scenecontroller_ble_ns,
)

DEPENDENCIES = ["daiko_scenecontroller_ble"]

DaikoMeshBinarySensor = daiko_scenecontroller_ble_ns.class_(
    "DaikoMeshBinarySensor", binary_sensor.BinarySensor, cg.Component
)
DaikoBinarySensorType = daiko_scenecontroller_ble_ns.enum("DaikoBinarySensorType", is_class=True)

BINARY_SENSOR_TYPES = {
    "connected": DaikoBinarySensorType.CONNECTED,
}

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(
        DaikoMeshBinarySensor,
        device_class=DEVICE_CLASS_CONNECTIVITY,
    )
    .extend(
        {
            cv.GenerateID(CONF_DAIKO_SCENECONTROLLER_BLE_ID): cv.use_id(DaikoMeshComponent),
            cv.Required(CONF_TYPE): cv.enum(BINARY_SENSOR_TYPES, lower=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)

    parent = await cg.get_variable(config[CONF_DAIKO_SCENECONTROLLER_BLE_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_type(config[CONF_TYPE]))
