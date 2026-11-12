# Third-Party Notices & Disclaimers

`esphome-daiko-scenecontroller-ble` interacts with open-source frameworks and third-party hardware ecosystems. This document provides trademark disclaimers, upstream license notices, and legal compliance guidelines.

---

## 1. Trademark & Non-Affiliation Disclaimer

- **DAIKO** (大光電機) is a trademark or registered trademark of DAIKO ELECTRIC CO., LTD. (大光電機株式会社).
- **Bluetooth** is a registered trademark of Bluetooth SIG, Inc.
- **ESP32** and **ESP-IDF** are trademarks of Espressif Systems (Shanghai) Co., Ltd.
- **Home Assistant** and **ESPHome** are trademarks of Open Home Foundation / Nabu Casa, Inc.

`esphome-daiko-scenecontroller-ble` is an independent open-source interoperability project and is **not** affiliated with, endorsed by, sponsored by, or supported by DAIKO ELECTRIC CO., LTD. or any of its affiliates.

---

## 2. Scope of Distributed Material & Clean-Room Compliance

This repository distributes **original C++ / Python source code, YAML configuration templates, and technical documentation only**.

1. **No Proprietary Code or Assets**:
   - This repository does **not** contain, redistribute, or bundle any proprietary applications, firmware binaries, decompiled code, images, manuals, or resources owned by third-party manufacturers.
2. **No Pre-Configured Cryptographic Keys**:
   - This repository does **not** include any device-specific or network-specific cryptographic keys (`NetKey` or `AppKey`). Users must configure their own randomly generated cryptographic keys for their lighting environment.
3. **Radio & Electrical Safety**:
   - This component operates over standard Bluetooth Low Energy (BLE Mesh) APIs provided by Espressif's official ESP-IDF framework on certified hardware modules. It does not modify radio frequency parameters or transmission power beyond regulatory limits.
   - No physical modification to commercial AC wiring or wall-mounted lighting controllers is required; all communication takes place wirelessly via the standard BLE Mesh bearer.

---

## 3. Upstream Open-Source Frameworks

When compiled with ESPHome, this component links against the following upstream open-source projects under their respective licenses:

| Component / Framework | Upstream License | Description |
| :--- | :--- | :--- |
| **ESPHome** | MIT / GPL-3.0 | Smart home firmware build framework and Python codegen API |
| **ESP-IDF (`esp_ble_mesh`, `bt`, `nvs_flash`)** | Apache-2.0 | Espressif IoT Development Framework and Bluetooth Mesh stack |
| **Zephyr Bluetooth Mesh (via ESP-BLE-Mesh)** | Apache-2.0 | Underlying Bluetooth Mesh profile implementation in ESP-IDF |
