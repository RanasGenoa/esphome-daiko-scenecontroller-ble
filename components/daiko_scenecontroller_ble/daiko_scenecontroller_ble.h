#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#ifdef USE_ESP32
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_local_data_operation_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"

extern "C" int bt_mesh_provisioner_remove_node(const uint8_t uuid[16]);
#endif

namespace esphome {
namespace text_sensor {
class TextSensor;
}
namespace daiko_scenecontroller_ble {

// ============================================================================
// DAIKO BLE Mesh Vendor Protocol Constants (DP-41924 / Company ID: 0x0AF8)
// ============================================================================
static constexpr uint16_t DAIKO_COMPANY_ID = 0x0AF8;
static constexpr uint16_t DAIKO_MODEL_ID = 0x0000;
static constexpr uint32_t SEQ_MAX_THRESHOLD = 15000000;
static constexpr uint32_t MAX_VALID_IV_INDEX = 0x00100000;
static constexpr uint32_t DAIKO_IV_STATE_MAGIC = 0x44495631;        // "DIV1"
static constexpr uint32_t DAIKO_CIRCUIT_TYPES_MAGIC = 0x44435431;  // "DCT1"
static constexpr uint8_t MAX_AUTO_ROTATION_STREAK = 2;

struct PersistedIvState {
  uint32_t iv_index{0};
  uint8_t ivu_in_progress{0};
  uint8_t reserved[3]{0, 0, 0};
  uint32_t magic{0};
};

struct PersistedCircuitTypes {
  std::array<uint8_t, 6> types{};
  uint8_t reserved[2]{0, 0};
  uint32_t magic{0};
};

#ifdef USE_ESP32
static constexpr uint32_t DAIKO_OPCODE_GET = ESP_BLE_MESH_MODEL_OP_3(0xE1, DAIKO_COMPANY_ID);
static constexpr uint32_t DAIKO_OPCODE_SET_ACK = ESP_BLE_MESH_MODEL_OP_3(0xE2, DAIKO_COMPANY_ID);
static constexpr uint32_t DAIKO_OPCODE_SET_UNACK = ESP_BLE_MESH_MODEL_OP_3(0xE3, DAIKO_COMPANY_ID);
static constexpr uint32_t DAIKO_OPCODE_STATUS = ESP_BLE_MESH_MODEL_OP_3(0xE4, DAIKO_COMPANY_ID);
#else
static constexpr uint32_t DAIKO_OPCODE_GET = 0xE10AF8;
static constexpr uint32_t DAIKO_OPCODE_SET_ACK = 0xE20AF8;
static constexpr uint32_t DAIKO_OPCODE_SET_UNACK = 0xE30AF8;
static constexpr uint32_t DAIKO_OPCODE_STATUS = 0xE40AF8;
#endif

static constexpr size_t NUM_CIRCUITS = 6;
static constexpr size_t NUM_CHANNELS = 12;
static constexpr size_t NUM_SCENES = 6;
static constexpr uint8_t SCENE_CHANNEL_EXCLUDED = 0xFF;

static constexpr uint8_t HEADER_SCENE_CTRL = 0x51;
static constexpr uint8_t HEADER_GROUP_CMD = 0x02;
static constexpr uint8_t SUBCMD_SCENE_RECALL = 0x08;

/// Vendor Sub-Commands following header byte 0x51 (`[0x51, SubCommand, len_lo, len_hi, ...payload]`)
enum class SubCommand : uint8_t {
  // Status / Get commands (Opcode 0xE10AF8 -> Response 0xE40AF8)
  CTRL_INFO = 0x01,            // 101-byte full controller configuration & 6 scene presets
  LEVEL_INFO = 0x02,           // 16-byte 12-channel brightness/colortemp & circuit ON/OFF state
  WEEKLY_SCHEDULE_GET = 0x05,  // Weekly timer schedule query/status
  TIME_INFO_GET = 0x06,        // 11-byte RTC clock query/status
  DAILY_SCHEDULE_GET = 0x08,   // Daily timer schedule query/status
  INTERLOCK_INFO = 0x10,       // 11-byte sensor interlock query/status
  SETTING_STATUS = 0x11,       // 5-byte hardware setting mode status

  // Control commands - Unacknowledged (Opcode 0xE30AF8)
  CHANNEL_STEP = 0x33,         // Step channel level up/down (`[0x51, 0x33, 0x02, 0x00, ch, dir]`)
  MASTER_POWER = 0x34,         // Master power ON/OFF (`[0x51, 0x34, 0x01, 0x00, on_off]`)
  CIRCUIT_ONOFF = 0x35,        // Individual circuit ON/OFF (`[0x51, 0x35, 0x02, 0x00, circuit, on_off]`)
  SCHEDULE_MODE = 0x36,        // Schedule mode (`[0x51, 0x36, 0x01, 0x00, mode]`)
  CANCEL_SETTING_MODE = 0x37,  // Cancel hardware setting mode (`[0x51, 0x37, ...]`)
  CHANNEL_DIMMING = 0x51,      // Set channel level 0-100% (`[0x51, 0x51, 0x02, 0x00, ch, level]`)

  // Control commands - Acknowledged (Opcode 0xE20AF8 -> Response 0xE40AF8)
  SCENE_SAVE = 0x50,           // Save scene levels & fade time (`[0x51, 0x50, 0x0E, 0x00, scene, ch0..11, fade]`)
  TIME_INFO_SET = 0x52,        // Set RTC clock (`[0x51, 0x52, 0x07, 0x00, YY, MM, DD, WW, hh, mm, ss]`)
  DAILY_SCHEDULE_SET = 0x58,   // Set daily schedule
  WEEKLY_SCHEDULE_SET = 0x59,  // Set weekly schedule
};

/// Hardware circuit load setting (L1..L6) reported in `CtrlInfo` (`51 01`) bytes 9..14
enum class HardwareCircuitType : uint8_t {
  UNKNOWN = 0x00,
  L1_DIMMABLE_LED = 0x01,          // L1: LED調光タイプ (逆位相制御 / 単色調光)
  L2_DIMMABLE_INCANDESCENT = 0x02, // L2: 白熱灯調光タイプ (逆位相制御 / 単色調光)
  L3_ON_OFF = 0x03,                // L3: ON/OFFタイプ (非調光スイッチ)
  L4_DIMMABLE_PWM = 0x04,          // L4: PWM調光タイプ (PWM信号線式 / 単色調光)
  L5_TUNABLE_PWM = 0x05,           // L5: PWM調色調光タイプ (PWM信号線式 / 調光調色)
  L6_TUNABLE_PHASE = 0x06,         // L6: 信号線不要調色調光タイプ (逆位相制御 / 調光調色)

  // Aliases
  DIMMABLE_PHASE = L1_DIMMABLE_LED,
  DIMMABLE_PWM = L4_DIMMABLE_PWM,
  ON_OFF = L3_ON_OFF,
  TUNABLE_WHITE = L6_TUNABLE_PHASE,
};

/// Hardware schedule/timer mode reported in `CtrlInfo` (`51 01`) byte 6
enum class ScheduleMode : uint8_t {
  OFF = 0x00,
  DAILY = 0x01,
  WEEKLY = 0x02,
};

/// Standalone BLE Mesh Pairing / Provisioning State
enum class PairingState : uint8_t {
  IDLE = 0,                ///< Normal operation / standby
  SCANNING,                ///< Actively scanning for unprovisioned DP-41924 beacon (PB-ADV)
  PROVISIONING,            ///< In progress: PB-ADV link open & NetKey injection
  CONFIG_APPKEY_ADDING,    ///< In progress: Injecting AppKey to node via Config Client
  CONFIG_MODEL_BINDING,    ///< In progress: Binding AppKey to Vendor Model
  CONFIG_MODEL_SUB_ADDING, ///< In progress: Subscribing Group Address (0xC078) to Vendor Model
  SUCCESS,                 ///< Completed successfully: NVS synced, normal control active
  FAILED,                  ///< Failed or timed out
};

/// Per-circuit runtime state tracked by the hub
struct CircuitState {
  bool relay_on{true};         ///< Circuit switch/relay state (`!(raw_dim & 0x80)` in `51 02`, controlled by `51 35`)
  uint8_t current_dim{0};      ///< Current brightness level `0..100` (`raw_dim & 0x7F` in `51 02`)
  uint8_t retained_dim{100};   ///< Last non-zero brightness `1..100` for restoring ON state
  uint8_t colortemp{50};       ///< Color temperature percentage `0..100` (`0%`=2700K, `100%`=5000K)
  HardwareCircuitType hw_type{HardwareCircuitType::UNKNOWN};

  /// Returns true when the circuit is actively outputting power/light
  bool is_effectively_on(bool master_power) const {
    return master_power && this->relay_on && (this->current_dim > 0);
  }
};

/// Scene preset configuration parsed from `CtrlInfo` (`51 01`) bytes 23..100
struct ScenePreset {
  std::array<uint8_t, NUM_CHANNELS> levels{};
  uint8_t fade_time_sec{0};
  bool valid{false};

  ScenePreset() { this->levels.fill(SCENE_CHANNEL_EXCLUDED); }
};

/// Controller RTC clock information parsed from `CtrlInfo` (`51 01`) or `TimeInfo` (`51 06`)
struct RtcTimeInfo {
  uint8_t year{0};     ///< Year offset from 2000 (e.g., 26 = 2026)
  uint8_t month{1};    ///< 1..12
  uint8_t day{1};      ///< 1..31
  uint8_t weekday{0};  ///< 0..6
  uint8_t hour{0};     ///< 0..23
  uint8_t minute{0};   ///< 0..59
  uint8_t second{0};   ///< 0..59
  bool valid{false};
};

// Callback types for child entities
using CircuitStateCallback = std::function<void(uint8_t circuit, bool is_on, uint8_t dim_pct, uint8_t colortemp_pct)>;
using MasterStateCallback = std::function<void(bool is_on)>;

class DaikoMeshComponent : public PollingComponent {
 public:
  DaikoMeshComponent() = default;

  // Component lifecycle
  void setup() override;
  void loop() override {}
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  // Configuration setters (used by ESPHome code generation)
  void set_net_key(const std::string &net_key) { this->net_key_str_ = net_key; }
  void set_app_key(const std::string &app_key) { this->app_key_str_ = app_key; }
  void set_controller_address(uint16_t addr) { this->controller_addr_ = addr; }
  void set_group_address(uint16_t addr) { this->group_addr_ = addr; }
  void set_self_address(uint16_t addr) {
    this->self_addr_ = addr;
    this->base_self_addr_ = addr;
  }

  // Entity registration callbacks
  void register_circuit_callback(CircuitStateCallback cb) {
    this->circuit_callbacks_.push_back(std::move(cb));
  }
  void register_master_callback(MasterStateCallback cb) {
    this->master_callbacks_.push_back(std::move(cb));
  }

  // ==========================================================================
  // High-Level Control API (for Light, Switch, and Button entities)
  // ==========================================================================

  /// Turn on a specific circuit (1..6). Handles Exclusive ON automatically if Master Power is OFF.
  void turn_on_circuit(uint8_t circuit, int brightness_pct = -1, int colortemp_pct = -1);

  /// Turn off a specific circuit (1..6) preserving retained brightness.
  void turn_off_circuit(uint8_t circuit);

  /// Set individual circuit ON/OFF state (`51 35`). `circuit` is 1..6.
  void set_circuit_onoff(uint8_t circuit, bool on);

  /// Set individual circuit brightness `0..100%` (`51 51` even channel). `circuit` is 1..6.
  void set_circuit_dimming(uint8_t circuit, uint8_t percentage);

  /// Set individual circuit color temperature `0..100%` (`51 51` odd channel). `circuit` is 1..6.
  void set_circuit_colortemp(uint8_t circuit, uint8_t percentage);

  /// Step channel level up (`up=true`) or down (`up=false`) (`51 33`). `channel` is 0..11.
  void step_channel_level(uint8_t channel, bool up);

  /// Set Master Power ON/OFF (`51 34` to group address).
  void set_master_power(bool on);
  void all_on() { this->set_master_power(true); }
  void all_off() { this->set_master_power(false); }

  /// Recall Scene `1..6` (`02 08` to group address, waking Master Power first if needed).
  void recall_scene(uint8_t scene_id);

  /// Advance sequence number if replay protection is suspected.
  void rotate_self_address();

  /// Save current (or specified) 12-channel levels and fade time into Scene `1..6` (`51 50`).
  void save_scene(uint8_t scene_id, const std::array<uint8_t, NUM_CHANNELS> &levels, uint8_t fade_time_sec = 4);

  /// Set hardware schedule/timer mode (`51 36`).
  void set_schedule_mode(ScheduleMode mode);

  /// Set controller RTC time (`51 52`).
  void set_controller_time(const RtcTimeInfo &time_info);

  /// Set auto-recovery timeout in milliseconds (0 = disabled).
  void set_auto_recovery_timeout(uint32_t ms) { this->auto_recovery_timeout_ms_ = ms; }

  /// Force reset sequence number to 2000 on current address and restart.
  void reset_sequence_number();

  // ==========================================================================
  // Standalone Provisioning & Pairing API
  // ==========================================================================

  /// Start standalone pairing / provisioning workflow (timeout in seconds, default 60s).
  void start_pairing(uint32_t timeout_sec = 60);

  /// Cancel pairing workflow.
  void cancel_pairing();

  /// Get current pairing state and description.
  PairingState get_pairing_state() const { return this->pairing_state_; }
  std::string get_pairing_state_str() const;

  // ==========================================================================
  // Low-Level Direct BLE Mesh Transmission
  // ==========================================================================
  bool send_vendor_packet(uint16_t dst, uint32_t opcode, const uint8_t *data, size_t len);

  // ==========================================================================
  // Status Queries
  // ==========================================================================

  /// Query 16-byte 12-channel level status (`51 02`).
  void query_status();

  /// Query 101-byte controller configuration & scene presets (`51 01`).
  void query_ctrl_info();

  /// Query 11-byte controller RTC clock (`51 06`).
  void query_time_info();

  /// Query 5-byte hardware setting mode status (`51 11`).
  void query_setting_status();

  // State accessors
  bool is_master_power_on() const { return this->master_power_; }
  uint8_t get_active_scene() const { return this->active_scene_; }
  uint16_t get_self_address() const { return this->self_addr_; }
  uint32_t get_sequence_number() const;
  bool is_connected() const { return this->is_connected_; }
  uint32_t get_missed_responses() const { return this->missed_responses_; }

  const CircuitState &get_circuit_state(uint8_t circuit) const {
    uint8_t idx = (circuit >= 1 && circuit <= NUM_CIRCUITS) ? (circuit - 1) : 0;
    return this->circuit_states_[idx];
  }
  HardwareCircuitType get_circuit_hw_type(uint8_t circuit) {
    this->ensure_circuit_types_loaded_();
    uint8_t idx = (circuit >= 1 && circuit <= NUM_CIRCUITS) ? (circuit - 1) : 0;
    return this->circuit_states_[idx].hw_type;
  }
  bool has_discovered_circuit_types() {
    this->ensure_circuit_types_loaded_();
    return this->has_persisted_circuit_types_ || this->has_received_ctrl_info_;
  }
  const ScenePreset &get_scene_preset(uint8_t scene_id) const {
    uint8_t idx = (scene_id >= 1 && scene_id <= NUM_SCENES) ? (scene_id - 1) : 0;
    return this->scene_presets_[idx];
  }

  // Maintenance & Monitoring Observers
  using AddressCallback = std::function<void(uint16_t)>;
  using SequenceCallback = std::function<void(uint32_t)>;
  using ConnectivityCallback = std::function<void(bool)>;
  using MissedCallback = std::function<void(uint32_t)>;
  using ActiveSceneCallback = std::function<void(uint8_t)>;
  using PairingStateCallback = std::function<void(PairingState, const std::string &)>;

  void register_address_callback(AddressCallback callback) {
    this->address_callbacks_.push_back(std::move(callback));
  }
  void register_sequence_callback(SequenceCallback callback) {
    this->sequence_callbacks_.push_back(std::move(callback));
  }
  void register_connectivity_callback(ConnectivityCallback callback) {
    this->connectivity_callbacks_.push_back(std::move(callback));
  }
  void register_missed_callback(MissedCallback callback) {
    this->missed_callbacks_.push_back(std::move(callback));
  }
  void register_active_scene_callback(ActiveSceneCallback callback) {
    this->active_scene_callbacks_.push_back(std::move(callback));
  }
  void register_pairing_state_callback(PairingStateCallback callback) {
    this->pairing_state_callbacks_.push_back(std::move(callback));
  }
  void set_pairing_state_text_sensor(text_sensor::TextSensor *s) {
    this->text_sensor_pairing_state_ = s;
  }

#ifdef USE_ESP32
  // BLE Mesh stack callbacks
  static void provisioning_callback(esp_ble_mesh_prov_cb_event_t event,
                                    esp_ble_mesh_prov_cb_param_t *param);
  static void config_client_callback(esp_ble_mesh_cfg_client_cb_event_t event,
                                     esp_ble_mesh_cfg_client_cb_param_t *param);
  static void custom_model_callback(esp_ble_mesh_model_cb_event_t event,
                                    esp_ble_mesh_model_cb_param_t *param);
#endif

 protected:
  // Internal helpers
  bool parse_hex_key_(const std::string &hex, uint8_t *out, size_t len);
  void ensure_circuit_types_loaded_();
  void init_bluetooth_();
  void init_ble_mesh_();
  void sync_iv_state_(bool from_tx);
  void rotate_self_address_(bool auto_triggered);

  void parse_vendor_status_(uint16_t src, const uint8_t *data, uint16_t len);
  void parse_ctrl_info_(const uint8_t *data, uint16_t len);
  void parse_level_info_(const uint8_t *data, uint16_t len);
  void parse_time_info_(const uint8_t *data, uint16_t len);
  void parse_setting_status_(const uint8_t *data, uint16_t len);
  void notify_circuit_state_(uint8_t circuit);
  void notify_master_state_();
  void publish_state_to_entities_();
  void publish_maintenance_sensors_();
  void publish_connectivity_state_();

  // Standalone Provisioning helpers
  void set_pairing_state_(PairingState state, const std::string &message = "");
  void send_config_app_key_add_(uint16_t node_addr);
  void send_config_model_app_bind_(uint16_t node_addr);
  void send_config_model_sub_add_(uint16_t node_addr);
  void on_pairing_completed_();
  void on_pairing_failed_(const std::string &reason);
  void restore_controller_node_();

  // Configuration
  std::string net_key_str_;
  std::string app_key_str_;
  std::array<uint8_t, 16> net_key_{};
  std::array<uint8_t, 16> app_key_{};

  uint16_t controller_addr_{0x0004};
  uint16_t group_addr_{0xC078};
  uint16_t self_addr_{0x0020};
  uint16_t base_self_addr_{0x0020};
  uint16_t net_idx_{0x0000};
  uint16_t app_idx_{0x0000};

  ESPPreferenceObject rtc_seq_;
  ESPPreferenceObject rtc_self_addr_;
  ESPPreferenceObject rtc_iv_state_;
  ESPPreferenceObject rtc_circuit_types_;
  ESPPreferenceObject rtc_rot_streak_;
  ESPPreferenceObject rtc_addr_migrated_v2_;
  ESPPreferenceObject rtc_wrap_epoch_;

  // Runtime state
  bool mesh_initialized_{false};
  bool node_ready_{false};
  bool has_received_ctrl_info_{false};
  bool circuit_types_loaded_{false};
  bool has_persisted_circuit_types_{false};
  std::array<uint8_t, NUM_CIRCUITS> persisted_circuit_types_{};
  uint32_t persisted_iv_index_{0};
  bool persisted_ivu_in_progress_{false};
  uint8_t auto_rotation_streak_{0};
  uint8_t wrap_epoch_{0};

  // Standalone Provisioning state
  PairingState pairing_state_{PairingState::IDLE};
  uint16_t provisioning_target_addr_{0x0004};
  uint32_t pairing_started_ms_{0};

  bool master_power_{true};
  uint8_t active_scene_{0};  ///< Last recalled scene index (0..5)
  ScheduleMode schedule_mode_{ScheduleMode::OFF};
  uint8_t daily_schedule_index_{0};
  uint8_t weekly_schedule_mode_{0};
  uint8_t setting_status_{0};

  // Health and maintenance tracking
  uint32_t auto_recovery_timeout_ms_{90000};  ///< Default 90s auto-recovery timeout
  uint32_t boot_ms_{0};
  uint32_t last_rx_ms_{0};
  uint32_t missed_responses_{0};
  bool is_connected_{false};
  bool initial_unresponsive_logged_{false};
  bool auto_recovery_paused_logged_{false};
  bool seq_warn_80_logged_{false};
  bool seq_warn_90_logged_{false};

  std::array<CircuitState, NUM_CIRCUITS> circuit_states_{};
  std::array<ScenePreset, NUM_SCENES> scene_presets_{};
  RtcTimeInfo rtc_time_{};

  // Registered entity callbacks
  std::vector<CircuitStateCallback> circuit_callbacks_;
  std::vector<MasterStateCallback> master_callbacks_;
  std::vector<AddressCallback> address_callbacks_;
  std::vector<SequenceCallback> sequence_callbacks_;
  std::vector<ConnectivityCallback> connectivity_callbacks_;
  std::vector<MissedCallback> missed_callbacks_;
  std::vector<ActiveSceneCallback> active_scene_callbacks_;
  std::vector<PairingStateCallback> pairing_state_callbacks_;
  text_sensor::TextSensor *text_sensor_pairing_state_{nullptr};

  static DaikoMeshComponent *instance_;
};

}  // namespace daiko_scenecontroller_ble
}  // namespace esphome
