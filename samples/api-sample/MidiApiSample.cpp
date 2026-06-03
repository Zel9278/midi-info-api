// MidiApiSample.cpp
// ---------------------------------------------------------------------------
// MidiInfoObject.aux2 が公開する C ABI（MidiInfoAPI.h）を使う、独立した
// サンプル外部プラグイン（.aux2）。
//
// この1ファイル + AviUtl2 SDK + MidiInfoAPI.h だけで完結する（コア非依存）。
// 共有MIDI（MIDI Source オブジェクトが読み込んだもの）から BPM・拍子・小節・
// ノート数などを取得してテキスト表示する。
// ---------------------------------------------------------------------------
#include <windows.h>

#include <cstring>
#include <string>
#include <vector>

#include "filter2.h"
#include "plugin2.h"
#include "MidiInfoAPI.h"   // include dir に src を追加してある

namespace {

// MidiInfoObject.aux2 の API を取得（初回のみ解決してキャッシュ）。
// 同一プロセスに常駐しているはずなので GetModuleHandle で十分。
const MidiInfoAPI* get_api() {
    static const MidiInfoAPI* api = [] () -> const MidiInfoAPI* {
        HMODULE h = GetModuleHandleW(L"MidiInfoObject.aux2");
        if (!h) return nullptr;
        auto fn = reinterpret_cast<MidiInfo_GetAPI_Fn>(GetProcAddress(h, MIDIINFO_GETAPI_SYMBOL));
        return fn ? fn(MIDIINFO_API_VERSION) : nullptr;
    }();
    return api;
}

// GDI でテキストを PIXEL_RGBA バッファへ描画。
// 白文字で描き、その輝度をアルファに、色は指定色へ置き換える（アンチエイリアス保持）。
void render_text(std::vector<PIXEL_RGBA>& px, int w, int h, const std::wstring& text,
                 int font_px, PIXEL_RGBA color) {
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp) { DeleteDC(dc); return; }
    HGDIOBJ oldbmp = SelectObject(dc, bmp);
    std::memset(bits, 0, size_t(w) * size_t(h) * 4); // 背景 = 黒(=透明)

    HFONT font = CreateFontW(font_px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE, L"Consolas");
    HGDIOBJ oldfont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    RECT rc{ 10, 10, w - 10, h - 10 };
    DrawTextW(dc, text.c_str(), -1, &rc, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK);
    GdiFlush();

    const uint8_t* src = static_cast<const uint8_t*>(bits);
    for (int i = 0; i < w * h; ++i) {
        uint8_t lum = src[size_t(i) * 4]; // B チャンネル（白文字なので輝度=アルファ）
        PIXEL_RGBA& o = px[size_t(i)];
        o.r = color.r; o.g = color.g; o.b = color.b; o.a = lum;
    }

    SelectObject(dc, oldfont); DeleteObject(font);
    SelectObject(dc, oldbmp); DeleteObject(bmp);
    DeleteDC(dc);
}

FILTER_ITEM_TRACK width_item(L"Width", 640, 16, 3840, 1);
FILTER_ITEM_TRACK height_item(L"Height", 220, 16, 2160, 1);
FILTER_ITEM_TRACK font_item(L"Font Size", 28, 6, 200, 1);
FILTER_ITEM_COLOR color_item(L"Color", 0x00ff66);

void* items[] = {
    &width_item,
    &height_item,
    &font_item,
    &color_item,
    nullptr,
};

FILTER_PLUGIN_TABLE plugin_table = {
    FILTER_PLUGIN_TABLE::FLAG_VIDEO | FILTER_PLUGIN_TABLE::FLAG_INPUT,
    L"MIDI API Sample ",
    L"MIDI Info",
    L"Sample: reads MidiInfoObject's shared MIDI via the C API",
    items,
    nullptr,
    nullptr,
};

bool func_proc_video(FILTER_PROC_VIDEO* video) {
    try {
        if (!video) return false;
        int w = int(width_item.value);  if (w < 16) w = 16;
        int h = int(height_item.value); if (h < 16) h = 16;
        int font_px = int(font_item.value);
        PIXEL_RGBA col{ color_item.value.r, color_item.value.g, color_item.value.b, 255 };
        std::vector<PIXEL_RGBA> px(size_t(w) * size_t(h), PIXEL_RGBA{0, 0, 0, 0});

        const MidiInfoAPI* api = get_api();
        std::wstring text;
        if (!api) {
            text = L"MidiInfoObject API not found.\n"
                   L"(MidiInfoObject.aux2 must be installed and loaded)";
        } else {
            // 共有再生時刻（MIDI Source に同期）。無効ならオブジェクト時刻。
            double t = api->get_shared_time();
            if (!(t == t)) t = video->object ? video->object->time : 0.0; // NaN なら fallback

            MidiInfoAnalysis* an = api->acquire(nullptr); // 共有MIDI
            if (api->is_ok(an)) {
                uint8_t num = 4, den = 4;
                api->signature_at(an, t, &num, &den);
                wchar_t pathbuf[260] = L"";
                api->get_shared_path(pathbuf, 260);
                wchar_t buf[1024];
                swprintf(buf, 1024,
                    L"MIDI API Sample (v%u)\n"
                    L"path : %s\n"
                    L"time : %.2f / %.2f s\n"
                    L"BPM  : %.2f\n"
                    L"meter: %u/%u   bar %llu\n"
                    L"notes: %u / %llu\n"
                    L"NPS  : %u   maxPoly: %u",
                    (unsigned)api->version, pathbuf,
                    t, api->duration(an), api->bpm_at(an, t),
                    (unsigned)num, (unsigned)den,
                    (unsigned long long)api->bar_number_at(an, t),
                    api->count_at(an, t, 0),
                    (unsigned long long)api->total_notes(an),
                    api->nps_at(an, t), api->max_polyphony(an));
                text = buf;
            } else {
                text = L"MIDI API Sample\n"
                       L"(no shared MIDI / loading)\n"
                       L"Place a 'MIDI Source' object and load a .mid.";
            }
            api->release(an); // 取得したハンドルは必ず解放
        }

        render_text(px, w, h, text, font_px, col);

        if (video->create_image_resource)
            video->create_image_resource(L"object", px.data(), w, h);
        else
            video->set_image_data(px.data(), w, h);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

// --- プラグイン登録（独立した .aux2 としての定型エクスポート） ---
static COMMON_PLUGIN_TABLE common_plugin_table = {
    L"MIDI API Sample",
    L"Reads MidiInfoObject shared MIDI via the public C API",
};

EXTERN_C __declspec(dllexport) COMMON_PLUGIN_TABLE* GetCommonPluginTable(void) {
    return &common_plugin_table;
}

EXTERN_C __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host) {
    if (!host) return;
    plugin_table.func_proc_video = func_proc_video;
    host->register_filter_plugin(&plugin_table);
}

EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD version) {
    (void)version;
    return true;
}

EXTERN_C __declspec(dllexport) void UninitializePlugin() {}

EXTERN_C __declspec(dllexport) FILTER_PLUGIN_TABLE* GetFilterPluginTable(void) {
    return nullptr;
}
