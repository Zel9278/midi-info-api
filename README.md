# MidiInfoAPI

`MidiInfoObject.aux2`（AviUtl2 用 MIDI 情報プラグイン）が公開する、外部プラグイン向けの安定 C ABI ヘッダです。このリポジトリは **ヘッダ 1 ファイルのみ**を MIT ライセンスで配布します。

他のプラグインから、プラグインが読み込んだ MIDI 解析結果（BPM・拍子・小節・ノート区間・チャンネル色・共有再生時刻など）へアクセスできます。

## 使い方

`MidiInfoAPI.h` を自分のプロジェクトにコピー（またはサブモジュールで取り込み）して include します。リンクは不要で、実行時に `GetProcAddress` で解決します。

```c
#include "MidiInfoAPI.h"

HMODULE h = GetModuleHandleW(L"MidiInfoObject.aux2");   // 同一プロセスに常駐
MidiInfo_GetAPI_Fn getapi =
    (MidiInfo_GetAPI_Fn)GetProcAddress(h, MIDIINFO_GETAPI_SYMBOL);
const MidiInfoAPI* api = getapi ? getapi(MIDIINFO_API_VERSION) : NULL;
if (!api) return; // 本体プラグインが無い / 互換バージョンが無い

MidiInfoAnalysis* an = api->acquire(NULL);   // NULL=共有MIDI、または L"path.mid"
if (api->is_ok(an)) {
    double bpm = api->bpm_at(an, 1.0);
    const MidiInfoNoteSpan* spans;
    int n = api->note_spans(an, 60, &spans); // C4 の区間配列
}
api->release(an);                            // 必ず解放
```

## サンプル

`samples/api-sample/MidiApiSample.cpp` は、この API を使う独立した AviUtl2 プラグイン（`.aux2`）の最小例です。共有 MIDI から BPM・拍子・小節・ノート数などを取得して表示します。

### ビルド

[xmake](https://xmake.io) と AviUtl2 SDK（`filter2.h` / `plugin2.h`）が必要です。

```bash
# SDK と出力先を環境変数で指定
set AVU2_SDK_DIR=C:\path\to\avu2-sdk
set AVU2_PLUGIN_DIR=C:\ProgramData\aviutl2\Plugin   # 省略可

xmake f -p windows -a x64 -m release
xmake build
```

ビルドすると `MidiApiSample.aux2` が生成され、`AVU2_PLUGIN_DIR` にコピーされます。本体 `MidiInfoObject.aux2` と一緒に AviUtl2 へ入れ、`MIDI Source` で MIDI を読み込むと値が表示されます。

## 互換方針

- 関数テーブル `MidiInfoAPI` は **末尾追加のみ**（並び替え・削除はしない）。
- 破壊的変更時に `MIDIINFO_API_VERSION` を上げます。
- `getapi(requested)` は `requested` が提供側より新しいとき `NULL` を返します。
- 利用側は `version` / `struct_size` を見て機能の有無を判定できます。

## ライセンス

MIT License. 詳細は [LICENSE](./LICENSE) を参照してください。
