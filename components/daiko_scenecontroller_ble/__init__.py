import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, button, light, sensor, text_sensor
from esphome.const import (
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_NAME,
    CONF_OUTPUT_ID,
    CONF_TYPE,
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

# In ESPHome 2026.2+, unused built-in ESP-IDF components are excluded by default.
# include_builtin_idf_component ensures Bluetooth, BLE Mesh, and NVS are properly linked.
try:
    from esphome.components.esp32 import include_builtin_idf_component
except ImportError:
    include_builtin_idf_component = None

CODEOWNERS = ["@RanasGenoa"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["light", "button", "sensor", "binary_sensor", "text_sensor"]
MULTI_CONF = False

daiko_scenecontroller_ble_ns = cg.esphome_ns.namespace("daiko_scenecontroller_ble")
DaikoMeshComponent = daiko_scenecontroller_ble_ns.class_(
    "DaikoMeshComponent", cg.PollingComponent
)

DaikoMeshLight = daiko_scenecontroller_ble_ns.class_(
    "DaikoMeshLight", light.LightOutput, cg.Component
)
DaikoLightType = daiko_scenecontroller_ble_ns.enum("DaikoLightType", is_class=True)

DaikoMeshButton = daiko_scenecontroller_ble_ns.class_(
    "DaikoMeshButton", button.Button, cg.Component
)
DaikoButtonType = daiko_scenecontroller_ble_ns.enum("DaikoButtonType", is_class=True)

DaikoMeshSensor = daiko_scenecontroller_ble_ns.class_(
    "DaikoMeshSensor", sensor.Sensor, cg.Component
)
DaikoSensorType = daiko_scenecontroller_ble_ns.enum("DaikoSensorType", is_class=True)

DaikoMeshBinarySensor = daiko_scenecontroller_ble_ns.class_(
    "DaikoMeshBinarySensor", binary_sensor.BinarySensor, cg.Component
)
DaikoBinarySensorType = daiko_scenecontroller_ble_ns.enum(
    "DaikoBinarySensorType", is_class=True
)

DaikoMeshTextSensor = daiko_scenecontroller_ble_ns.class_(
    "DaikoMeshTextSensor", text_sensor.TextSensor, cg.Component
)
DaikoTextSensorType = daiko_scenecontroller_ble_ns.enum(
    "DaikoTextSensorType", is_class=True
)

CONF_DAIKO_SCENECONTROLLER_BLE_ID = "daiko_scenecontroller_ble_id"
CONF_NET_KEY = "net_key"
CONF_APP_KEY = "app_key"
CONF_CONTROLLER_ADDRESS = "controller_address"
CONF_GROUP_ADDRESS = "group_address"
CONF_SELF_ADDRESS = "self_address"
CONF_AUTO_RECOVERY_TIMEOUT = "auto_recovery_timeout"
CONF_AUTO_CREATE_ENTITIES = "auto_create_entities"

CONF_CIRCUIT = "circuit"
CONF_MIN_MIREDS = "min_mireds"
CONF_MAX_MIREDS = "max_mireds"
CONF_SCENE_ID = "scene_id"
CONF_OBJECT_ID_OVERRIDE = "object_id_override"

CONF_AUTO_LIGHTS = "_auto_lights"
CONF_AUTO_BUTTONS = "_auto_buttons"
CONF_AUTO_BINARY_SENSORS = "_auto_binary_sensors"
CONF_AUTO_SENSORS = "_auto_sensors"
CONF_AUTO_TEXT_SENSORS = "_auto_text_sensors"

LIGHT_TYPES = {
    "auto": DaikoLightType.AUTO,
    "dimmable": DaikoLightType.DIMMABLE,
    "tunable_white": DaikoLightType.TUNABLE_WHITE,
    "on_off": DaikoLightType.ON_OFF,
    "master": DaikoLightType.MASTER,
}

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

SENSOR_TYPES = {
    "sequence_number": DaikoSensorType.SEQUENCE_NUMBER,
    "missed_responses": DaikoSensorType.MISSED_RESPONSES,
    "active_scene": DaikoSensorType.ACTIVE_SCENE,
}

BINARY_SENSOR_TYPES = {
    "connected": DaikoBinarySensorType.CONNECTED,
}

TEXT_SENSOR_TYPES = {
    "self_address": DaikoTextSensorType.SELF_ADDRESS,
    "connection_status": DaikoTextSensorType.CONNECTION_STATUS,
    "pairing_state": DaikoTextSensorType.PAIRING_STATUS,
    "pairing_status": DaikoTextSensorType.PAIRING_STATUS,
}


def validate_hex_key_16(value):
    value = cv.string_strict(value).strip()
    if len(value) != 32:
        raise cv.Invalid("Key must be exactly 32 hexadecimal characters (16 bytes)")
    try:
        int(value, 16)
    except ValueError as err:
        raise cv.Invalid(
            "Key must contain only hexadecimal characters (0-9, A-F)"
        ) from err
    return value.upper()


AUTO_LIGHT_SCHEMA = light.LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(DaikoMeshLight),
        cv.Optional(CONF_CIRCUIT, default=1): cv.int_range(min=1, max=6),
        cv.Optional(CONF_TYPE, default="auto"): cv.enum(LIGHT_TYPES, lower=True),
        cv.Optional(CONF_MIN_MIREDS, default=200.0): cv.float_range(
            min=100.0, max=500.0
        ),
        cv.Optional(CONF_MAX_MIREDS, default=370.0): cv.float_range(
            min=100.0, max=500.0
        ),
        cv.Optional(CONF_OBJECT_ID_OVERRIDE): cv.string_strict,
    }
).extend(cv.COMPONENT_SCHEMA)

AUTO_BUTTON_SCHEMA = (
    button.button_schema(DaikoMeshButton)
    .extend(
        {
            cv.Required(CONF_TYPE): cv.enum(BUTTON_TYPES, lower=True),
            cv.Optional(CONF_SCENE_ID, default=1): cv.int_range(min=1, max=6),
            cv.Optional(CONF_OBJECT_ID_OVERRIDE): cv.string_strict,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

AUTO_BINARY_SENSOR_SCHEMA = (
    binary_sensor.binary_sensor_schema(
        DaikoMeshBinarySensor,
        device_class=DEVICE_CLASS_CONNECTIVITY,
    )
    .extend(
        {
            cv.Required(CONF_TYPE): cv.enum(BINARY_SENSOR_TYPES, lower=True),
            cv.Optional(CONF_OBJECT_ID_OVERRIDE): cv.string_strict,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

AUTO_SENSOR_SCHEMA = (
    sensor.sensor_schema(
        DaikoMeshSensor,
        accuracy_decimals=0,
    )
    .extend(
        {
            cv.Required(CONF_TYPE): cv.enum(SENSOR_TYPES, lower=True),
            cv.Optional(CONF_OBJECT_ID_OVERRIDE): cv.string_strict,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

AUTO_TEXT_SENSOR_SCHEMA = (
    text_sensor.text_sensor_schema(DaikoMeshTextSensor)
    .extend(
        {
            cv.Required(CONF_TYPE): cv.enum(TEXT_SENSOR_TYPES, lower=True),
            cv.Optional(CONF_OBJECT_ID_OVERRIDE): cv.string_strict,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


def _default_auto_lights():
    items = [
        {
            CONF_ID: "light_master_power",
            CONF_NAME: "Master Power",
            CONF_TYPE: "master",
            CONF_ICON: "mdi:power",
            CONF_OBJECT_ID_OVERRIDE: "master_power",
        }
    ]
    for c in range(1, 7):
        items.append(
            {
                CONF_ID: f"light_circuit_{c}",
                CONF_NAME: f"Circuit {c}",
                CONF_CIRCUIT: c,
                CONF_TYPE: "auto",
                CONF_OBJECT_ID_OVERRIDE: f"circuit_{c}",
            }
        )
    return items


def _default_auto_buttons():
    items = []
    for s in range(1, 7):
        items.append(
            {
                CONF_ID: f"button_scene_{s}",
                CONF_NAME: f"Scene {s}",
                CONF_TYPE: "scene",
                CONF_SCENE_ID: s,
                CONF_OBJECT_ID_OVERRIDE: f"scene_{s}",
            }
        )
    items.extend(
        [
            {
                CONF_ID: "button_all_on",
                CONF_NAME: "All On",
                CONF_TYPE: "all_on",
                CONF_OBJECT_ID_OVERRIDE: "all_on",
            },
            {
                CONF_ID: "button_all_off",
                CONF_NAME: "All Off",
                CONF_TYPE: "all_off",
                CONF_OBJECT_ID_OVERRIDE: "all_off",
            },
            {
                CONF_ID: "button_query_status",
                CONF_NAME: "Sync Status",
                CONF_TYPE: "query_status",
                CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
                CONF_OBJECT_ID_OVERRIDE: "query_status",
            },
            {
                CONF_ID: "button_rotate_address",
                CONF_NAME: "Rotate Address",
                CONF_TYPE: "rotate_address",
                CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
                CONF_OBJECT_ID_OVERRIDE: "rotate_address",
            },
            {
                CONF_ID: "button_reset_sequence",
                CONF_NAME: "Reset Sequence Number",
                CONF_TYPE: "reset_sequence",
                CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
                CONF_OBJECT_ID_OVERRIDE: "reset_sequence",
            },
            {
                CONF_ID: "button_start_pairing",
                CONF_NAME: "Start Pairing",
                CONF_TYPE: "start_pairing",
                CONF_ICON: "mdi:bluetooth-connect",
                CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
                CONF_OBJECT_ID_OVERRIDE: "start_pairing",
            },
            {
                CONF_ID: "button_cancel_pairing",
                CONF_NAME: "Cancel Pairing",
                CONF_TYPE: "cancel_pairing",
                CONF_ICON: "mdi:bluetooth-off",
                CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
                CONF_OBJECT_ID_OVERRIDE: "cancel_pairing",
            },
        ]
    )
    return items


def _default_auto_binary_sensors():
    return [
        {
            CONF_ID: "binary_sensor_connected",
            CONF_NAME: "Connected",
            CONF_TYPE: "connected",
            CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
            CONF_OBJECT_ID_OVERRIDE: "connected",
        }
    ]


def _default_auto_sensors():
    return [
        {
            CONF_ID: "sensor_sequence_number",
            CONF_NAME: "Sequence Number",
            CONF_TYPE: "sequence_number",
            CONF_ICON: "mdi:counter",
            CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
            CONF_OBJECT_ID_OVERRIDE: "sequence_number",
        },
        {
            CONF_ID: "sensor_missed_responses",
            CONF_NAME: "Missed Responses",
            CONF_TYPE: "missed_responses",
            CONF_ICON: "mdi:alert-circle-outline",
            CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
            CONF_OBJECT_ID_OVERRIDE: "missed_responses",
        },
        {
            CONF_ID: "sensor_active_scene",
            CONF_NAME: "Active Scene",
            CONF_TYPE: "active_scene",
            CONF_ICON: "mdi:palette",
            CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
            CONF_OBJECT_ID_OVERRIDE: "active_scene",
        },
    ]


def _default_auto_text_sensors():
    return [
        {
            CONF_ID: "text_sensor_self_address",
            CONF_NAME: "Self Address",
            CONF_TYPE: "self_address",
            CONF_ICON: "mdi:card-account-details-outline",
            CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
            CONF_OBJECT_ID_OVERRIDE: "self_address",
        },
        {
            CONF_ID: "text_sensor_connection_status",
            CONF_NAME: "Connection Status",
            CONF_TYPE: "connection_status",
            CONF_ICON: "mdi:bluetooth-connect",
            CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
            CONF_OBJECT_ID_OVERRIDE: "connection_status",
        },
        {
            CONF_ID: "text_sensor_pairing_state",
            CONF_NAME: "Pairing State",
            CONF_TYPE: "pairing_state",
            CONF_ICON: "mdi:progress-wrench",
            CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
            CONF_OBJECT_ID_OVERRIDE: "pairing_state",
        },
    ]


def _populate_auto_entities(config):
    config = dict(config)
    if config.get(CONF_AUTO_CREATE_ENTITIES, True):
        config[CONF_AUTO_LIGHTS] = cv.Schema([AUTO_LIGHT_SCHEMA])(
            _default_auto_lights()
        )
        config[CONF_AUTO_BUTTONS] = cv.Schema([AUTO_BUTTON_SCHEMA])(
            _default_auto_buttons()
        )
        config[CONF_AUTO_BINARY_SENSORS] = cv.Schema([AUTO_BINARY_SENSOR_SCHEMA])(
            _default_auto_binary_sensors()
        )
        config[CONF_AUTO_SENSORS] = cv.Schema([AUTO_SENSOR_SCHEMA])(
            _default_auto_sensors()
        )
        config[CONF_AUTO_TEXT_SENSORS] = cv.Schema([AUTO_TEXT_SENSOR_SCHEMA])(
            _default_auto_text_sensors()
        )
    else:
        config[CONF_AUTO_LIGHTS] = []
        config[CONF_AUTO_BUTTONS] = []
        config[CONF_AUTO_BINARY_SENSORS] = []
        config[CONF_AUTO_SENSORS] = []
        config[CONF_AUTO_TEXT_SENSORS] = []
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DaikoMeshComponent),
            cv.Required(CONF_NET_KEY): validate_hex_key_16,
            cv.Required(CONF_APP_KEY): validate_hex_key_16,
            cv.Optional(CONF_CONTROLLER_ADDRESS, default=0x0004): cv.hex_uint16_t,
            cv.Optional(CONF_GROUP_ADDRESS, default=0xC078): cv.hex_uint16_t,
            cv.Optional(CONF_SELF_ADDRESS, default=0x0020): cv.hex_uint16_t,
            cv.Optional(
                CONF_AUTO_RECOVERY_TIMEOUT, default="90s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_AUTO_CREATE_ENTITIES, default=True): cv.boolean,
        }
    ).extend(cv.polling_component_schema("5s")),
    _populate_auto_entities,
)


async def to_code(config):
    if include_builtin_idf_component is not None:
        include_builtin_idf_component("bt")
        include_builtin_idf_component("esp_ble_mesh")
        include_builtin_idf_component("nvs_flash")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_net_key(config[CONF_NET_KEY]))
    cg.add(var.set_app_key(config[CONF_APP_KEY]))
    cg.add(var.set_controller_address(config[CONF_CONTROLLER_ADDRESS]))
    cg.add(var.set_group_address(config[CONF_GROUP_ADDRESS]))
    cg.add(var.set_self_address(config[CONF_SELF_ADDRESS]))
    cg.add(var.set_auto_recovery_timeout(config[CONF_AUTO_RECOVERY_TIMEOUT]))

    for l_conf in config.get(CONF_AUTO_LIGHTS, []):
        l_out = cg.new_Pvariable(l_conf[CONF_OUTPUT_ID])
        await cg.register_component(l_out, l_conf)
        await light.register_light(l_out, l_conf)
        cg.add(l_out.set_parent(var))
        cg.add(l_out.set_circuit(l_conf[CONF_CIRCUIT]))
        cg.add(l_out.set_light_type(l_conf[CONF_TYPE]))
        cg.add(l_out.set_min_mireds(l_conf[CONF_MIN_MIREDS]))
        cg.add(l_out.set_max_mireds(l_conf[CONF_MAX_MIREDS]))

    for b_conf in config.get(CONF_AUTO_BUTTONS, []):
        b_var = cg.new_Pvariable(b_conf[CONF_ID])
        await cg.register_component(b_var, b_conf)
        await button.register_button(b_var, b_conf)
        cg.add(b_var.set_parent(var))
        cg.add(b_var.set_button_type(b_conf[CONF_TYPE]))
        if b_conf[CONF_TYPE] == BUTTON_TYPES["scene"]:
            cg.add(b_var.set_scene_id(b_conf[CONF_SCENE_ID]))

    for bs_conf in config.get(CONF_AUTO_BINARY_SENSORS, []):
        bs_var = cg.new_Pvariable(bs_conf[CONF_ID])
        await cg.register_component(bs_var, bs_conf)
        await binary_sensor.register_binary_sensor(bs_var, bs_conf)
        cg.add(bs_var.set_parent(var))
        cg.add(bs_var.set_type(bs_conf[CONF_TYPE]))

    for s_conf in config.get(CONF_AUTO_SENSORS, []):
        s_var = cg.new_Pvariable(s_conf[CONF_ID])
        await cg.register_component(s_var, s_conf)
        await sensor.register_sensor(s_var, s_conf)
        cg.add(s_var.set_parent(var))
        cg.add(s_var.set_type(s_conf[CONF_TYPE]))

    for ts_conf in config.get(CONF_AUTO_TEXT_SENSORS, []):
        ts_var = cg.new_Pvariable(ts_conf[CONF_ID])
        await cg.register_component(ts_var, ts_conf)
        await text_sensor.register_text_sensor(ts_var, ts_conf)
        cg.add(ts_var.set_parent(var))
        cg.add(ts_var.set_type(ts_conf[CONF_TYPE]))
        if ts_conf[CONF_TYPE] == TEXT_SENSOR_TYPES["pairing_state"]:
            cg.add(var.set_pairing_state_text_sensor(ts_var))
