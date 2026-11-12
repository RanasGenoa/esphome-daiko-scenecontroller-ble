#include "daiko_scenecontroller_ble_button.h"

namespace esphome {
namespace daiko_scenecontroller_ble {

static const char *const TAG = "daiko_scenecontroller_ble.button";

void DaikoMeshButton::press_action() {
  if (this->parent_ == nullptr) {
    return;
  }

  switch (this->type_) {
    case DaikoButtonType::SCENE:
      this->parent_->recall_scene(this->scene_id_);
      break;
    case DaikoButtonType::ALL_ON:
      this->parent_->set_master_power(true);
      break;
    case DaikoButtonType::ALL_OFF:
      this->parent_->set_master_power(false);
      break;
    case DaikoButtonType::QUERY_STATUS:
      this->parent_->query_status();
      this->parent_->query_ctrl_info();
      break;
    case DaikoButtonType::ROTATE_ADDRESS:
      this->parent_->rotate_self_address();
      break;
    case DaikoButtonType::RESET_SEQUENCE:
      this->parent_->reset_sequence_number();
      break;
    case DaikoButtonType::START_PAIRING:
      this->parent_->start_pairing(60);
      break;
    case DaikoButtonType::CANCEL_PAIRING:
      this->parent_->cancel_pairing();
      break;
  }
}

void DaikoMeshButton::dump_config() {
  ESP_LOGCONFIG(TAG, "DAIKO Scene Controller BLE Button:");
  switch (this->type_) {
    case DaikoButtonType::SCENE:
      ESP_LOGCONFIG(TAG, "  Type: Scene Recall (Scene %u)", this->scene_id_);
      break;
    case DaikoButtonType::ALL_ON:
      ESP_LOGCONFIG(TAG, "  Type: All ON (Master Power ON)");
      break;
    case DaikoButtonType::ALL_OFF:
      ESP_LOGCONFIG(TAG, "  Type: All OFF (Master Power OFF)");
      break;
    case DaikoButtonType::QUERY_STATUS:
      ESP_LOGCONFIG(TAG, "  Type: Query Status");
      break;
    case DaikoButtonType::ROTATE_ADDRESS:
      ESP_LOGCONFIG(TAG, "  Type: Rotate Address");
      break;
    case DaikoButtonType::RESET_SEQUENCE:
      ESP_LOGCONFIG(TAG, "  Type: Reset Sequence");
      break;
    case DaikoButtonType::START_PAIRING:
      ESP_LOGCONFIG(TAG, "  Type: Start Pairing (Standalone Provisioning)");
      break;
    case DaikoButtonType::CANCEL_PAIRING:
      ESP_LOGCONFIG(TAG, "  Type: Cancel Pairing");
      break;
  }
}

}  // namespace daiko_scenecontroller_ble
}  // namespace esphome
