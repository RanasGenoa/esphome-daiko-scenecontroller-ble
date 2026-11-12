#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/component.h"
#include "daiko_scenecontroller_ble.h"

namespace esphome {
namespace daiko_scenecontroller_ble {

enum class DaikoButtonType : uint8_t {
  SCENE = 0,
  ALL_ON = 1,
  ALL_OFF = 2,
  QUERY_STATUS = 3,
  ROTATE_ADDRESS = 4,
  RESET_SEQUENCE = 5,
  START_PAIRING = 6,
  CANCEL_PAIRING = 7,
};

class DaikoMeshButton : public button::Button, public Component {
 public:
  void set_parent(DaikoMeshComponent *parent) { this->parent_ = parent; }
  void set_button_type(DaikoButtonType type) { this->type_ = type; }
  void set_scene_id(uint8_t scene_id) { this->scene_id_ = scene_id; }

  void dump_config() override;

 protected:
  void press_action() override;

  DaikoMeshComponent *parent_{nullptr};
  DaikoButtonType type_{DaikoButtonType::ALL_ON};
  uint8_t scene_id_{1};
};

}  // namespace daiko_scenecontroller_ble
}  // namespace esphome
