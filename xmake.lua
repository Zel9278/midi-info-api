-- midi-info-api / サンプル外部プラグインのビルド（xmake）
--
--   xmake f -p windows -a x64 -m release   (初回設定)
--   xmake build                            (サンプルをビルド)
--
-- AviUtl2 SDK が必要です（filter2.h / plugin2.h）。
--   入手: https://spring-fragrance.mints.ne.jp/aviutl/
--   直接: https://spring-fragrance.mints.ne.jp/aviutl/aviutl2_sdk.zip
-- 環境変数で SDK / プラグイン出力先を指定してください:
--   AVU2_SDK_DIR    … AviUtl2 SDK のパス（filter2.h などがある場所）
--   AVU2_PLUGIN_DIR … ビルド後のコピー先（既定 C:/ProgramData/aviutl2/Plugin）

set_project("midi-info-api")
set_languages("cxx17")
add_rules("mode.debug", "mode.release")
set_defaultmode("release")

-- ソースは UTF-8。MSVC は既定でシステムコードページ解釈なので明示する。
add_cxflags("/utf-8", {tools = "cl"})

local avu2_sdk = os.getenv("AVU2_SDK_DIR") or "avu2-sdk"

target("MidiApiSample")
    set_kind("shared")
    add_defines("NOMINMAX", "_CRT_SECURE_NO_WARNINGS", "UNICODE", "_UNICODE")
    -- "." はリポジトリ直下（MidiInfoAPI.h の場所）
    add_includedirs(avu2_sdk, ".")
    add_files("samples/api-sample/MidiApiSample.cpp")
    add_syslinks("user32", "gdi32")
    set_runtimes("MT")
    after_build(function (target)
        local dst = os.getenv("AVU2_PLUGIN_DIR") or "C:/ProgramData/aviutl2/Plugin"
        local out = path.join(dst, "MidiApiSample.aux2")
        os.trycp(target:targetfile(), out)
        print("copied -> " .. out)
    end)
