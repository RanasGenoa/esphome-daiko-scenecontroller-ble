#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"
#include "daiko_scenecontroller_ble.h"

namespace esphome {
namespace daiko_scenecontroller_ble {

enum class DaikoBinarySensorType : uint8_t {
  CONNECTED = 0,
};

class DaikoMeshBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void set_parent(DaikoMeshComponent *parent) { this->parent_ = parent; }
  void set_type(DaikoBinarySensorType type) { this->type_ = type; }

  void setup() override;
  void dump_config() override;

 protected:
  DaikoMeshComponent *parent_{nullptr};
  DaikoBinarySensorType type_{DaikoBinarySensorType::CONNECTED};
};

}  // namespace daiko_scenecontroller_ble
}  // namespace esphome
