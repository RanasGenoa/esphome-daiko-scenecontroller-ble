#include "daiko_scenecontroller_ble_text_sensor.h"

namespace esphome {
namespace daiko_scenecontroller_ble {

static const char *const TAG = "daiko_scenecontroller_ble.text_sensor";

void DaikoMeshTextSensor::setup() {
  if (this->parent_ == nullptr) {
    return;
  }

  if (this->type_ == DaikoTextSensorType::SELF_ADDRESS) {
    this->parent_->register_address_callback([this](uint16_t addr) {
      char buf[16];
      snprintf(buf, sizeof(buf), "0x%04X", addr);
      this->publish_state(buf);
    });
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%04X", this->parent_->get_self_address());
    this->publish_state(buf);
  } else if (this->type_ == DaikoTextSensorType::CONNECTION_STATUS) {
    this->parent_->register_connectivity_callback([this](bool connected) {
      this->publish_state(connected ? "online" : "offline");
    });
    this->publish_state(this->parent_->is_connected() ? "online" : "offline");
  } else if (this->type_ == DaikoTextSensorType::PAIRING_STATUS) {
    this->publish_state(this->parent_->get_pairing_state_str());
  }
}

void DaikoMeshTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "DAIKO Scene Controller BLE Text Sensor:");
  if (this->type_ == DaikoTextSensorType::SELF_ADDRESS) {
    ESP_LOGCONFIG(TAG, "  Type: Self Address");
  } else if (this->type_ == DaikoTextSensorType::CONNECTION_STATUS) {
    ESP_LOGCONFIG(TAG, "  Type: Connection Status");
  } else if (this->type_ == DaikoTextSensorType::PAIRING_STATUS) {
    ESP_LOGCONFIG(TAG, "  Type: Pairing Status");
  }
}

}  // namespace daiko_scenecontroller_ble
}  // namespace esphome
