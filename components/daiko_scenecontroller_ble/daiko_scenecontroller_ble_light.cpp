#include "daiko_scenecontroller_ble_light.h"

#include <algorithm>
#include <cmath>

namespace esphome {
namespace daiko_scenecontroller_ble {

static const char *const TAG = "daiko_scenecontroller_ble.light";

DaikoLightType DaikoMeshLight::resolve_effective_type_() const {
  if (this->type_ != DaikoLightType::AUTO) {
    return this->type_;
  }
  if (this->parent_ == nullptr) {
    return DaikoLightType::DIMMABLE;
  }
  const HardwareCircuitType hw_type = this->parent_->get_circuit_hw_type(this->circuit_);
  switch (hw_type) {
    case HardwareCircuitType::L3_ON_OFF:
      return DaikoLightType::ON_OFF;
    case HardwareCircuitType::L5_TUNABLE_PWM:
    case HardwareCircuitType::L6_TUNABLE_PHASE:
      return DaikoLightType::TUNABLE_WHITE;
    case HardwareCircuitType::L1_DIMMABLE_LED:
    case HardwareCircuitType::L2_DIMMABLE_INCANDESCENT:
    case HardwareCircuitType::L4_DIMMABLE_PWM:
    case HardwareCircuitType::UNKNOWN:
    default:
      return DaikoLightType::DIMMABLE;
  }
}

void DaikoMeshLight::setup() {
  if (this->parent_ == nullptr) {
    return;
  }
  if (this->type_ == DaikoLightType::MASTER) {
    this->parent_->register_master_callback([this](bool is_on) {
      this->on_master_state_(is_on);
    });
  } else {
    this->parent_->register_circuit_callback(
        [this](uint8_t circuit, bool is_on, uint8_t dim_pct, uint8_t colortemp_pct) {
          if (circuit == this->circuit_) {
            this->on_circuit_state_(is_on, dim_pct, colortemp_pct);
          }
        });
  }
}

light::LightTraits DaikoMeshLight::get_traits() {
  light::LightTraits traits;
  const DaikoLightType effective_type = this->resolve_effective_type_();
  switch (effective_type) {
    case DaikoLightType::MASTER:
    case DaikoLightType::ON_OFF:
      traits.set_supported_color_modes({light::ColorMode::ON_OFF});
      break;
    case DaikoLightType::TUNABLE_WHITE:
      traits.set_supported_color_modes({light::ColorMode::COLOR_TEMPERATURE});
      traits.set_min_mireds(this->min_mireds_);
      traits.set_max_mireds(this->max_mireds_);
      break;
    case DaikoLightType::DIMMABLE:
    case DaikoLightType::AUTO:
    default:
      traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
      break;
  }
  return traits;
}

void DaikoMeshLight::write_state(light::LightState *state) {
  if (this->initial_run_) {
    this->initial_run_ = false;
    return;
  }
  if (this->updating_from_remote_ || this->parent_ == nullptr || state == nullptr) {
    return;
  }

  const uint32_t now = millis();
  this->last_user_action_ms_ = now;
  const bool is_on = state->current_values.is_on();

  if (this->type_ == DaikoLightType::MASTER) {
    this->current_is_on_ = is_on;
    this->last_tx_ms_ = now;
    this->parent_->set_master_power(is_on);
    return;
  }

  const DaikoLightType effective_type = this->resolve_effective_type_();
  const std::string timer_name = "light_tx_" + std::to_string(this->circuit_);

  if (!is_on) {
    this->cancel_timeout(timer_name.c_str());
    this->pending_tx_ = false;
    this->current_is_on_ = false;
    this->current_dim_ = 0;
    this->last_tx_ms_ = now;
    if (effective_type == DaikoLightType::ON_OFF) {
      this->parent_->set_circuit_onoff(this->circuit_, false);
    }
    this->parent_->turn_off_circuit(this->circuit_);
    return;
  }

  uint8_t dim = 100;
  uint8_t col = this->current_col_;

  if (effective_type != DaikoLightType::ON_OFF) {
    const float brightness = state->current_values.get_brightness();
    dim = static_cast<uint8_t>(std::lround(brightness * 100.0f));
    if (dim == 0) {
      const uint8_t retained = this->parent_->get_circuit_state(this->circuit_).retained_dim;
      dim = (retained > 0) ? retained : 100;
    }
  }

  if (effective_type == DaikoLightType::TUNABLE_WHITE) {
    const float mireds = state->current_values.get_color_temperature();
    float ct_ratio = 1.0f;
    if (this->max_mireds_ > this->min_mireds_) {
      ct_ratio = (this->max_mireds_ - mireds) / (this->max_mireds_ - this->min_mireds_);
    }
    ct_ratio = std::clamp(ct_ratio, 0.0f, 1.0f);
    col = static_cast<uint8_t>(std::lround(ct_ratio * 100.0f));
  }

  this->pending_is_on_ = true;
  this->pending_dim_ = dim;
  this->pending_col_ = col;
  this->pending_tx_ = true;

  // Execute immediately on OFF -> ON transition or when 330ms throttle window has elapsed;
  // otherwise schedule trailing execution at the end of the 330ms window.
  const uint32_t elapsed = now - this->last_tx_ms_;
  if (!this->current_is_on_ || !this->parent_->is_master_power_on() ||
      elapsed >= THROTTLE_INTERVAL_MS) {
    this->cancel_timeout(timer_name.c_str());
    this->flush_pending_command_();
  } else {
    this->set_timeout(timer_name.c_str(), THROTTLE_INTERVAL_MS - elapsed,
                      [this]() { this->flush_pending_command_(); });
  }
}

void DaikoMeshLight::flush_pending_command_() {
  if (!this->pending_tx_ || this->parent_ == nullptr) {
    return;
  }
  this->pending_tx_ = false;
  this->last_tx_ms_ = millis();

  const DaikoLightType effective_type = this->resolve_effective_type_();
  const bool was_on = this->current_is_on_;
  const uint8_t prev_dim = this->current_dim_;
  const uint8_t prev_col = this->current_col_;

  this->current_is_on_ = this->pending_is_on_;
  this->current_dim_ = this->pending_dim_;
  this->current_col_ = this->pending_col_;

  const int col_arg =
      (effective_type == DaikoLightType::TUNABLE_WHITE) ? static_cast<int>(this->pending_col_) : -1;

  if (!this->parent_->is_master_power_on() || !was_on) {
    this->parent_->turn_on_circuit(this->circuit_, this->pending_dim_, col_arg);
    return;
  }

  // Already ON: transmit only changed attributes
  const bool dim_changed = (effective_type != DaikoLightType::ON_OFF) && (this->pending_dim_ != prev_dim);
  const bool col_changed =
      (effective_type == DaikoLightType::TUNABLE_WHITE) && (this->pending_col_ != prev_col);

  if (dim_changed && col_changed) {
    this->parent_->set_circuit_dimming(this->circuit_, this->pending_dim_);
    std::string t_col = "light_col_" + std::to_string(this->circuit_);
    this->set_timeout(t_col.c_str(), 60, [this]() {
      this->parent_->set_circuit_colortemp(this->circuit_, this->pending_col_);
    });
  } else if (dim_changed) {
    this->parent_->set_circuit_dimming(this->circuit_, this->pending_dim_);
  } else if (col_changed) {
    this->parent_->set_circuit_colortemp(this->circuit_, this->pending_col_);
  }
}

void DaikoMeshLight::on_master_state_(bool is_on) {
  if (this->state_ == nullptr) {
    return;
  }

  this->initial_run_ = false;

  if (millis() - this->last_user_action_ms_ < USER_COOLDOWN_MS) {
    ESP_LOGD(TAG, "Master Power light: Suppressing remote state update during user cooldown");
    return;
  }

  this->current_is_on_ = is_on;
  this->updating_from_remote_ = true;
  this->state_->current_values.set_color_mode(light::ColorMode::ON_OFF);
  this->state_->remote_values.set_color_mode(light::ColorMode::ON_OFF);
  this->state_->current_values.set_state(is_on);
  this->state_->remote_values.set_state(is_on);
  this->state_->publish_state();
  this->updating_from_remote_ = false;
}

void DaikoMeshLight::on_circuit_state_(bool is_on, uint8_t dim_pct, uint8_t colortemp_pct) {
  if (this->state_ == nullptr) {
    return;
  }

  this->initial_run_ = false;

  if (this->pending_tx_ || (millis() - this->last_user_action_ms_ < USER_COOLDOWN_MS)) {
    ESP_LOGD(TAG, "Circuit %u: Suppressing remote state update during user cooldown",
             this->circuit_);
    return;
  }

  const DaikoLightType effective_type = this->resolve_effective_type_();
  light::ColorMode active_mode = light::ColorMode::BRIGHTNESS;
  if (effective_type == DaikoLightType::ON_OFF) {
    active_mode = light::ColorMode::ON_OFF;
  } else if (effective_type == DaikoLightType::TUNABLE_WHITE) {
    active_mode = light::ColorMode::COLOR_TEMPERATURE;
  }

  this->current_is_on_ = is_on;
  this->current_dim_ = is_on ? dim_pct : 0;
  this->current_col_ = colortemp_pct;

  this->updating_from_remote_ = true;
  this->state_->current_values.set_color_mode(active_mode);
  this->state_->remote_values.set_color_mode(active_mode);
  this->state_->current_values.set_state(is_on);
  this->state_->remote_values.set_state(is_on);

  if (is_on) {
    if (effective_type != DaikoLightType::ON_OFF && dim_pct > 0) {
      const float b = static_cast<float>(dim_pct) / 100.0f;
      this->state_->current_values.set_brightness(b);
      this->state_->remote_values.set_brightness(b);
    }
    if (effective_type == DaikoLightType::TUNABLE_WHITE) {
      const float mireds = this->max_mireds_ - (static_cast<float>(colortemp_pct) / 100.0f) *
                                                   (this->max_mireds_ - this->min_mireds_);
      this->state_->current_values.set_color_temperature(mireds);
      this->state_->remote_values.set_color_temperature(mireds);
    }
  }

  this->state_->publish_state();
  this->updating_from_remote_ = false;
}

void DaikoMeshLight::dump_config() {
  if (this->type_ == DaikoLightType::MASTER) {
    ESP_LOGCONFIG(TAG, "DAIKO Scene Controller BLE Light:");
    ESP_LOGCONFIG(TAG, "  Type: master (on_off)");
    return;
  }

  const DaikoLightType effective_type = this->resolve_effective_type_();
  const char *eff_str = "dimmable";
  if (effective_type == DaikoLightType::ON_OFF) {
    eff_str = "on_off";
  } else if (effective_type == DaikoLightType::TUNABLE_WHITE) {
    eff_str = "tunable_white";
  }

  const uint8_t hw_code = (this->parent_ != nullptr)
                              ? static_cast<uint8_t>(this->parent_->get_circuit_hw_type(this->circuit_))
                              : 0;
  ESP_LOGCONFIG(TAG, "DAIKO Scene Controller BLE Light:");
  ESP_LOGCONFIG(TAG, "  Circuit: %u", this->circuit_);
  ESP_LOGCONFIG(TAG, "  Mode: %s (Effective: %s, HW Type: 0x%02X)",
                this->type_ == DaikoLightType::AUTO ? "auto" : eff_str, eff_str, hw_code);
}

}  // namespace daiko_scenecontroller_ble
}  // namespace esphome
