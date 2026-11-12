## 概要 (Summary)
<!-- 変更内容の目的と概要を簡潔に記述してください -->

## 関連 Issue (Related Issues)
<!-- 例: Closes #1 -->

## 変更内容 (Changes)
- 

## 検証・確認項目 (Checklist)
- [ ] `python3 -m unittest discover -s tests -v`（ガードレール・セキュリティ・法規監査テスト）がエラーなく通っているか
- [ ] `esphome config daiko_controller.yaml` がエラーなく通っているか
- [ ] BLE Mesh 重要制約（IV Index 追従、`CONFIG_BLE_MESH_IVU_RECOVERY_IVI: y`、ループバックエコー除外、有界アドレスローテーション、二重状態制御）を維持しているか
- [ ] 機密情報（実機の `NetKey` / `AppKey`、パスワード、固定プライベート IP、ローカルパス）が含まれていないか
- [ ] 第三者アプリのリソースや、リバースエンジニアリング・逆アセンブルを示唆する記述が含まれていないか
- [ ] ドキュメントやコメントに絵文字が含まれていないか
