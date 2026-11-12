#include "daiko_scenecontroller_ble_sensor.h"

namespace esphome {
namespace daiko_scenecontroller_ble {

static const char *const TAG = "daiko_scenecontroller_ble.sensor";

void DaikoMeshSensor::setup() {
  if (this->parent_ == nullptr) {
    return;
  }

  if (this->type_ == DaikoSensorType::SEQUENCE_NUMBER) {
    this->parent_->register_sequence_callback([this](uint32_t seq) {
      this->publish_state(static_cast<float>(seq));
    });
    this->publish_state(static_cast<float>(this->parent_->get_sequence_number()));
  } else if (this->type_ == DaikoSensorType::MISSED_RESPONSES) {
    this->parent_->register_missed_callback([this](uint32_t missed) {
      this->publish_state(static_cast<float>(missed));
    });
    this->publish_state(static_cast<float>(this->parent_->get_missed_responses()));
  } else if (this->type_ == DaikoSensorType::ACTIVE_SCENE) {
    this->parent_->register_active_scene_callback([this](uint8_t scene) {
      this->publish_state(static_cast<float>(scene));
    });
    this->publish_state(static_cast<float>(this->parent_->get_active_scene() + 1));
  }
}

void DaikoMeshSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "DAIKO Scene Controller BLE Sensor:");
  if (this->type_ == DaikoSensorType::SEQUENCE_NUMBER) {
    ESP_LOGCONFIG(TAG, "  Type: Sequence Number");
  } else if (this->type_ == DaikoSensorType::MISSED_RESPONSES) {
    ESP_LOGCONFIG(TAG, "  Type: Missed Responses");
  } else if (this->type_ == DaikoSensorType::ACTIVE_SCENE) {
    ESP_LOGCONFIG(TAG, "  Type: Active Scene");
  }
}

}  // namespace daiko_scenecontroller_ble
}  // namespace esphome
