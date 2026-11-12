#include "daiko_scenecontroller_ble_binary_sensor.h"

namespace esphome {
namespace daiko_scenecontroller_ble {

static const char *const TAG = "daiko_scenecontroller_ble.binary_sensor";

void DaikoMeshBinarySensor::setup() {
  if (this->parent_ == nullptr) {
    return;
  }

  if (this->type_ == DaikoBinarySensorType::CONNECTED) {
    this->parent_->register_connectivity_callback([this](bool connected) {
      this->publish_state(connected);
    });
    this->publish_state(this->parent_->is_connected());
  }
}

void DaikoMeshBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "DAIKO Scene Controller BLE Binary Sensor:");
  if (this->type_ == DaikoBinarySensorType::CONNECTED) {
    ESP_LOGCONFIG(TAG, "  Type: Controller Connected");
  }
}

}  // namespace daiko_scenecontroller_ble
}  // namespace esphome
