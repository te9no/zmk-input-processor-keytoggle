# zmk-input-processor-keytoggle

## 概要

`zmk,input-processor-keytoggle` は、特定の入力イベント（例: マウスの移動）が検出されている間だけ、指定されたキーを押し続けるInput Processorです。入力イベントが一定時間途絶えると、キーは自動的にリリースされます。これにより、センサーの動きに連動してキーの押下状態を制御するような高度な機能を実現できます。

## 互換性

`zmk,input-processor-keytoggle`

## プロパティ

- `keycodes`: (必須) センサーがアクティブな間、押下状態にするキーコードを指定します。ZMKで定義されている任意のキーコード（例: `KC_BTN1`, `KC_BTN2`, `KC_BTN3`, `LS(MCLK)`など）を使用できます。

## 使用例

以下は、`tlx493d_listener`というInput Listenerと組み合わせて `zip_keytoggle` Input Processorを使用する例です。この設定では、`tlx493d_listener`が移動を検知している間、マウスの中央ボタン（`MCLK`）が押下状態になります。

```dts
/ {
    tlx493d_listener: tlx493d_listener {
        compatible = "zmk,input-listener";
        status = "okay";
        device = <&magnet>;

        move {
            layers = <0>;
            input-processors = <&zip_keytoggle>, <&zip_xy_scaler 1 8>;
        };
    };
};

/ {
    input_processors {
        zip_keytoggle: zip_keytoggle {
            compatible = "zmk,input-processor-keytoggle";
            #input-processor-cells = <0>;
            keycode = <KC_BTN3>; // マウス中ボタンを例として指定
        };
    };
};
```

### 設定の解説

- `tlx493d_listener`: センサーからの入力イベントをリッスンするInput Listenerのノードです。
- `move`: `tlx493d_listener` の子ノードで、移動イベントが発生した際に適用される設定を定義します。
- `layers = <0>`: このInput Processorがレイヤー0でのみ有効になることを示します。
- `input-processors = <&zip_keytoggle>, <&zip_xy_scaler 1 8>`: `zip_keytoggle` と `zip_xy_scaler` の両方のInput Processorが、センサーからの入力イベントを処理するように指定しています。
- `zip_keytoggle`: `zmk,input-processor-keytoggle` と互換性のあるInput Processorのインスタンスです。
- `keycodes = <KC_BTN3>`: このInput Processorが、センサーがアクティブな間、`KC_BTN3`（マウス中ボタン）を押し続けるように設定されています。
- `#input-processor-cells = <0>`: このInput Processorがデバイスツリーでインスタンス化される際に、追加のパラメータを必要としないことを示します。`keycodes` はプロパティとして直接指定されます。

## 動作原理

`zmk,input-processor-keytoggle` は、以下のロジックで動作します。

1. **移動イベントの検出**: センサーから `INPUT_EV_REL` タイプのイベント（相対移動イベント）がZMKのInputサブシステムに報告されると、`keytoggle_handle_event` 関数が呼び出されます。
2. **キーの押下**: `keytoggle_handle_event` 関数内で、`is_pressed` フラグが `false` の場合（キーがまだ押されていない場合）、設定された `keycodes` のキープレスイベントがZMKの動作キューに送信され、`is_pressed` フラグが `true` に設定されます。
3. **キーリリースタイマーのリセット**: 移動イベントが検出されるたびに、キーリリース用の遅延ワークアイテム（`key_release_work`）がリセットされます。これにより、移動が続いている間はキーが押下状態に保たれます。
4. **キーのリリース**: 一定時間（デフォルトでは100ミリ秒、`K_MSEC(100)`で設定）移動イベントが検出されない場合、`key_release_work` が実行されます。`key_release_callback` 関数内で `is_pressed` フラグが `true` の場合、設定された `keycodes` のキーリリースイベントがZMKの動作キューに送信され、`is_pressed` フラグが `false` に設定されます。

この仕組みにより、センサーが移動を検知している間だけキーが押下され、移動が停止するとキーがリリースされる動作が実現されます。

## ビルドと有効化

このInput ProcessorをZMKファームウェアに含めるには、以下のKconfigオプションを`config/boards/<your_board>/<your_board>.conf`または`config/boards/<your_board>/<your_board>_defconfig`ファイルに追加する必要があります。

```kconfig
CONFIG_ZMK_INPUT_PROCESSOR_KEYTOGGLE=y
```

また、`app/src/pointing/CMakeLists.txt`に以下の行を追加して、ソースファイルがビルドされるようにする必要があります。

```cmake
target_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_KEYTOGGLE app PRIVATE input_processor_keytoggle.c)
```

## 注意事項

- `key_release_work` の遅延時間は、使用するセンサーやアプリケーションの要件に合わせて調整してください。短すぎると意図しないキーリリースが発生する可能性があり、長すぎるとキーリリースが遅れる可能性があります。
- このInput Processorは、主に `INPUT_EV_REL` タイプのイベント（相対移動）を処理するように設計されています。他のタイプのイベント（例: `INPUT_EV_ABS` などの絶対座標イベント）を処理する場合は、`keytoggle_handle_event` 関数のロジックを適宜変更する必要があります。


