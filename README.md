# ParryAction — Gameplay Programming Portfolio

Unreal Engine 5.7.4 / C++ / 個人制作 / Windows向け

敵の攻撃を見極め、パリィ・回避・反撃を使い分ける1対1の近接アクションプロトタイプです。このリポジトリは、**ゲームプレイの実装を読むためのコード資料**です。

> 完全なUnrealプロジェクトではありません。Blueprint、マップ、キャラクター、アニメーション、音声などは含めていないため、このリポジトリだけでゲームを起動・ビルドすることはできません。

## 見どころ

- **攻撃と連続入力:** アニメーション通知に合わせて球状スイープで命中判定。2段目を早く入力した場合は1回保持し、必要な条件を満たした後に実行。
- **パリィ・回避:** パリィ有効時間や回避中の無敵時間を管理。敵を注視中は入力方向に応じた回避アニメーションを選択。
- **危険攻撃:** 敵が前進しながら2回攻撃。初撃のパリィ後、ランダムなWASD 4キーを制限時間内に入力する。成功時は敵のひるみによる反撃機会、失敗・時間切れ時はダメージが発生。
- **結果と記録:** 勝敗、再挑戦、経過時間、勝利時の最速タイム保存を実装。

## コードの案内

| ファイル | 内容 |
| --- | --- |
| [ParryActionCharacter.cpp](Source/ParryAction/ParryActionCharacter.cpp) / [ヘッダー](Source/ParryAction/ParryActionCharacter.h) | 移動入力、攻撃・パリィ・回避、プレイヤーの体力 |
| [AnimNotify_AttackHit.cpp](Source/ParryAction/AnimNotify_AttackHit.cpp) | アニメーション通知から攻撃判定を呼び出す処理 |
| [TrainingEnemy.cpp](Source/ParryAction/TrainingEnemy.cpp) / [ヘッダー](Source/ParryAction/TrainingEnemy.h) | 敵の追跡・攻撃予告・2連続攻撃・ひるみ・体力 |
| [ParryActionPlayerController.cpp](Source/ParryAction/ParryActionPlayerController.cpp) | プレイヤーコントローラー、メニュー入力、強攻撃パリィ後のキー入力、記録管理 |
| [CombatHUDWidget.cpp](Source/ParryAction/CombatHUDWidget.cpp) | 戦闘状態、攻撃予告、入力チャレンジ、結果画面の表示 |
| [ParryActionPlayerTests.cpp](Source/ParryAction/ParryActionPlayerTests.cpp), [TrainingEnemyCombatTests.cpp](Source/ParryAction/TrainingEnemyCombatTests.cpp) | 戦闘ルールのUnreal Automation Test |

### 攻撃処理の流れ

```text
攻撃入力 → アニメーション再生 → AnimNotify → 球状スイープ → ダメージ・演出
                  └─ 早い2段目入力は最大1回保持 ──┘
```

元のゲームではBlueprintを主にC++クラスの継承、入力・アニメーション・エフェクトなどの参照設定に使用しています。この公開資料にBlueprintは含めていません。

## 確認した範囲と制約

- 制作者が移動・攻撃・パリィ・回避・勝敗などを手動でプレイ確認しました。元の完全なプロジェクトでは、一部の戦闘・記録ルールを自動テストしています。
- Windows Developmentビルドは開発PCで実行確認済みです。別PCでの実行とShippingビルドは未確認です。上記のテストやビルドを、このコード資料単体で再実行したものではありません。
- 敵の追跡は平坦な地形を前提とした簡易処理で、NavMeshによる障害物の迂回はありません。敵攻撃の当たり判定はタイマー基準です。

## 制作とAIの利用

制作者がゲームの方針・仕様を決め、AIが作成したコードの理解を進めながら改善点と修正方針を考え、プレイ検証を行いました。C++コードの作成・修正、Unreal Editorの設定や開発記録の整理にはOpenAI Codexを活用しています。

元のゲームはUnreal Engineのテンプレートを出発点とし、一部のファイルにはEpic Gamesの著作権表示が残っています。キャラクター、アニメーション、音楽など外部素材を使用していますが、この公開用コード資料には同梱していません。
