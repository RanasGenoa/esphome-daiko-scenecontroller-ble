import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_TYPE

from .. import (
    CONF_DAIKO_SCENECONTROLLER_BLE_ID,
    DaikoMeshComponent,
    daiko_scenecontroller_ble_ns,
)

DEPENDENCIES = ["daiko_scenecontroller_ble"]

DaikoMeshSensor = daiko_scenecontroller_ble_ns.class_(
    "DaikoMeshSensor", sensor.Sensor, cg.Component
)
DaikoSensorType = daiko_scenecontroller_ble_ns.enum("DaikoSensorType", is_class=True)

SENSOR_TYPES = {
    "sequence_number": DaikoSensorType.SEQUENCE_NUMBER,
    "missed_responses": DaikoSensorType.MISSED_RESPONSES,
    "active_scene": DaikoSensorType.ACTIVE_SCENE,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        DaikoMeshSensor,
        accuracy_decimals=0,
    )
    .extend(
        {
            cv.GenerateID(CONF_DAIKO_SCENECONTROLLER_BLE_ID): cv.use_id(DaikoMeshComponent),
            cv.Required(CONF_TYPE): cv.enum(SENSOR_TYPES, lower=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    parent = await cg.get_variable(config[CONF_DAIKO_SCENECONTROLLER_BLE_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_type(config[CONF_TYPE]))
