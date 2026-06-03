// MidiInfoAPI.h
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 c30 (zel9278)
// See the LICENSE file (MIT) for terms. This header may be freely copied into
// your own plugin project.
// ---------------------------------------------------------------------------
// MidiInfoObject.aux2 が公開する、外部プラグイン向けの安定 C ABI。
//
// 使い方（外部プラグイン側）:
//   #include "MidiInfoAPI.h"
//   HMODULE h = GetModuleHandleW(L"MidiInfoObject.aux2"); // 同一プロセス内に常駐
//   auto getapi = (MidiInfo_GetAPI_Fn)GetProcAddress(h, MIDIINFO_GETAPI_SYMBOL);
//   const MidiInfoAPI* api = getapi ? getapi(MIDIINFO_API_VERSION) : NULL;
//   if (!api) return; // 互換バージョンが無い
//
//   MidiInfoAnalysis* an = api->acquire(NULL);     // NULL=共有MIDI、または L"path.mid"
//   if (api->is_ok(an)) {
//       double bpm = api->bpm_at(an, 1.0);
//       const MidiInfoNoteSpan* spans; int n = api->note_spans(an, 60, &spans);
//   }
//   api->release(an);                              // 取得したハンドルは必ず解放
//
// 互換方針:
//   - 関数テーブル MidiInfoAPI は「末尾に追加」のみ。並び替え/削除はしない。
//   - 破壊的変更時は MIDIINFO_API_VERSION を上げる。
//   - 旧版を要求された場合、提供側は struct_size/version を見て安全に応答する。
//   - getapi(requested) は requested > 提供版 のとき NULL を返す。
// ---------------------------------------------------------------------------
#ifndef MIDIINFO_API_H
#define MIDIINFO_API_H

#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIDIINFO_API_VERSION 1
#define MIDIINFO_GETAPI_SYMBOL "MidiInfo_GetAPI"

// 1 ノート区間（秒）。内部表現とバイナリ互換（直接ポインタで返す）。
typedef struct MidiInfoNoteSpan {
    float    start;     // 発音時刻（秒）
    float    end;       // 消音時刻（秒）
    uint16_t track;     // トラック番号
    uint8_t  channel;   // 0..15
    uint8_t  _reserved; // パディング
    uint32_t color;     // 0xRRGGBB（色モードに応じた表示色）
    uint32_t seq;       // MIDIファイル上の登場順（同時刻ノートの前後）
} MidiInfoNoteSpan;

// 取得した解析への不透明ハンドル。内部で参照を保持し、release まで有効。
typedef struct MidiInfoAnalysis MidiInfoAnalysis;

// 外部に公開する関数テーブル。末尾追加のみ。
typedef struct MidiInfoAPI {
    uint32_t struct_size;   // = sizeof(MidiInfoAPI)（前方互換チェック用）
    uint32_t version;       // = MIDIINFO_API_VERSION

    // ---- 共有状態 / 色 ----
    // 共有MIDIパス(UTF-16)を buf に最大 cap 文字コピー（NUL終端）。必要文字数(NUL含む)を返す。
    int      (*get_shared_path)(wchar_t* buf, int cap);
    double   (*get_shared_time)(void);              // 共有再生時刻(秒)。無効時は NaN。
    uint32_t (*channel_color)(int channel);         // 0xRRGGBB
    uint32_t (*note_color)(int channel, int track); // 色モード反映の 0xRRGGBB
    int      (*color_mode)(void);                   // 0=Channel/1=Track/2=Channel+Track

    // ---- 解析の取得 / 解放（参照カウント）----
    // path=NULL で共有MIDI。未ロードでも非NULLハンドルを返す（is_ok で判定）。失敗時のみ NULL。
    MidiInfoAnalysis* (*acquire)(const wchar_t* path);
    // 解析完了までブロック（timeout_ms<=0 で内部上限）。エンコード用途向け。
    MidiInfoAnalysis* (*acquire_wait)(const wchar_t* path, int timeout_ms);
    void              (*release)(MidiInfoAnalysis* handle);

    // ---- メタ情報 ----
    int      (*is_ok)(const MidiInfoAnalysis*);              // 1=有効な解析
    double   (*duration)(const MidiInfoAnalysis*);           // 全体長(秒)
    uint16_t (*tpqn)(const MidiInfoAnalysis*);               // 四分音符あたり tick
    uint64_t (*total_notes)(const MidiInfoAnalysis*);
    uint32_t (*max_nps)(const MidiInfoAnalysis*);
    uint32_t (*max_polyphony)(const MidiInfoAnalysis*);
    uint32_t (*max_note_density)(const MidiInfoAnalysis*);
    double   (*first_note_seconds)(const MidiInfoAnalysis*); // 最初のノート(秒)。無ければ +Inf。

    // ---- 時間軸クエリ ----
    double   (*bpm_at)(const MidiInfoAnalysis*, double seconds);
    void     (*signature_at)(const MidiInfoAnalysis*, double seconds, uint8_t* num, uint8_t* den);
    int      (*key_sf_at)(const MidiInfoAnalysis*, double seconds);       // -7..+7（負=♭/正=♯）
    uint64_t (*bar_number_at)(const MidiInfoAnalysis*, double seconds);   // 1始まりの小節番号
    double   (*beat_in_bar_at)(const MidiInfoAnalysis*, double seconds);  // 小節内拍位置
    double   (*quarter_beats_at)(const MidiInfoAnalysis*, double seconds);// 累積四分音符拍
    double   (*seconds_at_beat)(const MidiInfoAnalysis*, double beats);   // 拍→秒

    // ---- カウント / 密度 ----
    uint32_t (*count_at)(const MidiInfoAnalysis*, double seconds, int note_offs); // 累積ノート数
    uint32_t (*nps_at)(const MidiInfoAnalysis*, double seconds);                  // 瞬間NPS

    // ---- ノート ----
    // pitch(0..127) の区間配列を *out に返し、要素数を返す。配列は release まで有効。
    int  (*note_spans)(const MidiInfoAnalysis*, int pitch, const MidiInfoNoteSpan** out);
    // seconds 時点で各鍵に表示する区間添字。span_index_out は要素数128（押下なし -1）。
    void (*active_keys_at)(const MidiInfoAnalysis*, double seconds, int* span_index_out);
    // pitch を t 秒で押下中の最優先区間の添字（channel_filter>=0 でch限定、なし -1）。
    int  (*active_span_at)(const MidiInfoAnalysis*, int pitch, float t, int channel_filter);
} MidiInfoAPI;

// GetProcAddress 用のエントリ関数ポインタ型。
typedef const MidiInfoAPI* (*MidiInfo_GetAPI_Fn)(uint32_t requested_version);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // MIDIINFO_API_H
