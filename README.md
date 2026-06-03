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

## 互換方針

- 関数テーブル `MidiInfoAPI` は **末尾追加のみ**（並び替え・削除はしない）。
- 破壊的変更時に `MIDIINFO_API_VERSION` を上げます。
- `getapi(requested)` は `requested` が提供側より新しいとき `NULL` を返します。
- 利用側は `version` / `struct_size` を見て機能の有無を判定できます。

## ライセンス

MIT License. 詳細は [LICENSE](./LICENSE) を参照してください。
