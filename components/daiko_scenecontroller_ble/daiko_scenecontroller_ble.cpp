#include "daiko_scenecontroller_ble.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef USE_ESP32
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

extern "C" {
extern struct bt_mesh_net bt_mesh;
int bt_mesh_provisioner_restore_node_info(struct bt_mesh_node *node);
int bt_mesh_provisioner_set_primary_elem_addr(uint16_t addr);
}
#endif

namespace esphome {
namespace daiko_scenecontroller_ble {

static const char *const TAG = "daiko_scenecontroller_ble";

DaikoMeshComponent *DaikoMeshComponent::instance_ = nullptr;

#ifdef USE_ESP32
// ============================================================================
// BLE Mesh Stack Static Definitions
// ============================================================================
static esp_ble_mesh_cfg_srv_t config_server = {
    .net_transmit = ESP_BLE_MESH_TRANSMIT(4, 20),
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(4, 20),
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
};

static esp_ble_mesh_client_t config_client;

static esp_ble_mesh_client_op_pair_t daiko_op_pair[] = {
    {DAIKO_OPCODE_GET, DAIKO_OPCODE_STATUS},
    {DAIKO_OPCODE_SET_ACK, DAIKO_OPCODE_STATUS},
    {DAIKO_OPCODE_SET_UNACK, DAIKO_OPCODE_STATUS},
};

static esp_ble_mesh_client_t daiko_vendor_client = {
    .op_pair_size = sizeof(daiko_op_pair) / sizeof(daiko_op_pair[0]),
    .op_pair = daiko_op_pair,
};

static esp_ble_mesh_model_op_t daiko_vendor_op[] = {
    ESP_BLE_MESH_MODEL_OP(DAIKO_OPCODE_STATUS, 0),
    ESP_BLE_MESH_MODEL_OP(DAIKO_OPCODE_SET_UNACK, 0),
    ESP_BLE_MESH_MODEL_OP(DAIKO_OPCODE_SET_ACK, 0),
    ESP_BLE_MESH_MODEL_OP(DAIKO_OPCODE_GET, 0),
    ESP_BLE_MESH_MODEL_OP_END,
};

static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_CFG_CLI(&config_client),
};

static esp_ble_mesh_model_t daiko_vendor_models[] = {
    ESP_BLE_MESH_VENDOR_MODEL(DAIKO_COMPANY_ID, DAIKO_MODEL_ID, daiko_vendor_op, nullptr,
                              &daiko_vendor_client),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, daiko_vendor_models),
};

static esp_ble_mesh_comp_t composition = {
    .cid = DAIKO_COMPANY_ID,
    .pid = 0,
    .vid = 0,
    .element_count = 1,
    .elements = elements,
};

static uint8_t dev_uuid[16] = {0xDA, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

static esp_ble_mesh_prov_t provision_config = {
    .prov_uuid = dev_uuid,
    .prov_unicast_addr = 0x0021,
    .prov_start_address = 0x0025,
    .prov_attention = 0x00,
    .prov_algorithm = 0x00,
    .prov_pub_key_oob = 0x00,
    .prov_static_oob_val = nullptr,
    .prov_static_oob_len = 0x00,
    .flags = 0x00,
    .iv_index = 0x00,
};

// ============================================================================
// BLE Mesh Callbacks
// ============================================================================
void DaikoMeshComponent::provisioning_callback(esp_ble_mesh_prov_cb_event_t event,
                                               esp_ble_mesh_prov_cb_param_t *param) {
  if (instance_ == nullptr) {
    return;
  }

  switch (event) {
    case ESP_BLE_MESH_PROVISIONER_RECV_UNPROV_ADV_PKT_EVT: {
      if (instance_->pairing_state_ != PairingState::SCANNING) {
        return;
      }
      char uuid_hex[33] = {0};
      for (size_t i = 0; i < 16; i++) {
        snprintf(uuid_hex + i * 2, 3, "%02X", param->provisioner_recv_unprov_adv_pkt.dev_uuid[i]);
      }
      ESP_LOGI(TAG, "[PAIR_SCAN] Discovered unprovisioned BLE Mesh device! UUID: %s, RSSI: %d",
               uuid_hex, param->provisioner_recv_unprov_adv_pkt.rssi);

      instance_->set_pairing_state_(PairingState::PROVISIONING, "Device discovered, provisioning...");

#ifdef USE_ESP32
      // Clear any existing provisioner node records to avoid duplicate address error
      bt_mesh_provisioner_remove_node(nullptr);
#endif

      // Start provisioning with target address fixed to controller_addr_ (0x0004)
      esp_err_t err = esp_ble_mesh_provisioner_prov_device_with_addr(
          param->provisioner_recv_unprov_adv_pkt.dev_uuid,
          param->provisioner_recv_unprov_adv_pkt.addr,
          param->provisioner_recv_unprov_adv_pkt.addr_type,
          param->provisioner_recv_unprov_adv_pkt.bearer,
          param->provisioner_recv_unprov_adv_pkt.oob_info,
          instance_->controller_addr_);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "[PAIR_ERR] Failed to start provisioning with addr 0x%04X: %d",
                 instance_->controller_addr_, err);
        instance_->on_pairing_failed_("Provisioning initiation failed");
      }
      break;
    }

    case ESP_BLE_MESH_PROVISIONER_PROV_LINK_OPEN_EVT:
      ESP_LOGI(TAG, "[PAIR] PB-ADV Provisioning Link opened with target device");
      break;

    case ESP_BLE_MESH_PROVISIONER_PROV_LINK_CLOSE_EVT:
      ESP_LOGI(TAG, "[PAIR] PB-ADV Provisioning Link closed (reason: %d)",
               param->provisioner_prov_link_close.reason);
      if (instance_->pairing_state_ == PairingState::PROVISIONING) {
        instance_->on_pairing_failed_("Link closed during provisioning");
      }
      break;

    case ESP_BLE_MESH_PROVISIONER_PROV_COMPLETE_EVT: {
      uint16_t unicast_addr = param->provisioner_prov_complete.unicast_addr;
      uint8_t elem_num = param->provisioner_prov_complete.element_num;
      ESP_LOGI(TAG, "[PAIR_PROV_OK] NetKey provisioning complete! Addr: 0x%04X, Elements: %u",
               unicast_addr, elem_num);

      // Reset sequence number to fresh safe margin 2000 because target DP-41924 was reset
      bt_mesh.seq = 2000;
      instance_->rtc_seq_.save(&bt_mesh.seq);

      // Reset self_address to base slot (0x0021)
      instance_->self_addr_ = instance_->base_self_addr_ + 1;
      instance_->rtc_self_addr_.save(&instance_->self_addr_);
#ifdef USE_ESP32
      bt_mesh_provisioner_set_primary_elem_addr(instance_->self_addr_);
      esp_ble_mesh_provisioner_bind_app_key_to_local_model(
          instance_->self_addr_, instance_->app_idx_, DAIKO_MODEL_ID, DAIKO_COMPANY_ID);
#endif

      // Reset IV Index state in NVS & runtime
      instance_->persisted_iv_index_ = 0;
      instance_->persisted_ivu_in_progress_ = false;
      PersistedIvState init_iv{0, 0, {0, 0, 0}, DAIKO_IV_STATE_MAGIC};
      instance_->rtc_iv_state_.save(&init_iv);

      instance_->auto_rotation_streak_ = 0;
      uint8_t zero_streak = 0;
      instance_->rtc_rot_streak_.save(&zero_streak);

      instance_->wrap_epoch_ = 0;
      uint8_t zero_epoch = 0;
      instance_->rtc_wrap_epoch_.save(&zero_epoch);

      global_preferences->sync();

      ESP_LOGI(TAG,
               "[PAIR_RESET] Initialized fresh SeqNum to 2000, IV Index to 0, and self_address to 0x%04X",
               instance_->self_addr_);

      // Proceed to Config Client: Add AppKey to controller node
      instance_->provisioning_target_addr_ = unicast_addr;
      instance_->set_pairing_state_(PairingState::CONFIG_APPKEY_ADDING, "Adding AppKey...");

      // Small delay before sending Config AppKey Add to allow link settling
      instance_->set_timeout("pair_appkey_add", 800, [unicast_addr]() {
        if (instance_ != nullptr && instance_->pairing_state_ == PairingState::CONFIG_APPKEY_ADDING) {
          instance_->send_config_app_key_add_(unicast_addr);
        }
      });
      break;
    }

    default:
      ESP_LOGD(TAG, "BLE Mesh Provisioning Event: %d", event);
      break;
  }
}

void DaikoMeshComponent::config_client_callback(esp_ble_mesh_cfg_client_cb_event_t event,
                                                esp_ble_mesh_cfg_client_cb_param_t *param) {
  if (instance_ == nullptr) {
    return;
  }

  ESP_LOGD(TAG, "BLE Mesh Config Client Event: %d, err_code: %d", event, param->error_code);

  if (event == ESP_BLE_MESH_CFG_CLIENT_TIMEOUT_EVT) {
    ESP_LOGW(TAG, "[PAIR_TIMEOUT] Config Client timeout (opcode: 0x%04X, addr: 0x%04X)",
             param->params ? param->params->opcode : 0,
             param->params ? param->params->ctx.addr : 0);
    if (instance_->pairing_state_ != PairingState::IDLE &&
        instance_->pairing_state_ != PairingState::SUCCESS) {
      instance_->on_pairing_failed_("Config message timeout");
    }
    return;
  }

  if (event == ESP_BLE_MESH_CFG_CLIENT_SET_STATE_EVT && param->params != nullptr) {
    uint32_t opcode = param->params->opcode;
    uint16_t addr = param->params->ctx.addr;

    switch (opcode) {
      case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD: {
        uint8_t status = param->status_cb.appkey_status.status;
        ESP_LOGI(TAG, "[PAIR_CFG] Config AppKey Add response from 0x%04X: status=%u (0=Success)",
                 addr, status);
        if (status == 0x00 || status == 0x02 /* Key already exists */) {
          instance_->set_pairing_state_(PairingState::CONFIG_MODEL_BINDING, "Binding Vendor Model...");
          instance_->set_timeout("pair_model_bind", 300, [addr]() {
            if (instance_ != nullptr && instance_->pairing_state_ == PairingState::CONFIG_MODEL_BINDING) {
              instance_->send_config_model_app_bind_(addr);
            }
          });
        } else {
          ESP_LOGE(TAG, "[PAIR_ERR] AppKey Add failed with status: %u", status);
          instance_->on_pairing_failed_("AppKey Add rejected by node");
        }
        break;
      }

      case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND: {
        uint8_t status = param->status_cb.model_app_status.status;
        ESP_LOGI(TAG, "[PAIR_CFG] Config Model App Bind response from 0x%04X: status=%u (0=Success)",
                 addr, status);
        if (status == 0x00 || status == 0x02 /* Already bound */) {
          instance_->set_pairing_state_(PairingState::CONFIG_MODEL_SUB_ADDING, "Subscribing Group 0xC078...");
          instance_->set_timeout("pair_sub_add", 300, [addr]() {
            if (instance_ != nullptr && instance_->pairing_state_ == PairingState::CONFIG_MODEL_SUB_ADDING) {
              instance_->send_config_model_sub_add_(addr);
            }
          });
        } else {
          ESP_LOGE(TAG, "[PAIR_ERR] Model App Bind failed with status: %u", status);
          instance_->on_pairing_failed_("Model App Bind rejected by node");
        }
        break;
      }

      case ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD: {
        uint8_t status = param->status_cb.model_sub_status.status;
        ESP_LOGI(TAG, "[PAIR_CFG] Config Model Sub Add response from 0x%04X: status=%u (0=Success)",
                 addr, status);
        if (status == 0x00 || status == 0x02 /* Already subscribed */) {
          ESP_LOGI(TAG, "[PAIR_COMPLETE] Node 0x%04X configured successfully! All bindings complete.", addr);
          instance_->on_pairing_completed_();
        } else {
          ESP_LOGE(TAG, "[PAIR_ERR] Model Sub Add failed with status: %u", status);
          instance_->on_pairing_failed_("Model Sub Add rejected by node");
        }
        break;
      }

      default:
        break;
    }
  }
}

void DaikoMeshComponent::custom_model_callback(esp_ble_mesh_model_cb_event_t event,
                                               esp_ble_mesh_model_cb_param_t *param) {
  if (instance_ == nullptr) {
    return;
  }

  auto dispatch_rx = [](const char *kind, esp_ble_mesh_msg_ctx_t *ctx, uint32_t opcode,
                        const uint8_t *data, uint16_t length) {
    if (ctx == nullptr) {
      return;
    }
    const uint16_t src = ctx->addr;
    if (src == instance_->self_addr_) {
      ESP_LOGD(TAG, "[RX_LOCAL] Ignoring loopback %s from self 0x%04X, op=0x%06X, len=%u", kind, src,
               static_cast<unsigned int>(opcode), length);
    } else {
      char hex[64] = {0};
      size_t print_len = length > 16 ? 16 : length;
      for (size_t i = 0; i < print_len; i++) {
        snprintf(hex + i * 3, sizeof(hex) - i * 3, "%02X ", data[i]);
      }
      ESP_LOGI(TAG, "[RX] %s from 0x%04X, op=0x%06X, len=%u: %s%s", kind, src,
               static_cast<unsigned int>(opcode), length, hex, length > 16 ? "..." : "");
    }
    instance_->parse_vendor_status_(src, data, length);
  };

  switch (event) {
    case ESP_BLE_MESH_MODEL_OPERATION_EVT:
      dispatch_rx("Op", param->model_operation.ctx, param->model_operation.opcode,
                  param->model_operation.msg, param->model_operation.length);
      break;

    case ESP_BLE_MESH_CLIENT_MODEL_RECV_PUBLISH_MSG_EVT:
      dispatch_rx("Pub", param->client_recv_publish_msg.ctx, param->client_recv_publish_msg.opcode,
                  param->client_recv_publish_msg.msg, param->client_recv_publish_msg.length);
      break;

    case ESP_BLE_MESH_MODEL_SEND_COMP_EVT:
      if (param->model_send_comp.err_code == ESP_OK) {
        ESP_LOGD(TAG, "[TX] Send Comp: op=0x%06X OK",
                 static_cast<unsigned int>(param->model_send_comp.opcode));
      } else {
        ESP_LOGW(TAG, "[TX_FAIL] Send Comp: op=0x%06X failed (err=%d)",
                 static_cast<unsigned int>(param->model_send_comp.opcode),
                 param->model_send_comp.err_code);
      }
      break;

    case ESP_BLE_MESH_CLIENT_MODEL_SEND_TIMEOUT_EVT:
      ESP_LOGW(TAG, "[TX_TIMEOUT] Ack timeout for op=0x%06X (dst=0x%04X)",
               static_cast<unsigned int>(param->client_send_timeout.opcode),
               param->client_send_timeout.ctx->addr);
      break;

    default:
      break;
  }
}
#endif

// ============================================================================
// Component Lifecycle
// ============================================================================
bool DaikoMeshComponent::parse_hex_key_(const std::string &hex, uint8_t *out, size_t len) {
  if (hex.length() != len * 2) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    char byte_str[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
    char *endptr = nullptr;
    unsigned long val = std::strtoul(byte_str, &endptr, 16);
    if (endptr == byte_str || *endptr != '\0') {
      return false;
    }
    out[i] = static_cast<uint8_t>(val);
  }
  return true;
}

void DaikoMeshComponent::ensure_circuit_types_loaded_() {
  if (this->circuit_types_loaded_) {
    return;
  }
  this->circuit_types_loaded_ = true;
#ifdef USE_ESP32
  if (global_preferences == nullptr) {
    return;
  }
  this->rtc_circuit_types_ =
      global_preferences->make_preference<PersistedCircuitTypes>(fnv1_hash("daiko_ctype_v1"));
  PersistedCircuitTypes saved{};
  if (this->rtc_circuit_types_.load(&saved) && saved.magic == DAIKO_CIRCUIT_TYPES_MAGIC) {
    this->has_persisted_circuit_types_ = true;
    this->persisted_circuit_types_ = saved.types;
    for (size_t i = 0; i < NUM_CIRCUITS; i++) {
      this->circuit_states_[i].hw_type = static_cast<HardwareCircuitType>(saved.types[i]);
    }
    ESP_LOGI(TAG,
             "[CTYPE_BOOT] Restored hardware circuit types from NVS: C1=0x%02X C2=0x%02X C3=0x%02X "
             "C4=0x%02X C5=0x%02X C6=0x%02X",
             saved.types[0], saved.types[1], saved.types[2], saved.types[3], saved.types[4],
             saved.types[5]);
  }
#endif
}

void DaikoMeshComponent::setup() {
  instance_ = this;
  ESP_LOGI(TAG, "Initializing DAIKO BLE Mesh component...");

  this->ensure_circuit_types_loaded_();

  if (!this->parse_hex_key_(this->net_key_str_, this->net_key_.data(), this->net_key_.size()) ||
      !this->parse_hex_key_(this->app_key_str_, this->app_key_.data(), this->app_key_.size())) {
    ESP_LOGE(TAG, "[INIT_ERR] Invalid net_key or app_key format (expected 32 hex characters)");
    this->mark_failed();
    return;
  }

#ifdef USE_ESP32
  // Clear any corrupted ble_mesh NVS state (such as corrupted IV Index)
  nvs_handle_t mesh_nvs_handle;
  if (nvs_open("ble_mesh", NVS_READWRITE, &mesh_nvs_handle) == ESP_OK) {
    nvs_erase_all(mesh_nvs_handle);
    nvs_commit(mesh_nvs_handle);
    nvs_close(mesh_nvs_handle);
    ESP_LOGI(TAG, "[INIT] Purged ble_mesh NVS namespace to eliminate corrupted IV Index");
  }

  // 1. Restore or migrate self_address and maintenance state from NVS
  this->rtc_self_addr_ =
      global_preferences->make_preference<uint16_t>(fnv1_hash("daiko_self_addr_v1"));
  this->rtc_addr_migrated_v2_ =
      global_preferences->make_preference<uint8_t>(fnv1_hash("daiko_addr_v2_m"));
  this->rtc_rot_streak_ =
      global_preferences->make_preference<uint8_t>(fnv1_hash("daiko_rot_streak_v1"));
  this->rtc_wrap_epoch_ =
      global_preferences->make_preference<uint8_t>(fnv1_hash("daiko_wrap_epoch_v1"));
  this->rtc_iv_state_ =
      global_preferences->make_preference<PersistedIvState>(fnv1_hash("daiko_iv_state_v1"));

  uint8_t saved_streak = 0;
  if (this->rtc_rot_streak_.load(&saved_streak)) {
    this->auto_rotation_streak_ = saved_streak;
  }
  uint8_t saved_epoch = 0;
  if (this->rtc_wrap_epoch_.load(&saved_epoch) && saved_epoch <= 3) {
    this->wrap_epoch_ = saved_epoch;
  }

  PersistedIvState saved_iv{};
  if (this->rtc_iv_state_.load(&saved_iv) && saved_iv.magic == DAIKO_IV_STATE_MAGIC &&
      saved_iv.iv_index <= MAX_VALID_IV_INDEX && saved_iv.ivu_in_progress <= 1) {
    if (saved_iv.iv_index == 0 && saved_iv.ivu_in_progress != 0) {
      ESP_LOGW(TAG, "[IV_BOOT] Ignoring invalid persisted state (iv=0, ivu=1); resetting to iv=0");
    } else {
      this->persisted_iv_index_ = saved_iv.iv_index;
      this->persisted_ivu_in_progress_ = (saved_iv.ivu_in_progress != 0);
    }
  }

  uint8_t migrated_v2 = 0;
  const bool has_migrated_v2 =
      this->rtc_addr_migrated_v2_.load(&migrated_v2) && (migrated_v2 == 1);

  uint16_t saved_addr = 0;
  if (this->rtc_self_addr_.load(&saved_addr) && saved_addr >= this->base_self_addr_ &&
      saved_addr <= (this->base_self_addr_ + 15)) {
    // Perform legacy 0x0020 -> 0x0021 migration once; later wrap-arounds to base_self_addr_ stay intact.
    if (saved_addr == this->base_self_addr_ && !has_migrated_v2) {
      this->self_addr_ = this->base_self_addr_ + 1;
      this->rtc_self_addr_.save(&this->self_addr_);
      migrated_v2 = 1;
      this->rtc_addr_migrated_v2_.save(&migrated_v2);
      global_preferences->sync();
      ESP_LOGI(TAG, "[ADDR_BOOT] Migrated legacy self_address from 0x%04X to 0x%04X",
               saved_addr, this->self_addr_);
    } else {
      this->self_addr_ = saved_addr;
      if (!has_migrated_v2) {
        migrated_v2 = 1;
        this->rtc_addr_migrated_v2_.save(&migrated_v2);
        global_preferences->sync();
      }
      ESP_LOGI(TAG, "[ADDR_BOOT] Restored self_address from NVS: 0x%04X (base: 0x%04X, slot %u/16)",
               this->self_addr_, this->base_self_addr_, (this->self_addr_ - this->base_self_addr_ + 1));
    }
  } else {
    // Fresh start: initialize to base_self_addr_ + 1 (0x0021)
    this->self_addr_ = this->base_self_addr_ + 1;
    this->rtc_self_addr_.save(&this->self_addr_);
    migrated_v2 = 1;
    this->rtc_addr_migrated_v2_.save(&migrated_v2);
    global_preferences->sync();
    ESP_LOGI(TAG, "[ADDR_BOOT] Initialized self_address to fresh slot: 0x%04X (base: 0x%04X)",
             this->self_addr_, this->base_self_addr_);
  }

  // 2. Initialize Bluetooth & BLE Mesh Provisioner
  this->init_bluetooth_();
  this->init_ble_mesh_();

  if (!this->mesh_initialized_) {
    this->mark_failed();
    return;
  }

  this->boot_ms_ = millis();

  // 3. Restore BLE Mesh Sequence Number from NVS and advance safely
  this->rtc_seq_ = global_preferences->make_preference<uint32_t>(fnv1_hash("daiko_seq_num_v1"));
  uint32_t saved_seq = 0;
  if (this->self_addr_ != saved_addr) {
    // Slot migrated or fresh boot: start from safe margin 2000
    bt_mesh.seq = 2000;
  } else if (this->rtc_seq_.load(&saved_seq) && saved_seq < SEQ_MAX_THRESHOLD) {
    bt_mesh.seq = (saved_seq >= 2000) ? (saved_seq + 2000) : 2000;
  } else {
    bt_mesh.seq = 2000;
  }
  if (!this->rtc_seq_.save(&bt_mesh.seq)) {
    ESP_LOGE(TAG, "[NVS_ERR] Failed to save initial sequence number %u to NVS", static_cast<unsigned int>(bt_mesh.seq));
  }
  global_preferences->sync();
  ESP_LOGI(TAG, "[SEQ_BOOT] BLE Mesh Sequence Number initialized to: %u (from saved=%u)",
           static_cast<unsigned int>(bt_mesh.seq), static_cast<unsigned int>(saved_seq));

  // 4. Initial synchronization: fetch CtrlInfo and LevelInfo
  this->set_timeout("boot_sync_ctrl", 2000, [this]() {
    this->query_ctrl_info();
  });
  this->set_timeout("boot_sync_level", 3500, [this]() {
    this->query_status();
  });

  this->publish_maintenance_sensors_();
  this->publish_connectivity_state_();
#endif
}

void DaikoMeshComponent::update() {
  if (!this->mesh_initialized_ || !this->node_ready_) {
    return;
  }

  this->sync_iv_state_(false);

  // Health check: verify controller responses (also covers no-RX-since-boot window)
  uint32_t now = millis();
  uint32_t silence_ms = (this->last_rx_ms_ > 0) ? (now - this->last_rx_ms_) : (now - this->boot_ms_);
  if (silence_ms > 15000) {
    this->missed_responses_++;
    if (this->is_connected_) {
      this->is_connected_ = false;
      ESP_LOGW(TAG, "[HEALTH] Controller 0x%04X is UNRESPONSIVE (silence for %u s, missed: %u)",
               this->controller_addr_, silence_ms / 1000, this->missed_responses_);
      this->publish_connectivity_state_();
    } else if (this->last_rx_ms_ == 0 && !this->initial_unresponsive_logged_) {
      this->initial_unresponsive_logged_ = true;
      ESP_LOGW(TAG,
               "[HEALTH] Controller 0x%04X has not responded since boot (%u s, self=0x%04X, iv=%u, ivu=%d, ivi_tx=%u, seq=%u)",
               this->controller_addr_, silence_ms / 1000, this->self_addr_,
               static_cast<unsigned int>(bt_mesh.iv_index),
               bt_mesh_atomic_test_bit(bt_mesh.flags, BLE_MESH_IVU_IN_PROGRESS),
               static_cast<unsigned int>(BLE_MESH_NET_IVI_TX),
               static_cast<unsigned int>(bt_mesh.seq));
    }

    // Auto-recovery: bounded rotation streak prevents 16-slot exhaustion storms during prolonged outages
    if (this->auto_recovery_timeout_ms_ > 0 && silence_ms >= this->auto_recovery_timeout_ms_) {
      if (this->auto_rotation_streak_ < MAX_AUTO_ROTATION_STREAK) {
        ESP_LOGE(TAG,
                 "[HEALTH] Controller 0x%04X unresponsive for %u s (>= %u ms threshold). "
                 "Triggering automatic address rotation (%u/%u)...",
                 this->controller_addr_, silence_ms / 1000, this->auto_recovery_timeout_ms_,
                 this->auto_rotation_streak_ + 1, MAX_AUTO_ROTATION_STREAK);
        this->rotate_self_address_(true);
        return;
      } else if (!this->auto_recovery_paused_logged_) {
        this->auto_recovery_paused_logged_ = true;
        ESP_LOGE(TAG,
                 "[HEALTH] Auto-recovery paused after %u consecutive failed rotations to protect address slots "
                 "(self=0x%04X, iv=%u, ivi_tx=%u; polling & IV recovery beacons remain active)",
                 this->auto_rotation_streak_, this->self_addr_,
                 static_cast<unsigned int>(bt_mesh.iv_index),
                 static_cast<unsigned int>(BLE_MESH_NET_IVI_TX));
      }
    }
  }

#ifdef USE_ESP32
  // Sequence number monitoring and maintenance warnings
  uint32_t current_seq = bt_mesh.seq;
  if (current_seq >= 13500000 && !this->seq_warn_90_logged_) {
    this->seq_warn_90_logged_ = true;
    ESP_LOGW(TAG,
             "[SEQ_WARN] Sequence number %u reached 90%% of threshold (%u). Automatic address rotation is imminent!",
             static_cast<unsigned int>(current_seq), static_cast<unsigned int>(SEQ_MAX_THRESHOLD));
  } else if (current_seq >= 12000000 && !this->seq_warn_80_logged_) {
    this->seq_warn_80_logged_ = true;
    ESP_LOGW(TAG,
             "[SEQ_WARN] Sequence number %u reached 80%% of threshold (%u). Maintenance rotation recommended.",
             static_cast<unsigned int>(current_seq), static_cast<unsigned int>(SEQ_MAX_THRESHOLD));
  }
#endif

  this->query_status();
  this->set_timeout("periodic_ctrl_info", 500, [this]() {
    this->query_ctrl_info();
  });

  this->publish_maintenance_sensors_();
}

void DaikoMeshComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DAIKO BLE Mesh Controller (DP-41924):");
  ESP_LOGCONFIG(TAG, "  Self Address: 0x%04X (base: 0x%04X, rotation range: 0x%04X..0x%04X)",
                this->self_addr_, this->base_self_addr_, this->base_self_addr_,
                static_cast<uint16_t>(this->base_self_addr_ + 15));
  ESP_LOGCONFIG(TAG, "  Controller Address: 0x%04X", this->controller_addr_);
  ESP_LOGCONFIG(TAG, "  Group Address: 0x%04X", this->group_addr_);
  ESP_LOGCONFIG(TAG, "  SeqNum Threshold: %u", static_cast<unsigned int>(SEQ_MAX_THRESHOLD));
  ESP_LOGCONFIG(TAG, "  Update Interval: %u ms", this->get_update_interval());
}

// ============================================================================
// Bluetooth & BLE Mesh Initialization
// ============================================================================
void DaikoMeshComponent::init_bluetooth_() {
#ifdef USE_ESP32
  esp_err_t err;

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  err = esp_bt_controller_init(&bt_cfg);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "[BT_ERR] esp_bt_controller_init failed: %d", err);
    return;
  }

  err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "[BT_ERR] esp_bt_controller_enable failed: %d", err);
    return;
  }

  err = esp_bluedroid_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "[BT_ERR] esp_bluedroid_init failed: %d", err);
    return;
  }

  err = esp_bluedroid_enable();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "[BT_ERR] esp_bluedroid_enable failed: %d", err);
    return;
  }
#endif
}

void DaikoMeshComponent::init_ble_mesh_() {
#ifdef USE_ESP32
  esp_err_t err;

  const_cast<uint16_t &>(provision_config.prov_unicast_addr) = this->self_addr_;
  provision_config.prov_start_address = this->self_addr_ + 4;
  provision_config.flags = static_cast<uint8_t>(this->persisted_ivu_in_progress_ ? BLE_MESH_NET_FLAG_IVU : 0);
  provision_config.iv_index = this->persisted_iv_index_;

  esp_ble_mesh_register_prov_callback(DaikoMeshComponent::provisioning_callback);
  esp_ble_mesh_register_config_client_callback(DaikoMeshComponent::config_client_callback);
  esp_ble_mesh_register_custom_model_callback(DaikoMeshComponent::custom_model_callback);

  err = esp_ble_mesh_init(&provision_config, &composition);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[MESH_ERR] esp_ble_mesh_init failed: %d", err);
    return;
  }

  err = esp_ble_mesh_client_model_init(&daiko_vendor_models[0]);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[MESH_ERR] esp_ble_mesh_client_model_init failed: %d", err);
    return;
  }

  err = esp_ble_mesh_provisioner_prov_enable(
      static_cast<esp_ble_mesh_prov_bearer_t>(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT));
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "esp_ble_mesh_provisioner_prov_enable returned: %d", err);
  }

  int set_addr_err = bt_mesh_provisioner_set_primary_elem_addr(this->self_addr_);
  if (set_addr_err != 0) {
    ESP_LOGW(TAG, "bt_mesh_provisioner_set_primary_elem_addr returned: %d", set_addr_err);
  }

  err = esp_ble_mesh_provisioner_update_local_net_key(this->net_key_.data(), this->net_idx_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "update_local_net_key returned: %d", err);
  }

  err = esp_ble_mesh_provisioner_add_local_app_key(this->app_key_.data(), this->net_idx_,
                                                   this->app_idx_);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "add_local_app_key returned: %d", err);
  }

  err = esp_ble_mesh_provisioner_bind_app_key_to_local_model(
      this->self_addr_, this->app_idx_, DAIKO_MODEL_ID, DAIKO_COMPANY_ID);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "bind_app_key_to_local_model returned: %d", err);
  }

  // Register target scene controller node (0x0004) in Provisioner database
  this->restore_controller_node_();

  // Subscribe local vendor model to group address (0xC078)
  err = esp_ble_mesh_model_subscribe_group_addr(this->self_addr_, DAIKO_COMPANY_ID, DAIKO_MODEL_ID,
                                                this->group_addr_);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "subscribe_group_addr (0x%04X) returned: %d", this->group_addr_, err);
  }

  this->mesh_initialized_ = true;
  this->node_ready_ = true;
  ESP_LOGW(TAG,
           "[DAIKO_BOOT] DAIKO BLE Mesh ready: self=0x%04X (slot %u/16), ctrl=0x%04X, group=0x%04X, "
           "iv=%u (ivu=%d, ivi_tx=%u), rot_streak=%u/%u, epoch=%u",
           this->self_addr_, (this->self_addr_ - this->base_self_addr_ + 1), this->controller_addr_,
           this->group_addr_, static_cast<unsigned int>(bt_mesh.iv_index),
           bt_mesh_atomic_test_bit(bt_mesh.flags, BLE_MESH_IVU_IN_PROGRESS),
           static_cast<unsigned int>(BLE_MESH_NET_IVI_TX),
           this->auto_rotation_streak_, MAX_AUTO_ROTATION_STREAK, this->wrap_epoch_);
#endif
}

void DaikoMeshComponent::sync_iv_state_(bool from_tx) {
#ifdef USE_ESP32
  if (!this->mesh_initialized_) {
    return;
  }

  uint32_t cur_iv = bt_mesh.iv_index;
  bool cur_ivu = (bt_mesh_atomic_test_bit(bt_mesh.flags, BLE_MESH_IVU_IN_PROGRESS) != 0);

  // Guard against corrupted IV Index or 0 - 1 = 0xFFFFFFFF TX underflow
  if (cur_iv > MAX_VALID_IV_INDEX) {
    ESP_LOGE(TAG, "[IV_ERR] Corrupted bt_mesh.iv_index=0x%08X detected; restoring persisted iv=%u",
             static_cast<unsigned int>(cur_iv), static_cast<unsigned int>(this->persisted_iv_index_));
    bt_mesh.iv_index = this->persisted_iv_index_;
    cur_iv = this->persisted_iv_index_;
    if (this->persisted_ivu_in_progress_) {
      bt_mesh_atomic_set_bit(bt_mesh.flags, BLE_MESH_IVU_IN_PROGRESS);
    } else {
      bt_mesh_atomic_clear_bit(bt_mesh.flags, BLE_MESH_IVU_IN_PROGRESS);
    }
    cur_ivu = this->persisted_ivu_in_progress_;
  }
  if (cur_iv == 0 && cur_ivu) {
    ESP_LOGE(TAG, "[IV_ERR] Invalid state (iv=0, ivu=1) would underflow IVI_TX; clearing IVU flag");
    bt_mesh_atomic_clear_bit(bt_mesh.flags, BLE_MESH_IVU_IN_PROGRESS);
    cur_ivu = false;
  }

  if (cur_iv != this->persisted_iv_index_ || cur_ivu != this->persisted_ivu_in_progress_) {
    const uint32_t prev_iv = this->persisted_iv_index_;
    const bool prev_ivu = this->persisted_ivu_in_progress_;

    ESP_LOGW(TAG,
             "[IV_UPDATE] %s observed IV state change: iv %u (ivu=%u) -> %u (ivu=%u), ivi_tx=%u, seq=%u",
             from_tx ? "TX" : "Poll",
             static_cast<unsigned int>(prev_iv), prev_ivu ? 1 : 0,
             static_cast<unsigned int>(cur_iv), cur_ivu ? 1 : 0,
             static_cast<unsigned int>(BLE_MESH_NET_IVI_TX),
             static_cast<unsigned int>(bt_mesh.seq));

    // When a higher IV Index becomes active in Normal Operation, sequence space restarts cleanly
    if (!cur_ivu && cur_iv > prev_iv) {
      this->wrap_epoch_ = 0;
      this->rtc_wrap_epoch_.save(&this->wrap_epoch_);
      this->seq_warn_80_logged_ = false;
      this->seq_warn_90_logged_ = false;
      uint32_t s = bt_mesh.seq;
      this->rtc_seq_.save(&s);
    }

    this->persisted_iv_index_ = cur_iv;
    this->persisted_ivu_in_progress_ = cur_ivu;

    PersistedIvState iv_state{};
    iv_state.iv_index = cur_iv;
    iv_state.ivu_in_progress = cur_ivu ? 1 : 0;
    iv_state.magic = DAIKO_IV_STATE_MAGIC;
    if (!this->rtc_iv_state_.save(&iv_state)) {
      ESP_LOGE(TAG, "[NVS_ERR] Failed to persist IV state (iv=%u, ivu=%u)",
               static_cast<unsigned int>(cur_iv), iv_state.ivu_in_progress);
    }
    global_preferences->sync();
  }
#endif
}

void DaikoMeshComponent::rotate_self_address() {
  this->rotate_self_address_(false);
}

void DaikoMeshComponent::rotate_self_address_(bool auto_triggered) {
#ifdef USE_ESP32
  uint16_t next_addr = this->self_addr_ + 1;
  bool wrapped = false;
  if (next_addr > (this->base_self_addr_ + 15) || next_addr > 0x7FFF) {
    next_addr = this->base_self_addr_;
    wrapped = true;
    if (this->wrap_epoch_ < 3) {
      this->wrap_epoch_++;
    }
  }
  uint32_t reset_seq = (this->wrap_epoch_ == 0) ? 2000 : static_cast<uint32_t>(this->wrap_epoch_) * 4000000;
  if (wrapped) {
    ESP_LOGW(TAG,
             "[ADDR_WRAP] Address rotation WRAP-AROUND! All 16 slots (0x%04X..0x%04X) exhausted. "
             "Reusing base address 0x%04X (epoch=%u) with seq=%u.",
             this->base_self_addr_, this->base_self_addr_ + 15, next_addr,
             this->wrap_epoch_, static_cast<unsigned int>(reset_seq));
  } else {
    ESP_LOGW(TAG,
             "[ADDR_ROTATE] Rotating self_address 0x%04X -> 0x%04X (slot %u/16, epoch=%u, resetting seq to %u) and restarting...",
             this->self_addr_, next_addr, (next_addr - this->base_self_addr_ + 1),
             this->wrap_epoch_, static_cast<unsigned int>(reset_seq));
  }

  uint8_t next_streak = auto_triggered ? static_cast<uint8_t>(this->auto_rotation_streak_ + 1) : 0;
  uint8_t migrated_v2 = 1;

  if (!this->rtc_self_addr_.save(&next_addr)) {
    ESP_LOGE(TAG, "[NVS_ERR] Failed to save next self_address 0x%04X to NVS!", next_addr);
  }
  if (!this->rtc_seq_.save(&reset_seq)) {
    ESP_LOGE(TAG, "[NVS_ERR] Failed to save reset_seq %u to NVS!", static_cast<unsigned int>(reset_seq));
  }
  this->rtc_addr_migrated_v2_.save(&migrated_v2);
  this->rtc_rot_streak_.save(&next_streak);
  this->rtc_wrap_epoch_.save(&this->wrap_epoch_);
  global_preferences->sync();

  vTaskDelay(pdMS_TO_TICKS(500));
  esp_restart();
#endif
}

uint32_t DaikoMeshComponent::get_sequence_number() const {
#ifdef USE_ESP32
  return bt_mesh.seq;
#else
  return 0;
#endif
}

void DaikoMeshComponent::reset_sequence_number() {
#ifdef USE_ESP32
  uint32_t reset_seq = 2000;
  uint8_t zero_streak = 0;
  ESP_LOGW(TAG, "[SEQ_RESET] Manual Sequence Number reset requested. Setting seq to %u on address 0x%04X and restarting...",
           static_cast<unsigned int>(reset_seq), this->self_addr_);
  if (!this->rtc_seq_.save(&reset_seq)) {
    ESP_LOGE(TAG, "[NVS_ERR] Failed to save reset sequence number %u to NVS!", static_cast<unsigned int>(reset_seq));
  }
  this->rtc_rot_streak_.save(&zero_streak);
  global_preferences->sync();

  vTaskDelay(pdMS_TO_TICKS(500));
  esp_restart();
#endif
}

// ============================================================================
// Direct BLE Mesh Vendor Transmission
// ============================================================================
bool DaikoMeshComponent::send_vendor_packet(uint16_t dst, uint32_t opcode, const uint8_t *data, size_t len) {
#ifdef USE_ESP32
  if (!this->mesh_initialized_) {
    ESP_LOGE(TAG, "[TX_ERR] Cannot send: BLE Mesh not initialized");
    return false;
  }

  this->sync_iv_state_(true);

  esp_ble_mesh_msg_ctx_t ctx = {
      .net_idx = this->net_idx_,
      .app_idx = this->app_idx_,
      .addr = dst,
      .send_rel = false,
      .send_ttl = 7,
  };

  esp_ble_mesh_model_t *model =
      daiko_vendor_client.model ? daiko_vendor_client.model : &daiko_vendor_models[0];
  esp_err_t err = esp_ble_mesh_client_model_send_msg(
      model, &ctx, opcode, len, const_cast<uint8_t *>(data), 1000,
      false, ROLE_PROVISIONER);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[TX_ERR] esp_ble_mesh_client_model_send_msg failed (err=%d, opcode=0x%06X, dst=0x%04X)",
             err, static_cast<unsigned int>(opcode), dst);
    return false;
  }

  char hex[32] = {0};
  size_t print_len = len > 8 ? 8 : len;
  for (size_t i = 0; i < print_len; i++) {
    snprintf(hex + i * 3, sizeof(hex) - i * 3, "%02X ", data[i]);
  }
  ESP_LOGI(TAG, "[TX] Send: src=0x%04X dst=0x%04X op=0x%06X seq=%u iv=%u/%u len=%u payload=%s%s",
           this->self_addr_, dst, static_cast<unsigned int>(opcode),
           static_cast<unsigned int>(bt_mesh.seq),
           static_cast<unsigned int>(bt_mesh.iv_index),
           static_cast<unsigned int>(BLE_MESH_NET_IVI_TX),
           len, hex, len > 8 ? "..." : "");

  if (bt_mesh.seq % 5 == 0) {
    uint32_t s = bt_mesh.seq;
    if (!this->rtc_seq_.save(&s)) {
      ESP_LOGE(TAG, "[NVS_ERR] Failed to save sequence number %u to NVS!", static_cast<unsigned int>(s));
    }
  }
  if (bt_mesh.seq >= SEQ_MAX_THRESHOLD) {
    ESP_LOGE(TAG,
             "[SEQ_MAX] Sequence number %u reached MAX threshold (%u)! Executing address rotation...",
             static_cast<unsigned int>(bt_mesh.seq), static_cast<unsigned int>(SEQ_MAX_THRESHOLD));
    this->rotate_self_address();
  }
  return true;
#else
  return false;
#endif
}

// ============================================================================
// High-Level Control API
// ============================================================================
void DaikoMeshComponent::set_circuit_onoff(uint8_t circuit, bool on) {
  if (circuit < 1 || circuit > NUM_CIRCUITS) {
    return;
  }
  const uint8_t idx = circuit - 1;
  this->circuit_states_[idx].relay_on = on;

  uint8_t payload[6] = {HEADER_SCENE_CTRL, static_cast<uint8_t>(SubCommand::CIRCUIT_ONOFF), 0x02, 0x00, idx, static_cast<uint8_t>(on ? 0x01 : 0x00)};
  ESP_LOGI(TAG, "[CMD] Circuit %u Relay -> %s", circuit, on ? "ON" : "OFF");
  this->send_vendor_packet(this->controller_addr_, DAIKO_OPCODE_SET_UNACK, payload, sizeof(payload));
}

void DaikoMeshComponent::set_circuit_dimming(uint8_t circuit, uint8_t percentage) {
  if (circuit < 1 || circuit > NUM_CIRCUITS) {
    return;
  }
  if (percentage > 100) {
    percentage = 100;
  }

  const uint8_t idx = circuit - 1;
  this->circuit_states_[idx].current_dim = percentage;
  if (percentage > 0) {
    this->circuit_states_[idx].retained_dim = percentage;
  }

  uint8_t payload[6] = {HEADER_SCENE_CTRL, static_cast<uint8_t>(SubCommand::CHANNEL_DIMMING), 0x02, 0x00, static_cast<uint8_t>(idx * 2), percentage};
  ESP_LOGI(TAG, "[CMD] Circuit %u Dimming -> %u%%", circuit, percentage);
  this->send_vendor_packet(this->controller_addr_, DAIKO_OPCODE_SET_UNACK, payload, sizeof(payload));
}

void DaikoMeshComponent::set_circuit_colortemp(uint8_t circuit, uint8_t percentage) {
  if (circuit < 1 || circuit > NUM_CIRCUITS) {
    return;
  }
  if (percentage > 100) {
    percentage = 100;
  }

  const uint8_t idx = circuit - 1;
  this->circuit_states_[idx].colortemp = percentage;

  uint8_t payload[6] = {HEADER_SCENE_CTRL, static_cast<uint8_t>(SubCommand::CHANNEL_DIMMING), 0x02, 0x00, static_cast<uint8_t>(idx * 2 + 1), percentage};
  ESP_LOGI(TAG, "[CMD] Circuit %u ColorTemp -> %u%%", circuit, percentage);
  this->send_vendor_packet(this->controller_addr_, DAIKO_OPCODE_SET_UNACK, payload, sizeof(payload));
}

void DaikoMeshComponent::step_channel_level(uint8_t channel, bool up) {
  if (channel >= NUM_CHANNELS) {
    return;
  }
  uint8_t payload[6] = {HEADER_SCENE_CTRL, static_cast<uint8_t>(SubCommand::CHANNEL_STEP), 0x02, 0x00, channel, static_cast<uint8_t>(up ? 0x01 : 0x00)};
  ESP_LOGI(TAG, "[CMD] Channel %u Step -> %s", channel, up ? "UP" : "DOWN");
  this->send_vendor_packet(this->controller_addr_, DAIKO_OPCODE_SET_UNACK, payload, sizeof(payload));
}

void DaikoMeshComponent::turn_on_circuit(uint8_t circuit, int brightness_pct, int colortemp_pct) {
  if (circuit < 1 || circuit > NUM_CIRCUITS) {
    return;
  }

  const uint8_t idx = circuit - 1;
  auto &st = this->circuit_states_[idx];

  uint8_t target_dim = (brightness_pct > 0)
                           ? static_cast<uint8_t>(brightness_pct > 100 ? 100 : brightness_pct)
                           : (st.retained_dim > 0 ? st.retained_dim : 100);
  uint8_t target_col = (colortemp_pct >= 0)
                           ? static_cast<uint8_t>(colortemp_pct > 100 ? 100 : colortemp_pct)
                           : st.colortemp;
  const bool send_col = (colortemp_pct >= 0) ||
                        ((st.hw_type == HardwareCircuitType::L5_TUNABLE_PWM ||
                          st.hw_type == HardwareCircuitType::L6_TUNABLE_PHASE) &&
                         target_col > 0);

  if (!this->master_power_) {
    ESP_LOGI(TAG, "[CMD] Circuit %u turned ON while Master Power OFF -> Exclusive ON", circuit);

    // Optimistic update
    this->master_power_ = true;
    this->notify_master_state_();
    for (uint8_t c = 1; c <= NUM_CIRCUITS; c++) {
      if (c == circuit) {
        this->circuit_states_[c - 1].relay_on = true;
        this->circuit_states_[c - 1].current_dim = target_dim;
        this->circuit_states_[c - 1].colortemp = target_col;
      } else {
        this->circuit_states_[c - 1].current_dim = 0;
      }
      this->notify_circuit_state_(c);
    }

    // 1. Master power ON to group
    this->all_on();

    // 2. Turn off other circuits staggered by 60ms
    uint32_t delay = 60;
    for (uint8_t c = 1; c <= NUM_CIRCUITS; c++) {
      if (c != circuit) {
        this->set_timeout(delay, [this, c]() {
          this->set_circuit_dimming(c, 0);
        });
        delay += 60;
      }
    }

    // 3. Turn on target circuit (staggered by 60ms to avoid BLE collision)
    this->set_timeout(delay, [this, circuit, target_dim, target_col, send_col]() {
      this->set_circuit_onoff(circuit, true);
      std::string t1 = "turn_on_dim_" + std::to_string(circuit);
      std::string t2 = "turn_on_col_" + std::to_string(circuit);
      this->set_timeout(t1.c_str(), 60, [this, circuit, target_dim, target_col, send_col, t2]() {
        this->set_circuit_dimming(circuit, target_dim);
        if (send_col) {
          this->set_timeout(t2.c_str(), 60, [this, circuit, target_col]() {
            this->set_circuit_colortemp(circuit, target_col);
          });
        }
      });
    });

    this->set_timeout(delay + 800, [this]() {
      this->query_status();
    });
  } else {
    // Optimistic update
    st.relay_on = true;
    st.current_dim = target_dim;
    st.retained_dim = target_dim;
    st.colortemp = target_col;
    this->notify_circuit_state_(circuit);

    this->set_circuit_onoff(circuit, true);
    std::string t1 = "turn_on_dim_" + std::to_string(circuit);
    std::string t2 = "turn_on_col_" + std::to_string(circuit);
    this->set_timeout(t1.c_str(), 60, [this, circuit, target_dim, target_col, send_col, t2]() {
      this->set_circuit_dimming(circuit, target_dim);
      if (send_col) {
        this->set_timeout(t2.c_str(), 60, [this, circuit, target_col]() {
          this->set_circuit_colortemp(circuit, target_col);
        });
      }
    });
  }
}

void DaikoMeshComponent::turn_off_circuit(uint8_t circuit) {
  if (circuit < 1 || circuit > NUM_CIRCUITS) {
    return;
  }
  this->set_circuit_dimming(circuit, 0);
  this->notify_circuit_state_(circuit);
}

void DaikoMeshComponent::set_master_power(bool on) {
  ESP_LOGI(TAG, "[CMD] Set Master Power -> %s", on ? "ON" : "OFF");
  this->master_power_ = on;
  uint8_t payload[5] = {HEADER_SCENE_CTRL, static_cast<uint8_t>(SubCommand::MASTER_POWER), 0x01, 0x00, static_cast<uint8_t>(on ? 0x01 : 0x00)};
  this->send_vendor_packet(this->group_addr_, DAIKO_OPCODE_SET_UNACK, payload, sizeof(payload));

  this->notify_master_state_();
  if (!on) {
    for (size_t i = 0; i < NUM_CIRCUITS; i++) {
      this->circuit_states_[i].current_dim = 0;
      this->notify_circuit_state_(static_cast<uint8_t>(i + 1));
    }
  }
}

void DaikoMeshComponent::recall_scene(uint8_t scene_id) {
  if (scene_id < 1 || scene_id > NUM_SCENES) {
    return;
  }
  ESP_LOGI(TAG, "[CMD] Recall Scene %u (Auto-recovering light states & Optimistic update)", scene_id);

  const uint8_t s_idx = scene_id - 1;
  const bool was_master_off = !this->master_power_;
  const auto &preset = this->scene_presets_[s_idx];

  // Optimistic update
  this->master_power_ = true;
  this->active_scene_ = s_idx;
  this->notify_master_state_();

  if (preset.valid) {
    for (size_t c = 0; c < NUM_CIRCUITS; c++) {
      uint8_t dim = preset.levels[c * 2];
      uint8_t col = preset.levels[c * 2 + 1];
      if (dim != SCENE_CHANNEL_EXCLUDED) {
        this->circuit_states_[c].current_dim = dim;
        this->circuit_states_[c].relay_on = (dim > 0);
        if (dim > 0) {
          this->circuit_states_[c].retained_dim = dim;
        }
      }
      if (col != SCENE_CHANNEL_EXCLUDED) {
        this->circuit_states_[c].colortemp = col;
      }
      this->notify_circuit_state_(static_cast<uint8_t>(c + 1));
    }
  }

  uint32_t delay = 20;
  if (was_master_off) {
    ESP_LOGI(TAG, "[CMD] Master Power was OFF -> Powering ON before scene recall");
    uint8_t p_on[5] = {HEADER_SCENE_CTRL, static_cast<uint8_t>(SubCommand::MASTER_POWER), 0x01, 0x00, 0x01};
    this->send_vendor_packet(this->group_addr_, DAIKO_OPCODE_SET_UNACK, p_on, sizeof(p_on));
    delay = 120;
  }

  // Pre-enable circuits scheduled to be ON
  if (preset.valid) {
    for (size_t c = 0; c < NUM_CIRCUITS; c++) {
      uint8_t dim = preset.levels[c * 2];
      if (dim != SCENE_CHANNEL_EXCLUDED && dim > 0) {
        uint8_t c_num = static_cast<uint8_t>(c + 1);
        this->set_timeout(delay, [this, c_num]() {
          this->set_circuit_onoff(c_num, true);
        });
        delay += 60;
      }
    }
  } else {
    for (uint8_t c = 1; c <= NUM_CIRCUITS; c++) {
      this->set_timeout(delay, [this, c]() {
        this->set_circuit_onoff(c, true);
      });
      delay += 60;
    }
  }

  this->set_timeout(delay + 80, [this, s_idx]() {
    uint8_t payload[5] = {HEADER_GROUP_CMD, SUBCMD_SCENE_RECALL, s_idx, 0x00, 0x00};
    this->send_vendor_packet(this->group_addr_, DAIKO_OPCODE_SET_UNACK, payload, sizeof(payload));
  });

  this->set_timeout(delay + 1000, [this]() {
    this->query_status();
    this->query_ctrl_info();
  });
  this->set_timeout(delay + 3500, [this]() {
    this->query_status();
  });
}

void DaikoMeshComponent::save_scene(uint8_t scene_id,
                                    const std::array<uint8_t, NUM_CHANNELS> &levels,
                                    uint8_t fade_time_sec) {
  if (scene_id < 1 || scene_id > NUM_SCENES) {
    return;
  }
  const uint8_t s = scene_id - 1;
  uint8_t payload[18];
  payload[0] = HEADER_SCENE_CTRL;
  payload[1] = static_cast<uint8_t>(SubCommand::SCENE_SAVE);
  payload[2] = 0x0E;
  payload[3] = 0x00;
  payload[4] = s;
  std::memcpy(&payload[5], levels.data(), NUM_CHANNELS);
  payload[17] = fade_time_sec;

  ESP_LOGI(TAG, "[CMD] Save Scene %u (fade=%us)", scene_id, fade_time_sec);
  this->send_vendor_packet(this->controller_addr_, DAIKO_OPCODE_SET_UNACK, payload, sizeof(payload));
}

void DaikoMeshComponent::set_schedule_mode(ScheduleMode mode) {
  uint8_t payload[5] = {HEADER_SCENE_CTRL, static_cast<uint8_t>(SubCommand::SCHEDULE_MODE), 0x01, 0x00, static_cast<uint8_t>(mode)};
  ESP_LOGI(TAG, "[CMD] Set Schedule Mode -> %u", static_cast<uint8_t>(mode));
  this->send_vendor_packet(this->controller_addr_, DAIKO_OPCODE_SET_UNACK, payload, sizeof(payload));
  this->schedule_mode_ = mode;
}

void DaikoMeshComponent::set_controller_time(const RtcTimeInfo &time_info) {
  uint8_t payload[11] = {
      HEADER_SCENE_CTRL,
      static_cast<uint8_t>(SubCommand::TIME_INFO_SET),
      0x07,
      0x00,
      time_info.year,
      time_info.month,
      time_info.day,
      time_info.weekday,
      time_info.hour,
      time_info.minute,
      time_info.second,
  };
  ESP_LOGI(TAG, "[CMD] Set Controller RTC Time -> 20%02u-%02u-%02u %02u:%02u:%02u",
           time_info.year, time_info.month, time_info.day, time_info.hour, time_info.minute,
           time_info.second);
  this->send_vendor_packet(this->controller_addr_, DAIKO_OPCODE_SET_UNACK, payload, sizeof(payload));
}

// ============================================================================
// Status Queries
// ============================================================================
void DaikoMeshComponent::query_status() {
  uint8_t payload[4] = {HEADER_SCENE_CTRL, static_cast<uint8_t>(SubCommand::LEVEL_INFO), 0x00, 0x00};
  ESP_LOGI(TAG, "[CMD] Query Status (Get Levels: 51 02)");
  this->send_vendor_packet(this->controller_addr_, DAIKO_OPCODE_GET, payload, sizeof(payload));
}

void DaikoMeshComponent::query_ctrl_info() {
  uint8_t payload[4] = {HEADER_SCENE_CTRL, static_cast<uint8_t>(SubCommand::CTRL_INFO), 0x00, 0x00};
  ESP_LOGI(TAG, "[CMD] Query Ctrl Info (51 01)");
  this->send_vendor_packet(this->controller_addr_, DAIKO_OPCODE_GET, payload, sizeof(payload));
}

void DaikoMeshComponent::query_time_info() {
  uint8_t payload[4] = {HEADER_SCENE_CTRL, static_cast<uint8_t>(SubCommand::TIME_INFO_GET), 0x00, 0x00};
  ESP_LOGD(TAG, "[CMD] Query Time Info");
  this->send_vendor_packet(this->controller_addr_, DAIKO_OPCODE_GET, payload, sizeof(payload));
}

void DaikoMeshComponent::query_setting_status() {
  uint8_t payload[4] = {HEADER_SCENE_CTRL, static_cast<uint8_t>(SubCommand::SETTING_STATUS), 0x00, 0x00};
  ESP_LOGD(TAG, "[CMD] Query Setting Status");
  this->send_vendor_packet(this->controller_addr_, DAIKO_OPCODE_GET, payload, sizeof(payload));
}

// ============================================================================
// Vendor Status Response Parsers
// ============================================================================
void DaikoMeshComponent::parse_vendor_status_(uint16_t src, const uint8_t *data, uint16_t len) {
  if (data == nullptr || len < 4) {
    return;
  }

  // Ignore local loopback copies of our own group-addressed packets (e.g. 0xC078)
  if (src == this->self_addr_) {
    return;
  }

  // Handle Scene Recall packet broadcasted to group (02 08 <s> 00 00) from external nodes
  if (data[0] == HEADER_GROUP_CMD && data[1] == SUBCMD_SCENE_RECALL) {
    uint8_t scene_idx = data[2];
    if (scene_idx < NUM_SCENES) {
      ESP_LOGI(TAG, "[RX] Mesh Scene Recall from 0x%04X: Scene %u", src, scene_idx + 1);
      this->active_scene_ = scene_idx;
      this->master_power_ = true;
      this->notify_master_state_();
      this->set_timeout("post_mesh_scene_sync", 1500, [this]() {
        this->query_status();
      });
    }
    return;
  }

  if (data[0] != HEADER_SCENE_CTRL) {
    ESP_LOGW(TAG, "[RX_WARN] Unexpected packet header from 0x%04X: 0x%02X (expected 0x51)", src, data[0]);
    return;
  }

  // Only treat packets originating from the target controller (0x0004) as health proof and status responses
  if (src != this->controller_addr_) {
    ESP_LOGD(TAG, "[RX_IGNORE] Ignoring 0x51 packet from non-controller src=0x%04X (expected 0x%04X)",
             src, this->controller_addr_);
    return;
  }

  // Update controller health & connectivity
  this->last_rx_ms_ = millis();
  this->initial_unresponsive_logged_ = false;
  this->auto_recovery_paused_logged_ = false;
  if (this->auto_rotation_streak_ != 0) {
    this->auto_rotation_streak_ = 0;
#ifdef USE_ESP32
    uint8_t zero_streak = 0;
    this->rtc_rot_streak_.save(&zero_streak);
    global_preferences->sync();
#endif
  }
  if (this->missed_responses_ > 0) {
    this->missed_responses_ = 0;
    for (const auto &cb : this->missed_callbacks_) {
      cb(0);
    }
  }
  if (!this->is_connected_) {
    this->is_connected_ = true;
    ESP_LOGW(TAG, "[HEALTH] Controller 0x%04X communication RESTORED (now ONLINE, silence cleared)", this->controller_addr_);
    this->publish_connectivity_state_();
  }

  const auto subcmd = static_cast<SubCommand>(data[1]);
  switch (subcmd) {
    case SubCommand::CTRL_INFO:
      this->parse_ctrl_info_(data, len);
      break;
    case SubCommand::LEVEL_INFO:
      this->parse_level_info_(data, len);
      break;
    case SubCommand::TIME_INFO_GET:
      this->parse_time_info_(data, len);
      break;
    case SubCommand::SETTING_STATUS:
      this->parse_setting_status_(data, len);
      break;
    default:
      ESP_LOGD(TAG, "Received vendor status subcmd=0x%02X, len=%u", data[1], len);
      break;
  }
}

void DaikoMeshComponent::parse_ctrl_info_(const uint8_t *data, uint16_t len) {
  if (len < 101) {
    ESP_LOGW(TAG, "[RX_ERR] CtrlInfo packet too short (%u < 101)", len);
    return;
  }

  const bool prev_master_power = this->master_power_;
  const bool first_ctrl_info = !this->has_received_ctrl_info_;

  this->active_scene_ = data[4];
  this->master_power_ = (data[5] == 0x01);
  this->schedule_mode_ = static_cast<ScheduleMode>(data[6]);
  this->daily_schedule_index_ = data[7];
  this->weekly_schedule_mode_ = data[8];

  this->ensure_circuit_types_loaded_();
  std::array<uint8_t, NUM_CIRCUITS> discovered_types{};
  bool any_nonzero_type = false;
  for (size_t i = 0; i < NUM_CIRCUITS; i++) {
    discovered_types[i] = data[9 + i];
    if (discovered_types[i] != 0) {
      any_nonzero_type = true;
    }
    this->circuit_states_[i].hw_type = static_cast<HardwareCircuitType>(discovered_types[i]);
  }

  if (any_nonzero_type &&
      (!this->has_persisted_circuit_types_ || discovered_types != this->persisted_circuit_types_)) {
    this->persisted_circuit_types_ = discovered_types;
    this->has_persisted_circuit_types_ = true;
#ifdef USE_ESP32
    PersistedCircuitTypes to_save{};
    to_save.types = discovered_types;
    to_save.magic = DAIKO_CIRCUIT_TYPES_MAGIC;
    if (this->rtc_circuit_types_.save(&to_save)) {
      uint32_t s = bt_mesh.seq;
      this->rtc_seq_.save(&s);
      global_preferences->sync();
      ESP_LOGW(TAG,
               "[CTRL_INFO] Discovered/updated hardware circuit types in NVS "
               "(C1=0x%02X C2=0x%02X C3=0x%02X C4=0x%02X C5=0x%02X C6=0x%02X). "
               "Restarting to apply Home Assistant light traits...",
               discovered_types[0], discovered_types[1], discovered_types[2], discovered_types[3],
               discovered_types[4], discovered_types[5]);
      this->set_timeout("ctype_apply_reboot", 300, []() { esp_restart(); });
    }
#endif
  }

  // Bytes 16..22: Embedded RTC Time [YY, MM, DD, WW, hh, mm, ss]
  this->rtc_time_.year = data[16];
  this->rtc_time_.month = data[17];
  this->rtc_time_.day = data[18];
  this->rtc_time_.weekday = data[19];
  this->rtc_time_.hour = data[20];
  this->rtc_time_.minute = data[21];
  this->rtc_time_.second = data[22];
  this->rtc_time_.valid = true;

  // Bytes 23..100: 6 Scenes x 13 Bytes ([ch0..ch11, fade_sec])
  const uint8_t *scene_base = &data[23];
  for (size_t s = 0; s < NUM_SCENES; s++) {
    const uint8_t *entry = &scene_base[s * 13];
    std::memcpy(this->scene_presets_[s].levels.data(), entry, NUM_CHANNELS);
    this->scene_presets_[s].fade_time_sec = entry[12];
    this->scene_presets_[s].valid = true;
  }

  this->has_received_ctrl_info_ = true;
  ESP_LOGI(TAG,
           "[STATE] CtrlInfo received: MasterPower=%s, ActiveScene=%u, SchedMode=%u, RTC=20%02u-%02u-%02u %02u:%02u:%02u",
           this->master_power_ ? "ON" : "OFF", this->active_scene_ + 1,
           static_cast<unsigned int>(this->schedule_mode_), this->rtc_time_.year, this->rtc_time_.month,
           this->rtc_time_.day, this->rtc_time_.hour, this->rtc_time_.minute, this->rtc_time_.second);

  this->notify_master_state_();

  if (!this->master_power_) {
    this->publish_state_to_entities_();
  } else if ((first_ctrl_info || !prev_master_power) && this->master_power_) {
    this->query_status();
  }
}

void DaikoMeshComponent::parse_level_info_(const uint8_t *data, uint16_t len) {
  if (len < 16 || data[2] != 0x0C) {
    ESP_LOGW(TAG, "[RX_ERR] Invalid LevelInfo packet (len=%u)", len);
    return;
  }

  const uint8_t *levels = &data[4];
  for (uint8_t c = 1; c <= NUM_CIRCUITS; c++) {
    const uint8_t idx = c - 1;
    const uint8_t raw_dim = levels[idx * 2];
    const uint8_t raw_col = levels[idx * 2 + 1];

    // Bit 7 (0x80) of even channel byte indicates circuit switch/relay OFF (51 35)
    const bool relay_on = (raw_dim & 0x80) == 0;
    uint8_t dim = raw_dim & 0x7F;
    if (dim > 100) {
      dim = 100;
    }
    uint8_t col = raw_col & 0x7F;
    if (col > 100) {
      col = 100;
    }

    this->circuit_states_[idx].relay_on = relay_on;
    this->circuit_states_[idx].current_dim = dim;
    if (dim > 0) {
      this->circuit_states_[idx].retained_dim = dim;
    }
    this->circuit_states_[idx].colortemp = col;

    ESP_LOGD(TAG, "  [STATE] Circuit %u: %s (relay=%s, dim=%u%%, col=%u%%, raw=0x%02X)", c,
             this->circuit_states_[idx].is_effectively_on(this->master_power_) ? "ON" : "OFF",
             relay_on ? "ON" : "OFF", dim, col, raw_dim);
  }

  char levels_str[128] = {0};
  int offset = 0;
  for (uint8_t c = 1; c <= NUM_CIRCUITS; c++) {
    offset += snprintf(levels_str + offset, sizeof(levels_str) - offset,
                       "C%u:%s(%u%%) ", c,
                       this->circuit_states_[c - 1].is_effectively_on(this->master_power_) ? "ON" : "OFF",
                       this->circuit_states_[c - 1].current_dim);
  }
  ESP_LOGI(TAG, "[STATE] LevelInfo: %s", levels_str);

  this->publish_state_to_entities_();
}

void DaikoMeshComponent::parse_time_info_(const uint8_t *data, uint16_t len) {
  if (len < 11 || data[2] != 0x07) {
    return;
  }
  this->rtc_time_.year = data[4];
  this->rtc_time_.month = data[5];
  this->rtc_time_.day = data[6];
  this->rtc_time_.weekday = data[7];
  this->rtc_time_.hour = data[8];
  this->rtc_time_.minute = data[9];
  this->rtc_time_.second = data[10];
  this->rtc_time_.valid = true;
  ESP_LOGD(TAG, "[STATE] TimeInfo: 20%02u-%02u-%02u %02u:%02u:%02u", this->rtc_time_.year,
           this->rtc_time_.month, this->rtc_time_.day, this->rtc_time_.hour, this->rtc_time_.minute,
           this->rtc_time_.second);
}

void DaikoMeshComponent::parse_setting_status_(const uint8_t *data, uint16_t len) {
  if (len < 5) {
    return;
  }
  this->setting_status_ = data[4];
  ESP_LOGD(TAG, "[STATE] SettingStatus: %u", this->setting_status_);
}

void DaikoMeshComponent::notify_circuit_state_(uint8_t circuit) {
  if (circuit < 1 || circuit > NUM_CIRCUITS) {
    return;
  }
  const auto &st = this->circuit_states_[circuit - 1];
  const bool report_on = st.is_effectively_on(this->master_power_);
  const uint8_t report_dim = report_on ? st.current_dim : st.retained_dim;

  for (const auto &cb : this->circuit_callbacks_) {
    cb(circuit, report_on, report_dim, st.colortemp);
  }
}

void DaikoMeshComponent::notify_master_state_() {
  for (const auto &cb : this->master_callbacks_) {
    cb(this->master_power_);
  }
}

void DaikoMeshComponent::publish_state_to_entities_() {
  for (uint8_t c = 1; c <= NUM_CIRCUITS; c++) {
    this->notify_circuit_state_(c);
  }
}

void DaikoMeshComponent::publish_maintenance_sensors_() {
  uint16_t addr = this->self_addr_;
  for (const auto &cb : this->address_callbacks_) {
    cb(addr);
  }
  uint32_t seq = this->get_sequence_number();
  for (const auto &cb : this->sequence_callbacks_) {
    cb(seq);
  }
  uint32_t missed = this->missed_responses_;
  for (const auto &cb : this->missed_callbacks_) {
    cb(missed);
  }
  uint8_t scene = this->active_scene_ + 1;
  for (const auto &cb : this->active_scene_callbacks_) {
    cb(scene);
  }
  std::string pair_str = this->get_pairing_state_str();
  for (const auto &cb : this->pairing_state_callbacks_) {
    cb(this->pairing_state_, pair_str);
  }
}

void DaikoMeshComponent::publish_connectivity_state_() {
  bool conn = this->is_connected_;
  for (const auto &cb : this->connectivity_callbacks_) {
    cb(conn);
  }
}

// ============================================================================
// Standalone Provisioning & Pairing Implementation
// ============================================================================
void DaikoMeshComponent::set_pairing_state_(PairingState state, const std::string &message) {
  this->pairing_state_ = state;
  const std::string desc = message.empty() ? this->get_pairing_state_str() : message;
  ESP_LOGI(TAG, "[PAIR_STATE] %s", desc.c_str());
  if (this->text_sensor_pairing_state_ != nullptr) {
    this->text_sensor_pairing_state_->publish_state(desc);
  }
  for (const auto &cb : this->pairing_state_callbacks_) {
    if (cb) {
      cb(state, desc);
    }
  }
}

std::string DaikoMeshComponent::get_pairing_state_str() const {
  switch (this->pairing_state_) {
    case PairingState::IDLE:
      return "Idle / Standby";
    case PairingState::SCANNING:
      return "Scanning for DP-41924 (PB-ADV)...";
    case PairingState::PROVISIONING:
      return "Provisioning NetKey...";
    case PairingState::CONFIG_APPKEY_ADDING:
      return "Adding AppKey...";
    case PairingState::CONFIG_MODEL_BINDING:
      return "Binding Vendor Model...";
    case PairingState::CONFIG_MODEL_SUB_ADDING:
      return "Subscribing Group 0xC078...";
    case PairingState::SUCCESS:
      return "Success";
    case PairingState::FAILED:
      return "Failed";
    default:
      return "Unknown";
  }
}

void DaikoMeshComponent::send_config_app_key_add_(uint16_t node_addr) {
#ifdef USE_ESP32
  esp_ble_mesh_cfg_app_key_add_t app_key_add = {
      .net_idx = this->net_idx_,
      .app_idx = this->app_idx_,
  };
  memcpy(app_key_add.app_key, this->app_key_.data(), 16);

  esp_ble_mesh_client_common_param_t common = {
      .opcode = ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD,
      .model = config_client.model ? config_client.model : &root_models[1],
      .ctx = {
          .net_idx = this->net_idx_,
          .app_idx = 0x0000,
          .addr = node_addr,
          .send_rel = false,
          .send_ttl = 7,
      },
      .msg_timeout = 8000,
  };
  esp_err_t err = esp_ble_mesh_config_client_set_state(
      &common, reinterpret_cast<esp_ble_mesh_cfg_client_set_state_t *>(&app_key_add));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[PAIR_ERR] esp_ble_mesh_config_client_set_state (AppKey Add) failed: %d", err);
    this->on_pairing_failed_("Failed to send AppKey Add");
  } else {
    ESP_LOGI(TAG, "[PAIR_TX] Sent Config AppKey Add to 0x%04X (app_idx=%u)", node_addr, this->app_idx_);
  }
#endif
}

void DaikoMeshComponent::send_config_model_app_bind_(uint16_t node_addr) {
#ifdef USE_ESP32
  esp_ble_mesh_cfg_model_app_bind_t model_bind = {
      .element_addr = node_addr,
      .model_app_idx = this->app_idx_,
      .model_id = DAIKO_MODEL_ID,
      .company_id = DAIKO_COMPANY_ID,
  };

  esp_ble_mesh_client_common_param_t common = {
      .opcode = ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND,
      .model = config_client.model ? config_client.model : &root_models[1],
      .ctx = {
          .net_idx = this->net_idx_,
          .app_idx = 0x0000,
          .addr = node_addr,
          .send_rel = false,
          .send_ttl = 7,
      },
      .msg_timeout = 8000,
  };
  esp_err_t err = esp_ble_mesh_config_client_set_state(
      &common, reinterpret_cast<esp_ble_mesh_cfg_client_set_state_t *>(&model_bind));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[PAIR_ERR] esp_ble_mesh_config_client_set_state (Model App Bind) failed: %d", err);
    this->on_pairing_failed_("Failed to send Model App Bind");
  } else {
    ESP_LOGI(TAG, "[PAIR_TX] Sent Config Model App Bind to 0x%04X (model=0x%04X, cid=0x%04X)",
             node_addr, DAIKO_MODEL_ID, DAIKO_COMPANY_ID);
  }
#endif
}

void DaikoMeshComponent::send_config_model_sub_add_(uint16_t node_addr) {
#ifdef USE_ESP32
  esp_ble_mesh_cfg_model_sub_add_t model_sub = {
      .element_addr = node_addr,
      .sub_addr = this->group_addr_,
      .model_id = DAIKO_MODEL_ID,
      .company_id = DAIKO_COMPANY_ID,
  };

  esp_ble_mesh_client_common_param_t common = {
      .opcode = ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD,
      .model = config_client.model ? config_client.model : &root_models[1],
      .ctx = {
          .net_idx = this->net_idx_,
          .app_idx = 0x0000,
          .addr = node_addr,
          .send_rel = false,
          .send_ttl = 7,
      },
      .msg_timeout = 8000,
  };
  esp_err_t err = esp_ble_mesh_config_client_set_state(
      &common, reinterpret_cast<esp_ble_mesh_cfg_client_set_state_t *>(&model_sub));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[PAIR_ERR] esp_ble_mesh_config_client_set_state (Model Sub Add) failed: %d", err);
    this->on_pairing_failed_("Failed to send Model Sub Add");
  } else {
    ESP_LOGI(TAG, "[PAIR_TX] Sent Config Model Sub Add to 0x%04X (group=0x%04X, model=0x%04X)",
             node_addr, this->group_addr_, DAIKO_MODEL_ID);
  }
#endif
}

void DaikoMeshComponent::start_pairing(uint32_t timeout_sec) {
  if (this->pairing_state_ != PairingState::IDLE &&
      this->pairing_state_ != PairingState::FAILED &&
      this->pairing_state_ != PairingState::SUCCESS) {
    ESP_LOGW(TAG, "[PAIR] Pairing workflow already in progress (state: %s)",
             this->get_pairing_state_str().c_str());
    return;
  }

  ESP_LOGW(TAG, "============================================================");
  ESP_LOGW(TAG, ">>> STARTING STANDALONE BLE MESH PAIRING WORKFLOW <<<");
  ESP_LOGW(TAG, "  Target Address: 0x%04X", this->controller_addr_);
  ESP_LOGW(TAG, "  Group Address: 0x%04X", this->group_addr_);
  ESP_LOGW(TAG, "  Timeout: %u seconds", static_cast<unsigned int>(timeout_sec));
  ESP_LOGW(TAG, "  Please set DP-41924 into pairing / unprovisioned mode now!");
  ESP_LOGW(TAG, "============================================================");

  // Pause normal periodic polling while pairing is running
  this->node_ready_ = false;
  this->pairing_started_ms_ = millis();

#ifdef USE_ESP32
  // Clear any existing provisioner node records so duplicate address checks pass
  bt_mesh_provisioner_remove_node(nullptr);
#endif

  this->set_pairing_state_(PairingState::SCANNING, "Scanning for DP-41924 (PB-ADV)...");

  // Timeout guard
  this->set_timeout("pairing_timeout", timeout_sec * 1000, [this]() {
    ESP_LOGW(TAG, "[PAIR_TIMEOUT] Pairing workflow timed out waiting for device");
    this->on_pairing_failed_("Pairing timed out");
  });
}

void DaikoMeshComponent::cancel_pairing() {
  if (this->pairing_state_ == PairingState::IDLE) {
    return;
  }
  ESP_LOGW(TAG, "[PAIR] User cancelled pairing workflow");
  this->cancel_timeout("pairing_timeout");
  this->cancel_timeout("pair_appkey_add");
  this->cancel_timeout("pair_model_bind");
  this->cancel_timeout("pair_sub_add");
  this->set_pairing_state_(PairingState::IDLE, "Pairing cancelled");
  this->node_ready_ = true;
}

void DaikoMeshComponent::on_pairing_completed_() {
  this->cancel_timeout("pairing_timeout");
  ESP_LOGW(TAG, "============================================================");
  ESP_LOGW(TAG, ">>> STANDALONE PAIRING COMPLETED SUCCESSFULLY! <<<");
  ESP_LOGW(TAG, "  Node 0x%04X provisioned and configured.", this->controller_addr_);
  ESP_LOGW(TAG, "  SeqNum initialized to 2000. Normal control active!");
  ESP_LOGW(TAG, "============================================================");

  this->set_pairing_state_(PairingState::SUCCESS, "Pairing successful! Ready.");

  this->mesh_initialized_ = true;
  this->node_ready_ = true;
  this->is_connected_ = true;
  this->last_rx_ms_ = millis();
  this->missed_responses_ = 0;

  // Sync initial state
  this->set_timeout("boot_sync_ctrl", 1500, [this]() { this->query_ctrl_info(); });
  this->set_timeout("boot_sync_level", 3000, [this]() { this->query_status(); });

  this->publish_maintenance_sensors_();
  this->publish_connectivity_state_();

  // Return status to IDLE after 5 seconds
  this->set_timeout("pair_status_reset", 5000, [this]() {
    if (this->pairing_state_ == PairingState::SUCCESS) {
      this->set_pairing_state_(PairingState::IDLE, "Idle / Connected");
    }
  });
}

void DaikoMeshComponent::on_pairing_failed_(const std::string &reason) {
  this->cancel_timeout("pairing_timeout");
  this->cancel_timeout("pair_appkey_add");
  this->cancel_timeout("pair_model_bind");
  this->cancel_timeout("pair_sub_add");

  ESP_LOGE(TAG, "[PAIR_FAIL] Pairing workflow terminated: %s", reason.c_str());
  this->set_pairing_state_(PairingState::FAILED, reason);

  // Resume normal polling if we previously had a valid node
  this->restore_controller_node_();
  this->node_ready_ = true;
}

void DaikoMeshComponent::restore_controller_node_() {
#ifdef USE_ESP32
  struct bt_mesh_node ctrl_node = {};
  ctrl_node.dev_uuid[0] = 0xDA;
  ctrl_node.dev_uuid[1] = 0x10;
  ctrl_node.unicast_addr = this->controller_addr_;
  ctrl_node.element_num = 6;
  ctrl_node.net_idx = this->net_idx_;
  bt_mesh_provisioner_restore_node_info(&ctrl_node);
#endif
}

}  // namespace daiko_scenecontroller_ble
}  // namespace esphome
