# esphome-daiko-scenecontroller-ble - Developer & AI Agent Guide (AGENTS.md)

本ドキュメントは、`esphome-daiko-scenecontroller-ble` の開発・保守・機能拡張を行う **開発者および AI コーディングエージェント** 向けのアーキテクチャ設計・開発規約ガイドラインです。

---

## 1. プロジェクト概要 & アーキテクチャ

`esphome-daiko-scenecontroller-ble` は、大光電機（DAIKO）製 6回路シーンコントローラー（型番: `DP-41924`）を ESP32 の ESP-IDF Bluetooth Mesh スタック（`esp_ble_mesh`）を用いてローカル制御し、Home Assistant へ統合するための ESPHome カスタムコンポーネント（`daiko_scenecontroller_ble`）です。

```text
[ Home Assistant ]
       │  ESPHome Native API (Noise Encrypted over Wi-Fi)
       ▼
[ ESP32 Node (ESPHome + daiko_scenecontroller_ble) ]
  ├── Hub Component     : DaikoMeshComponent (PollingComponent, Unicast: 0x0020..0x002F)
  ├── Child Entities    : Auto-created Light (x7: Master + Circuits 1..6 auto-type), Button (x13), Sensor (x3), BinarySensor (x1), TextSensor (x3)
  ├── NVS Persistence   : SeqNum, SelfAddress, PersistedIvState, PersistedCircuitTypes, RotationStreak, WrapEpoch
  └── ESP-IDF BLE Mesh  : Provisioner Role, Vendor Model (CID: 0x0AF8, Model ID: 0x0000)
       │  BLE Mesh Advertising Bearer (NetKey / AppKey AES-CCM Encrypted)
       ▼
[ DAIKO Scene Controller DP-41924 (Unicast: 0x0004, Group: 0xC078) ]
```

---

## 2. ビルド・テスト・検証手順

コードやドキュメントを変更した際は、必ず以下のコマンドでガードレールテストと ESPHome 検証が通ることを確認してください：

```bash
# 1. PR ガードレール・構文・セキュリティ・法規コンプライアンス検証スイート
python3 -m unittest discover -s tests -v

# 2. Python コード品質チェック (Ruff がインストールされている場合)
ruff check components tests

# 3. secrets.yaml の準備（初回のみ）
cp -n secrets.yaml.example secrets.yaml

# 4. ESPHome 設定スキーマ & C++ コード生成検証
esphome config daiko_controller.yaml

# 5. ESPHome ファームウェアコンパイル検証（ESP-IDF ビルド環境がある場合）
esphome compile daiko_controller.yaml
```

---

## 3. ディレクトリ構造 & 主要モジュール

```text
esphome-daiko-scenecontroller-ble/
├── AGENTS.md                        # 本ドキュメント (開発・AIエージェント向け設計規約)
├── CONTRIBUTING.md                  # コントリビューション・ブランチ運用ガイド
├── README.md                        # プロジェクト概要・クイックスタート・エンティティ一覧
├── PAIRING_GUIDE.md                 # ペアリング & 初期化手順書
├── LICENSE                          # MIT ライセンス
├── THIRD_PARTY_NOTICES.md           # 商標・サードパーティライセンス・法規遵守表記
├── daiko_controller.yaml            # ESPHome 設定ファイルテンプレート
├── secrets.yaml.example             # secrets.yaml テンプレート
├── docs/
│   ├── PROTOCOL_SPEC.md             # BLE Mesh ベンダープロトコル仕様書
│   ├── ARCHITECTURE_AND_BEHAVIOR.md # 内部挙動と設計アーキテクチャ仕様書
│   └── TROUBLESHOOTING_AND_MAINTENANCE_GUIDE.md # トラブルシューティング & 保守改修ガイド
├── components/
│   └── daiko_scenecontroller_ble/   # ESPHome カスタムコンポーネント本体
│       ├── __init__.py              # ハブ設定スキーマ・自動エンティティ生成 & ESP-IDF コンポーネント組み込み定義
│       ├── daiko_scenecontroller_ble.h   # プロトコル定数・状態構造体・DaikoMeshComponent 宣言
│       ├── daiko_scenecontroller_ble.cpp # BLE Mesh 初期化・NVS永続化・回路種別自動判別・送受信・自己復旧実装
│       ├── light/                   # 主電源・ON/OFF・調光・調色ライトエンティティ (type: auto/master 対応 & 330ms スロットル)
│       ├── button/                  # シーン1〜6呼出・全点灯/全消灯・同期・保守ボタン
│       ├── sensor/                  # SeqNum・未応答カウント・現在シーン番号センサー
│       ├── binary_sensor/           # 親機オンライン接続状態センサー
│       └── text_sensor/             # セルフアドレス・通信ステータステキストセンサー
└── tests/
    └── test_pr_guardrails.py        # PR 品質・アーキテクチャ・セキュリティ・法規監査テスト
```

---

## 4. 開発・修正時の重要ナレッジ

1. **ハードウェアの二重状態モデル（`0x80` 回路遮断ビット vs `0..100%` 調光レベル）**:
   - `DP-41924` の各回路は `51 35`（回路開閉、`LevelInfo` の Bit 7 `0x80`）と `51 51`（調光レベル `0..100%`）の2層で管理されます。
   - `0x80` ビットがセットされた回路は、`51 51` やシーン呼出（`02 08`）を送っても点灯復帰しないハードウェア特性があるため、通常消灯時は `51 51` でレベル `0%` を送り、点灯時やシーン呼出時には `51 35`（回路 ON）を先行送信する必要があります。
2. **BLE Mesh IV Index 追従と NVS 汚染隔離 (`sync_iv_state_`)**:
   - ESP-IDF 標準の `ble_mesh` NVS 名前空間は起動時にパージしつつ、正当な IV Index / IVU 状態は独自 NVS（`daiko_iv_state_v1`, `PersistedIvState`, `magic = 0x44495631`）で永続化します。
   - `sdkconfig_options` の `CONFIG_BLE_MESH_IVU_RECOVERY_IVI: y` により、Secure Network Beacon（SNB）による上位 IV Index への遷移（`0 -> 1 ...`）に自動追従します。
   - ESP-IDF の `BLE_MESH_NET_IVI_TX` マクロが `(iv_index == 0, ivu_in_progress == 1)` で `0 - 1 = 0xFFFFFFFF` にアンダーフローするのを防ぐガードを維持してください。
3. **ローカルループバック遮断と送信元検証**:
   - グループアドレス `0xC078` を購読しているため、自身が送信したグループコマンドが `custom_model_callback()` にエコーバックされます。
   - `src == self_addr_` は `[RX_LOCAL]` として無視し、`0x51` ステータスパケットは `src == controller_addr_`（`0x0004`）からの応答のみをヘルスチェック（`last_rx_ms_` 更新・`RESTORED` 判定）に採用してください。
4. **有界なセルフアドレス自動ローテーション (`MAX_AUTO_ROTATION_STREAK = 2`)**:
   - 親機無応答時の自動アドレスローテーションは連続 2 回までに制限しています。これにより、親機停電や IV Index 不一致の際に 16 スロット（`0x0020`〜`0x002F`）を短時間で焼き切る暴走を防ぎます。
5. **ペアリング不変条件と状態初期化 (`start_pairing`)**:
   - `DP-41924` は標準 BLE Mesh PB-ADV (No OOB) でプロビジョニングされます。
   - プロビジョニング完了時（`PROV_COMPLETE_EVT`）には、親機側のリプレイ保護リスト（RPL）および設定状態初期化に合わせ、ESP32 側の SeqNum を 2000、IV Index を 0、セルフアドレスを `0x0021` にリセットし、ローテーション履歴をクリアします。
   - ペアリングシーケンス（Provisioning -> AppKey Add -> Model App Bind -> Model Sub Add 0xC078 -> Status Sync）は `Config Client` を用いて決定論的に進行し、途中で失敗した場合は安全に `IDLE` へ復帰させます。

---

## 5. 開発時のアンチパターン（厳禁事項）

1. **`bt_mesh.iv_index = 0` への強制固定・引き戻しを行わない**:
   ネットワークの正規 IV Index 更新（`0 -> 1` 等）が発生した瞬間に AES-CCM Nonce 不一致で全パケットが破棄されます。
2. **`CONFIG_BLE_MESH_IVU_RECOVERY_IVI: y` を削除しない**:
   `daiko_controller.yaml` の `sdkconfig_options` からこの設定を外すと、IV Index 遷移からの自動回復が機能しなくなります。
3. **受信元アドレス（`src == controller_addr_`）の検証を外さない**:
   自アドレスからのループバックエコーで `is_connected_ = true` に誤復帰し、ヘルス監視が破綻します。
4. **公式アプリのバイナリ・リソースやリバースエンジニアリングを示唆する記述を混入させない**:
   APK、共有ライブラリ（`.so`）、展開リソース等のコミットは厳禁です。また、コードコメントやドキュメントに逆アセンブル・バイナリ解析・内部シンボル名を示唆する記述を含めてはなりません。
5. **絵文字を使用しない**:
   `README.md`、各種ドキュメント、Issue/PR テンプレートに絵文字（Emoji）を使用しないでください。

---

## 6. AI エージェント行動規範 & ブランチ・リリース運用ルール (AI Agent Rules)

AI エージェント（Antigravity, Cursor, Gemini, Claude 等）が自律的またはペアプログラミングで作業を行う際、**以下のルールを最優先で遵守** してください。

### 1. 保護ブランチへの直接コミット・プッシュ厳禁
- **`main` および `release/*` ブランチへ直接コミットまたは直接プッシュ（Direct push）することは厳禁** です。
- 新機能の実装やバグ修正を行う際は、必ず最新の `main` から派生したトピックブランチ（`feat/...`, `fix/...`, `test/...`, `refactor/...`, `docs/...`, `chore/...`）を作成して作業してください。
- 作業完了後はトピックブランチから `main` ブランチへ向けた **Pull Request (PR)** を作成し、ユーザー（オーナー）に確認を求めてください。

### 2. リリースタグ・バージョニング規則の遵守
- リリースタグ `v<MAJOR>.<MINOR>.<PATCH>` の作成は、**ユーザーから明示的なリリース指示があった場合のみ** 行ってください。
- バージョン採番は **SemVer 2.0.0** に厳格に従ってください。

### 3. テスト実行と設定検証の義務
- コードやドキュメントを変更した後は、コミットまたは PR 作成前に **必ず以下のコマンドを実行し、全件 PASS を確認** してください：
  - `python3 -m unittest discover -s tests -v`: ガードレールテストスイート全件 PASS
  - `esphome config daiko_controller.yaml`: ESPHome 設定検証 PASS

### 4. 機密情報・特定環境固有パラメータのコミット防止
- 実際の `NetKey` / `AppKey`、Wi-Fi パスワード、API キー、プライベート IP アドレス（`192.168.x.x` 等）、ローカルホストパス（`/Users/...` 等）をコードやドキュメントに記述してコミットしてはなりません。
- 特定環境向けの秘密情報は必ず `.gitignore` 対象の `secrets.yaml` または `.local/` 内にのみ保持し、公開ドキュメント上はプレースホルダー（`<SYSLOG_SERVER_IP>` 等）を使用してください。
