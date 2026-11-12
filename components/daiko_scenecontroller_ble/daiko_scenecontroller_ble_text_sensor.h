#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "daiko_scenecontroller_ble.h"

namespace esphome {
namespace daiko_scenecontroller_ble {

enum class DaikoTextSensorType : uint8_t {
  SELF_ADDRESS = 0,
  CONNECTION_STATUS = 1,
  PAIRING_STATUS = 2,
};

class DaikoMeshTextSensor : public text_sensor::TextSensor, public Component {
 public:
  void set_parent(DaikoMeshComponent *parent) { this->parent_ = parent; }
  void set_type(DaikoTextSensorType type) { this->type_ = type; }

  void setup() override;
  void dump_config() override;

 protected:
  DaikoMeshComponent *parent_{nullptr};
  DaikoTextSensorType type_{DaikoTextSensorType::SELF_ADDRESS};
};

}  // namespace daiko_scenecontroller_ble
}  // namespace esphome
