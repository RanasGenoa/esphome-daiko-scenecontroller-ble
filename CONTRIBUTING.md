# esphome-daiko-scenecontroller-ble コントリビューション & ブランチ・リリース運用ガイド (CONTRIBUTING.md)

`esphome-daiko-scenecontroller-ble` プロジェクトへのご協力ありがとうございます。
本ドキュメントは、**複数人での開発・検証（DP-41924 実機検証を含む）を安全かつ迅速に進めるためのブランチ戦略、バージョニング規則、および開発運用ルール** を定めたガイドラインです。

---

## 1. バージョニング戦略 (Semantic Versioning)

本プロジェクトは **[Semantic Versioning 2.0.0 (SemVer)](https://semver.org/lang/ja/)** に準拠してリリース版を管理します。

形式: `vMAJOR.MINOR.PATCH`（例: `v1.0.0`）

| 種別 | インクリメント基準 | 変更の例 |
| :--- | :--- | :--- |
| **MAJOR** (破壊的変更) | YAML 設定スキーマの非互換変更、必須 ESPHome バージョンや前提条件の破壊的刷新 | コンポーネント設定キーの変更、エンティティ構造の非互換刷新 |
| **MINOR** (機能追加) | 後方互換性のある新機能の追加・拡張 | 新規サブコマンド対応（スケジュール設定等）、新規センサー追加、ポーリング制御改善 |
| **PATCH** (バグ修正) | 後方互換性のあるバグ修正・安定性向上・ドキュメント改善 | BLE Mesh IV Index 追従改善、NVS 永続化バグ修正、ドキュメント修正、テスト追加 |

### 実機検証用プレリリース (Release Candidate)
`DP-41924` 実機での長期稼働検証や BLE Mesh 通信テストを行う場合、**プレリリースタグ** を使用します：
- 例: `v1.1.0-rc.1`, `v1.1.0-rc.2`
- 実機での動作検証（各回路の調光・調色、シーン切替、主電源連動、IV Index / SeqNum 遷移、再起動後の NVS 復元）が確認された段階で、正式版を発行します。

---

## 2. ブランチ戦略 (GitHub Flow + Release Branch)

迅速な反復開発と実機環境での安全な安定化を両立するため、**GitHub Flow をベースとしたトピックブランチ運用** を採用します。

```text
[ feature / bugfix branches ]
  feat/schedule-support ───┐
  fix/iv-index-recovery ───┼──> (Pull Request & レビュー) ──> [ main ] (保護ブランチ)
  test/protocol-guardrail ─┘                                      │
                                                                  ▼
                                                      Tag: v1.1.0-rc.1 (実機検証)
                                                                  │
                                                                  ▼
                                                      Tag: v1.1.0 (正式リリース)
```

### ブランチの種別と命名規則

| ブランチ種別 | 命名プレフィックス | 説明・用途 |
| :--- | :--- | :--- |
| **メインブランチ** | `main` | **保護ブランチ**。常にテストおよび `esphome config` 検証が通り稼働可能な安定版。直接 push 禁止。 |
| **新機能開発** | `feat/<機能名>` | 新しい機能やエンティティの追加（例: `feat/rtc-sync`, `feat/schedule-entity`） |
| **バグ修正** | `fix/<バグ概要>` | 不具合の修正（例: `fix/iv-index-recovery`, `fix/scene-fade-cooldown`） |
| **検証・テスト** | `test/<検証対象>` | ガードレールテストや検証スイートの追加（例: `test/pr-quality-guardrails`） |
| **リファクタリング** | `refactor/<対象>` | 動作を変えないコード構造改善（例: `refactor/tx-pacing`） |
| **ドキュメント** | `docs/<内容>` | ドキュメントの加筆・修正（例: `docs/troubleshooting-guide`） |
| **雑務・CI設定** | `chore/<内容>` | CI 設定やビルド補助の更新（例: `chore/bump-version-1.1.0`） |
| **リリースブランチ** | `release/v<バージョン>` | **保護ブランチ**。大規模リリース前の検証・安定化専用 |

---

## 3. GitHub 保護ルール (Rulesets) と権限設計

コントリビューターが安全に作業できるよう、GitHub 上で **Rulesets（保護ルール）** が有効化されています。

### コントリビューターができること（許可事項）
- 任意のトピックブランチ（`feat/*`, `fix/*`, `test/*` 等）の作成、コミット、プッシュ
- Issue（課題・要望・バグ報告）の作成とディスカッション
- `main` ブランチへ向けた Pull Request（PR）の作成

### 保護されていること（禁止・制限事項）
1. **`main` および `release/*` ブランチへの直接 push の禁止**:
   - `git push origin main` は拒否されます。変更は必ず Pull Request を経由してください。
   - `main` ブランチの削除や Force Push（`--force`）は禁止されています。
2. **リリースタグ（`v*`）の作成・変更・削除の制限**:
   - `v*` 形式のタグ作成・更新・削除はリポジトリ管理者（オーナー）のみに制限されています。

---

## 4. 開発・コントリビュートの手順 (ワークフロー)

### ステップ 1: トピックブランチの作成
必ず最新の `main` からブランチを派生させます：

```bash
git checkout main
git pull origin main
git checkout -b feat/your-feature-name
```

### ステップ 2: ローカル開発と検証
コードやドキュメントを変更した後は、**必ず以下のガードレールテストおよび ESPHome 設定検証が成功すること** を確認してください：

```bash
# 1. PR 品質・セキュリティ・法規・アーキテクチャガードレールテストの実行
python3 -m unittest discover -s tests -v

# 2. 静的コード解析 (Ruff が利用可能な場合)
ruff check components tests
ruff format --check components tests

# 3. secrets.yaml が未作成の場合はテンプレートから作成
cp -n secrets.yaml.example secrets.yaml

# 4. ESPHome 構文・スキーマ・コード生成検証
esphome config daiko_controller.yaml
```

### ステップ 3: コミットメッセージ規約 (Conventional Commits)
コミットメッセージは変更内容が把握しやすいよう、プレフィックスを付けて記述してください：

- `feat: <新機能の概要>`
- `fix: <バグ修正の概要>`
- `refactor: <リファクタリングの概要>`
- `test: <テストの追加・修正>`
- `docs: <ドキュメントの更新>`
- `chore: <CI設定や雑務>`

### ステップ 4: Pull Request の作成
ブランチを GitHub へプッシュし、Pull Request を作成します：

```bash
git push -u origin feat/your-feature-name
```

#### Pull Request 作成時の確認項目
- [ ] 変更内容の概要と目的が記載されているか
- [ ] `python3 -m unittest discover -s tests -v` が全件 PASS しているか
- [ ] `esphome config daiko_controller.yaml` がエラーなく成功するか
- [ ] BLE Mesh の重要設計制約（IV Index 自動追従、ループバックエコー除外、有界アドレスローテーション、二重状態制御）を破壊していないか
- [ ] 機密情報（実際の `NetKey` / `AppKey`、Wi-Fi パスワード、プライベート IP、個人環境パス）が含まれていないか
- [ ] 第三者アプリのバイナリ・リソースや、リバースエンジニアリング・逆アセンブルを示唆する表記が含まれていないか
- [ ] ドキュメント・テンプレートに絵文字を使用していないか

### ステップ 5: レビュー & マージ
- 自動 CI およびコードレビューによる確認後、`Squash and merge` または `Merge pull request` で `main` に取り込まれます。
- マージ完了後、作業ブランチは削除します。

---

## 5. DP-41924 実機検証チェックリスト

実機（ESP32 + DAIKO `DP-41924`）で動作検証を行う際は、以下の項目を確認してください：

1. **起動時 NVS 復元と初期同期 (`[DAIKO_BOOT]`)**:
   - 再起動後 15 秒以内に `CtrlInfo`（`51 01`）および `LevelInfo`（`51 02`）が受信され、Home Assistant 上の `DAIKO 親機オンライン (Connected)` が `on` になること。
2. **調光・調色操作とスナップバック抑止**:
   - スライダーを連続操作した際、330ms レート制限により遅延や取りこぼしなく追従し、操作直後にスライダーが古い値へ跳ね戻らないこと。
3. **シーン切替時の消灯回路復帰**:
   - 特定回路が消灯（`0xFF`）に設定されたシーンから、全回路点灯シーンへ切り替えた際、消灯していた回路も確実に点灯復帰すること。
4. **主電源 OFF からの単独排他点灯 (`Exclusive ON`)**:
   - 主電源スイッチを OFF にした状態から任意の 1 回路のみを ON にした際、他の回路が誤点灯せず指定回路のみが点灯すること。
5. **ループバック遮断とヘルス監視**:
   - グループアドレス（`0xC078`）宛てのコマンド送信時に、自身の送信パケット（`[RX_LOCAL]`）で `last_rx_ms_` が誤更新されないこと。
