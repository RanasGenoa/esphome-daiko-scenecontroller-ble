#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "daiko_scenecontroller_ble.h"

namespace esphome {
namespace daiko_scenecontroller_ble {

enum class DaikoSensorType : uint8_t {
  SEQUENCE_NUMBER = 0,
  MISSED_RESPONSES = 1,
  ACTIVE_SCENE = 2,
};

class DaikoMeshSensor : public sensor::Sensor, public Component {
 public:
  void set_parent(DaikoMeshComponent *parent) { this->parent_ = parent; }
  void set_type(DaikoSensorType type) { this->type_ = type; }

  void setup() override;
  void dump_config() override;

 protected:
  DaikoMeshComponent *parent_{nullptr};
  DaikoSensorType type_{DaikoSensorType::SEQUENCE_NUMBER};
};

}  // namespace daiko_scenecontroller_ble
}  // namespace esphome
