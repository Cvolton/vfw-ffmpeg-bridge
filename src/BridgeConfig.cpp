#include "BridgeConfig.hpp"
#include "CodecState.hpp"

#include <string_view>
#include <string>
#include "resource.h"

static constexpr const wchar_t* defaultEncoders[] = {
    L"h264_nvenc", L"h264_amf", L"h264_qsv", L"h264_vaapi", 
    L"hevc_nvenc", L"hevc_amf", L"hevc_qsv", L"hevc_vaapi", 
    L"libx264", L"libx265", L"Other"
};

static constexpr const wchar_t* qsvPresets[] = { L"veryfast", L"faster", L"fast", L"medium", L"slow", L"slower", L"veryslow" };
static constexpr const wchar_t* amfPresets[] = { L"quality", L"balanced", L"speed" };
static constexpr const wchar_t* nvencPresets[] = { L"p1", L"p2", L"p3", L"p4", L"p5", L"p6", L"p7" };
static constexpr const wchar_t* x264Presets[] = { L"ultrafast", L"superfast", L"veryfast", L"faster", L"fast", L"medium", L"slow", L"slower", L"veryslow" };

static constexpr const wchar_t* nvencTunes[] = { L"(none)", L"ull", L"ll", L"hq", L"llhq", L"lossless" };
static constexpr const wchar_t* x264Tunes[] = { L"(none)", L"film", L"animation", L"grain", L"stillimage", L"psnr", L"ssim", L"fastdecode", L"zerolatency" };

static constexpr const wchar_t* pixelFormats[] = { L"yuv420p", L"yuv422p", L"yuv444p", L"nv12", L"nv16", L"nv21", L"p010le" };

void UpdatePresetTuneUI(HWND hwndDlg, std::wstring_view encoder, const std::wstring& savedPreset, const std::wstring& savedTune) {
    HWND hPreset = GetDlgItem(hwndDlg, IDC_COMBO_PRESET);
    HWND hTune = GetDlgItem(hwndDlg, IDC_COMBO_TUNE);

    SendMessageW(hPreset, CB_RESETCONTENT, 0, 0);
    SendMessageW(hTune, CB_RESETCONTENT, 0, 0);

    auto PopulateCombo = [](HWND hCombo, const wchar_t* const* items, size_t count, const std::wstring& target, const wchar_t* fallback) {
        for (size_t i = 0; i < count; ++i) {
            SendMessageW(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(items[i]));
        }
        if (target.empty() || SendMessageW(hCombo, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(target.c_str())) == CB_ERR) {
            SendMessageW(hCombo, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(fallback));
        }
    };

    if (encoder == L"Other") {
        EnableWindow(hPreset, FALSE);
        EnableWindow(hTune, FALSE);
        return;
    }

    EnableWindow(hPreset, TRUE);

    if (encoder.find(L"qsv") != std::wstring_view::npos) {
        PopulateCombo(hPreset, qsvPresets, std::size(qsvPresets), savedPreset, L"medium");
        EnableWindow(hTune, FALSE);
    } 
    else if (encoder.find(L"amf") != std::wstring_view::npos) {
        PopulateCombo(hPreset, amfPresets, std::size(amfPresets), savedPreset, L"balanced");
        EnableWindow(hTune, FALSE);
    } 
    else if (encoder.find(L"nvenc") != std::wstring_view::npos) {
        PopulateCombo(hPreset, nvencPresets, std::size(nvencPresets), savedPreset, L"p4");
        EnableWindow(hTune, TRUE);
        PopulateCombo(hTune, nvencTunes, std::size(nvencTunes), savedTune, L"(none)");
    } 
    else if (encoder.find(L"x264") != std::wstring_view::npos || encoder.find(L"x265") != std::wstring_view::npos) {
        PopulateCombo(hPreset, x264Presets, std::size(x264Presets), savedPreset, L"medium");
        EnableWindow(hTune, TRUE);
        PopulateCombo(hTune, x264Tunes, std::size(x264Tunes), savedTune, L"(none)");
    } 
    else {
        EnableWindow(hPreset, FALSE);
        EnableWindow(hTune, FALSE);
    }
}

INT_PTR CALLBACK BridgeConfig::ConfigDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            CodecState* state = reinterpret_cast<CodecState*>(lParam);
            SetWindowLongPtr(hwndDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
            
            SetDlgItemTextW(hwndDlg, IDC_EDIT_EXTRA_ARGS, state->extra_args.c_str());
            SetDlgItemTextW(hwndDlg, IDC_EDIT_LOC_PATH, state->path.c_str());
            
            CheckRadioButton(hwndDlg, IDC_RADIO_AUTO, IDC_RADIO_CUSTOM, IDC_RADIO_CUSTOM);
            CheckRadioButton(hwndDlg, IDC_RADIO_LOC_ASK, IDC_RADIO_LOC_OTHER, IDC_RADIO_LOC_OTHER);
            
            // Pixel formats
            HWND hPixFmtCombo = GetDlgItem(hwndDlg, IDC_COMBO_PIXFMT);
            for (const auto& pixFmt : pixelFormats) {
                SendMessageW(hPixFmtCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(pixFmt));
            }
            SendMessageW(hPixFmtCombo, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(L"yuv420p"));
            
            // Encoder
            HWND hEncoderCombo = GetDlgItem(hwndDlg, IDC_COMBO_ENCODER);
            HWND hEncoderCustom = GetDlgItem(hwndDlg, IDC_EDIT_ENC_CUSTOM);
            
            int matchIndex = -1;
            for (int i = 0; i < std::size(defaultEncoders); ++i) {
                SendMessageW(hEncoderCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(defaultEncoders[i]));
                if (state->codec == defaultEncoders[i]) {
                    matchIndex = i;
                }
            }
            
            // Defaults
            std::wstring_view activeEncoderUI;
            if (matchIndex != -1 && matchIndex != (std::size(defaultEncoders) - 1)) {
                SendMessageW(hEncoderCombo, CB_SETCURSEL, matchIndex, 0);
                activeEncoderUI = defaultEncoders[matchIndex];
                EnableWindow(hEncoderCustom, FALSE);
            } else {
                SendMessageW(hEncoderCombo, CB_SETCURSEL, std::size(defaultEncoders) - 1, 0); 
                EnableWindow(hEncoderCustom, TRUE); 
                SetWindowTextW(hEncoderCustom, state->codec.c_str()); 
                activeEncoderUI = L"Other";
            }
            
            UpdatePresetTuneUI(hwndDlg, activeEncoderUI, state->preset, state->tune);
            
            return (INT_PTR)TRUE;
        }

        case WM_COMMAND: {
            CodecState* state = reinterpret_cast<CodecState*>(GetWindowLongPtr(hwndDlg, DWLP_USER));
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmId == IDC_COMBO_ENCODER && wmEvent == CBN_SELCHANGE) {
                HWND hEncoderCombo = GetDlgItem(hwndDlg, IDC_COMBO_ENCODER);
                HWND hEncoderCustom = GetDlgItem(hwndDlg, IDC_EDIT_ENC_CUSTOM);
                
                int selIndex = SendMessageW(hEncoderCombo, CB_GETCURSEL, 0, 0);
                wchar_t buffer[64];
                SendMessageW(hEncoderCombo, CB_GETLBTEXT, selIndex, reinterpret_cast<LPARAM>(buffer));

                std::wstring_view bufferView(buffer);
                
                if (bufferView == L"Other") {
                    EnableWindow(hEncoderCustom, TRUE);
                    SetFocus(hEncoderCustom);
                } else {
                    EnableWindow(hEncoderCustom, FALSE);
                }
                
                UpdatePresetTuneUI(hwndDlg, bufferView, L"", L"");
                
                return (INT_PTR)TRUE;
            }

            switch (wmId) {
                case IDOK: {
                    wchar_t buffer[512];
                    
                    GetDlgItemTextW(hwndDlg, IDC_EDIT_EXTRA_ARGS, buffer, _countof(buffer));
                    state->extra_args = buffer;
                    
                    GetDlgItemTextW(hwndDlg, IDC_EDIT_LOC_PATH, buffer, _countof(buffer));
                    state->path = buffer;
                    
                    HWND hEncoderCombo = GetDlgItem(hwndDlg, IDC_COMBO_ENCODER);
                    int selIndex = SendMessageW(hEncoderCombo, CB_GETCURSEL, 0, 0);
                    SendMessageW(hEncoderCombo, CB_GETLBTEXT, selIndex, reinterpret_cast<LPARAM>(buffer));

                    if (std::wstring_view(buffer) == L"Other") {
                        GetDlgItemTextW(hwndDlg, IDC_EDIT_ENC_CUSTOM, buffer, _countof(buffer));
                        state->codec = buffer;
                    } else {
                        state->codec = buffer;
                    }
                    
                    HWND hPresetCombo = GetDlgItem(hwndDlg, IDC_COMBO_PRESET);
                    if (IsWindowEnabled(hPresetCombo)) {
                        selIndex = SendMessageW(hPresetCombo, CB_GETCURSEL, 0, 0);
                        if (selIndex != CB_ERR) {
                            SendMessageW(hPresetCombo, CB_GETLBTEXT, selIndex, reinterpret_cast<LPARAM>(buffer));
                            state->preset = buffer;
                        }
                    } else {
                        state->preset.clear();
                    }

                    HWND hTuneCombo = GetDlgItem(hwndDlg, IDC_COMBO_TUNE);
                    if (IsWindowEnabled(hTuneCombo)) {
                        selIndex = SendMessageW(hTuneCombo, CB_GETCURSEL, 0, 0);
                        if (selIndex != CB_ERR) {
                            SendMessageW(hTuneCombo, CB_GETLBTEXT, selIndex, reinterpret_cast<LPARAM>(buffer));
                            state->tune = buffer;
                        }
                    } else {
                        state->tune.clear();
                    }

                    HWND hPixFmtCombo = GetDlgItem(hwndDlg, IDC_COMBO_PIXFMT);
                    selIndex = SendMessageW(hPixFmtCombo, CB_GETCURSEL, 0, 0);
                    if (selIndex != CB_ERR) {
                        SendMessageW(hPixFmtCombo, CB_GETLBTEXT, selIndex, reinterpret_cast<LPARAM>(buffer));
                        state->pix_fmt = buffer;
                    }
                    
                    EndDialog(hwndDlg, IDOK);
                    return (INT_PTR)TRUE;
                }
                case IDCANCEL:
                    EndDialog(hwndDlg, IDCANCEL);
                    return (INT_PTR)TRUE;
            }
            break;
        }
    }
    return (INT_PTR)FALSE;
}