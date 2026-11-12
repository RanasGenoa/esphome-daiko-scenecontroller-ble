#pragma once

#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_traits.h"
#include "esphome/core/component.h"
#include "daiko_scenecontroller_ble.h"

namespace esphome {
namespace daiko_scenecontroller_ble {

enum class DaikoLightType : uint8_t {
  AUTO = 0,
  DIMMABLE = 1,
  TUNABLE_WHITE = 2,
  ON_OFF = 3,
  MASTER = 4,
};

class DaikoMeshLight : public light::LightOutput, public Component {
 public:
  void set_parent(DaikoMeshComponent *parent) { this->parent_ = parent; }
  void set_circuit(uint8_t circuit) { this->circuit_ = circuit; }
  void set_light_type(DaikoLightType type) { this->type_ = type; }
  void set_min_mireds(float min_mireds) { this->min_mireds_ = min_mireds; }
  void set_max_mireds(float max_mireds) { this->max_mireds_ = max_mireds; }

  void setup_state(light::LightState *state) override {
    this->state_ = state;
    if (this->state_ != nullptr) {
      this->state_->set_default_transition_length(0);
    }
  }

  void setup() override;
  void dump_config() override;

  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;

 protected:
  static constexpr uint32_t THROTTLE_INTERVAL_MS = 330;  // Minimum interval between consecutive dimming updates
  static constexpr uint32_t USER_COOLDOWN_MS = 3500;     // Snapback guard window during hardware fade

  DaikoLightType resolve_effective_type_() const;
  void on_circuit_state_(bool is_on, uint8_t dim_pct, uint8_t colortemp_pct);
  void on_master_state_(bool is_on);
  void flush_pending_command_();

  DaikoMeshComponent *parent_{nullptr};
  light::LightState *state_{nullptr};
  uint8_t circuit_{1};
  DaikoLightType type_{DaikoLightType::AUTO};
  float min_mireds_{200.0f};  // 5000K (Cool white)
  float max_mireds_{370.0f};  // 2700K (Warm white)

  bool updating_from_remote_{false};
  bool initial_run_{true};

  // Rate-limiting & differential state tracking
  uint32_t last_user_action_ms_{0};
  uint32_t last_tx_ms_{0};
  bool pending_tx_{false};
  bool pending_is_on_{false};
  uint8_t pending_dim_{100};
  uint8_t pending_col_{50};

  bool current_is_on_{false};
  uint8_t current_dim_{100};
  uint8_t current_col_{50};
};

}  // namespace daiko_scenecontroller_ble
}  // namespace esphome
