#include "BridgeConfig.hpp"
#include "CodecState.hpp"
#include "CodecSets.hpp"

#include <ShObjIdl_core.h>
#include <string_view>
#include <string>
#include <vector>
#include "resource.h"

void PopulateCombo(HWND hCombo, const wchar_t* const* items, size_t count, const std::wstring& target, const wchar_t* fallback) {
    for (size_t i = 0; i < count; ++i) {
        SendMessageW(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(items[i]));
    }
    if (target.empty() || SendMessageW(hCombo, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(target.c_str())) == CB_ERR) {
        SendMessageW(hCombo, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(fallback));
    }
};

void ApplyDefaultQualityValues(HWND hwndDlg) {
    HWND hQualCombo = GetDlgItem(hwndDlg, IDC_COMBO_QUAL_MODE);
    
    wchar_t buffer[32];
    int selIndex = SendMessageW(hQualCombo, CB_GETCURSEL, 0, 0);
    if (selIndex == CB_ERR) return;
    
    SendMessageW(hQualCombo, CB_GETLBTEXT, selIndex, reinterpret_cast<LPARAM>(buffer));
    
    QualityMode mode = QualityUtils::GetQualityModeFromString(buffer);
    QualityDefaults defs = QualityUtils::GetDefaultQualityForMode(mode);
    
    SetDlgItemInt(hwndDlg, IDC_EDIT_QUAL_VAL1, defs.first, FALSE);
    SetDlgItemInt(hwndDlg, IDC_EDIT_QUAL_VAL2, defs.second, FALSE);
}

void UpdateQualityInputs(HWND hwndDlg) {
    HWND hQualCombo = GetDlgItem(hwndDlg, IDC_COMBO_QUAL_MODE);
    HWND hVal1 = GetDlgItem(hwndDlg, IDC_EDIT_QUAL_VAL1);
    HWND hVal2 = GetDlgItem(hwndDlg, IDC_EDIT_QUAL_VAL2);

    wchar_t buffer[32];
    int selIndex = SendMessageW(hQualCombo, CB_GETCURSEL, 0, 0);
    if (selIndex == CB_ERR) {
        EnableWindow(hVal1, FALSE);
        EnableWindow(hVal2, FALSE);
        return;
    }
    SendMessageW(hQualCombo, CB_GETLBTEXT, selIndex, reinterpret_cast<LPARAM>(buffer));
    std::wstring_view mode(buffer);

    if (mode == L"Lossless" || mode == L"") {
        EnableWindow(hVal1, FALSE);
        EnableWindow(hVal2, FALSE);
    } else if (mode == L"VBR" || mode == L"VBR_LAT" || mode == L"QVBR" || mode == L"HQVBR") {
        EnableWindow(hVal1, TRUE);
        EnableWindow(hVal2, TRUE);
    } else {
        EnableWindow(hVal1, TRUE);
        EnableWindow(hVal2, FALSE);
    }
}

void UpdateQualityUI(HWND hwndDlg, std::wstring_view encoder, const std::wstring& savedMode, bool resetValues) {
    HWND hQualCombo = GetDlgItem(hwndDlg, IDC_COMBO_QUAL_MODE);
    SendMessageW(hQualCombo, CB_RESETCONTENT, 0, 0);

    const CodecSets::CodecInfo* info = CodecSets::GetCodecInfo(encoder);

    if (!info || info->qualityModes.empty()) {
        EnableWindow(hQualCombo, FALSE);
        UpdateQualityInputs(hwndDlg);
        return;
    }

    EnableWindow(hQualCombo, TRUE);

    std::vector<const wchar_t*> qualNames;
    qualNames.reserve(info->qualityModes.size());
    for (QualityMode mode : info->qualityModes) {
        qualNames.push_back(QualityUtils::GetStringFromQualityMode(mode));
    }

    const wchar_t* fallback = QualityUtils::GetStringFromQualityMode(info->defaultQualityMode);
    PopulateCombo(hQualCombo, qualNames.data(), qualNames.size(), savedMode, fallback);

    UpdateQualityInputs(hwndDlg);
    if (resetValues) {
        ApplyDefaultQualityValues(hwndDlg);
    }
}

void UpdatePresetTuneUI(HWND hwndDlg, std::wstring_view encoder, const std::wstring& savedPreset, const std::wstring& savedTune) {
    HWND hPreset = GetDlgItem(hwndDlg, IDC_COMBO_PRESET);
    HWND hTune = GetDlgItem(hwndDlg, IDC_COMBO_TUNE);

    SendMessageW(hPreset, CB_RESETCONTENT, 0, 0);
    SendMessageW(hTune, CB_RESETCONTENT, 0, 0);

    auto info = CodecSets::GetCodecInfo(encoder);

    if (!info || info->presets.empty()) {
        EnableWindow(hPreset, FALSE);
        EnableWindow(hTune, FALSE);
        return;
    }

    EnableWindow(hPreset, TRUE);
    PopulateCombo(hPreset, info->presets.data(), info->presets.size(), savedPreset, info->defaultPreset);

    if (info->tunes.empty()) {
        EnableWindow(hTune, FALSE);
    } else {
        EnableWindow(hTune, TRUE);
        PopulateCombo(hTune, info->tunes.data(), info->tunes.size(), savedTune, info->defaultTune);
    }
}

void UpdateModeUI(HWND hwndDlg, bool autoMode) {
    HWND hEncoderCombo = GetDlgItem(hwndDlg, IDC_COMBO_ENCODER);
    HWND hEncoderCustom = GetDlgItem(hwndDlg, IDC_EDIT_ENC_CUSTOM);
    HWND hExtraArgs = GetDlgItem(hwndDlg, IDC_EDIT_EXTRA_ARGS);
    HWND hPixFmtCombo = GetDlgItem(hwndDlg, IDC_COMBO_PIXFMT);
    HWND hQualCombo = GetDlgItem(hwndDlg, IDC_COMBO_QUAL_MODE);
    HWND hPresetCombo = GetDlgItem(hwndDlg, IDC_COMBO_PRESET);
    HWND hTuneCombo = GetDlgItem(hwndDlg, IDC_COMBO_TUNE);

    EnableWindow(hEncoderCombo, !autoMode);
    EnableWindow(hExtraArgs, !autoMode);
    EnableWindow(hPixFmtCombo, !autoMode);

    if (autoMode) {
        EnableWindow(hEncoderCustom, FALSE);
        EnableWindow(hQualCombo, FALSE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_QUAL_VAL1), FALSE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_SPIN_QUAL_VAL1), FALSE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_QUAL_VAL2), FALSE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_SPIN_QUAL_VAL2), FALSE);
        EnableWindow(hPresetCombo, FALSE);
        EnableWindow(hTuneCombo, FALSE);
        return;
    }

    wchar_t encoderBuf[64] = {};
    int selIndex = SendMessageW(hEncoderCombo, CB_GETCURSEL, 0, 0);
    if (selIndex != CB_ERR) {
        SendMessageW(hEncoderCombo, CB_GETLBTEXT, selIndex, reinterpret_cast<LPARAM>(encoderBuf));
    }
    std::wstring_view encoderView(encoderBuf);

    EnableWindow(hEncoderCustom, encoderView == CodecSets::CustomEncoderLabel);

    wchar_t presetBuf[64] = {};
    selIndex = SendMessageW(hPresetCombo, CB_GETCURSEL, 0, 0);
    if (selIndex != CB_ERR) {
        SendMessageW(hPresetCombo, CB_GETLBTEXT, selIndex, reinterpret_cast<LPARAM>(presetBuf));
    }

    wchar_t tuneBuf[64] = {};
    selIndex = SendMessageW(hTuneCombo, CB_GETCURSEL, 0, 0);
    if (selIndex != CB_ERR) {
        SendMessageW(hTuneCombo, CB_GETLBTEXT, selIndex, reinterpret_cast<LPARAM>(tuneBuf));
    }

    wchar_t qualBuf[32] = {};
    selIndex = SendMessageW(hQualCombo, CB_GETCURSEL, 0, 0);
    if (selIndex != CB_ERR) {
        SendMessageW(hQualCombo, CB_GETLBTEXT, selIndex, reinterpret_cast<LPARAM>(qualBuf));
    }

    UpdatePresetTuneUI(hwndDlg, encoderView, presetBuf, tuneBuf);
    UpdateQualityUI(hwndDlg, encoderView, qualBuf, false);
}

void UpdateLocationUI(HWND hwndDlg, LocationSelection selection) {
    CheckRadioButton(hwndDlg, IDC_RADIO_LOC_ASK, IDC_RADIO_LOC_FOLDER,
        (selection == LocationSelection::Ask) ? IDC_RADIO_LOC_ASK :
        (selection == LocationSelection::NextToAvi) ? IDC_RADIO_LOC_NEXT : IDC_RADIO_LOC_FOLDER);

    if (selection == LocationSelection::Folder) {
        EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_LOC_PATH), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_LOC_BROWSE), TRUE);
    } else {
        EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_LOC_PATH), FALSE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_LOC_BROWSE), FALSE);
    }
}

INT_PTR CALLBACK BridgeConfig::ConfigDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            CodecState* state = reinterpret_cast<CodecState*>(lParam);
            SetWindowLongPtr(hwndDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
            
            SetDlgItemTextW(hwndDlg, IDC_EDIT_EXTRA_ARGS, state->extra_args.c_str());
            SetDlgItemTextW(hwndDlg, IDC_EDIT_LOC_PATH, state->otherLocation.c_str());
            
            CheckRadioButton(hwndDlg, IDC_RADIO_AUTO, IDC_RADIO_CUSTOM,
                state->selectAuto ? IDC_RADIO_AUTO : IDC_RADIO_CUSTOM);
            UpdateLocationUI(hwndDlg, state->locationSelection);

            CheckDlgButton(hwndDlg, IDC_CHECK_TM_AUDIO_WORKAROUNDS,
                state->tmAudioHooks ? BST_CHECKED : BST_UNCHECKED);
            
            // Pixel formats
            HWND hPixFmtCombo = GetDlgItem(hwndDlg, IDC_COMBO_PIXFMT);
            for (const auto& pixFmt : CodecSets::GetPixelFormats()) {
                SendMessageW(hPixFmtCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(pixFmt));
            }
            
            if (
                state->pix_fmt.empty() ||
                SendMessageW(hPixFmtCombo, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(state->pix_fmt.c_str())) == CB_ERR
            ) {
                SendMessageW(hPixFmtCombo, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(L"yuv420p"));
            }
            
            // Encoder
            HWND hEncoderCombo = GetDlgItem(hwndDlg, IDC_COMBO_ENCODER);
            HWND hEncoderCustom = GetDlgItem(hwndDlg, IDC_EDIT_ENC_CUSTOM);

            const auto& encoders = CodecSets::GetEncoders();

            int matchIndex = -1;
            for (size_t i = 0; i < encoders.size(); ++i) {
                SendMessageW(hEncoderCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(encoders[i].first.c_str()));
                if (state->codec == encoders[i].first) {
                    matchIndex = static_cast<int>(i);
                }
            }
            int otherIndex = static_cast<int>(encoders.size());
            SendMessageW(hEncoderCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(CodecSets::CustomEncoderLabel));
            
            // Defaults
            std::wstring_view activeEncoderUI;
            if (matchIndex != -1) {
                SendMessageW(hEncoderCombo, CB_SETCURSEL, matchIndex, 0);
                activeEncoderUI = encoders[matchIndex].first;
                EnableWindow(hEncoderCustom, FALSE);
            } else {
                SendMessageW(hEncoderCombo, CB_SETCURSEL, otherIndex, 0); 
                EnableWindow(hEncoderCustom, TRUE); 
                SetWindowTextW(hEncoderCustom, state->codec.c_str()); 
                activeEncoderUI = CodecSets::CustomEncoderLabel;
            }
            
            UpdatePresetTuneUI(hwndDlg, activeEncoderUI, state->preset, state->tune);
            UpdateQualityUI(hwndDlg, activeEncoderUI, QualityUtils::GetStringFromQualityMode(state->qualityMode), false);
            
            SetDlgItemInt(hwndDlg, IDC_EDIT_QUAL_VAL1, state->qualityValue1, FALSE);
            SetDlgItemInt(hwndDlg, IDC_EDIT_QUAL_VAL2, state->qualityValue2, FALSE);
            
            UpdateModeUI(hwndDlg, state->selectAuto);
            
            return (INT_PTR)TRUE;
        }

        case WM_COMMAND: {
            CodecState* state = reinterpret_cast<CodecState*>(GetWindowLongPtr(hwndDlg, DWLP_USER));
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if ((wmId == IDC_RADIO_AUTO || wmId == IDC_RADIO_CUSTOM) && wmEvent == BN_CLICKED) {
                UpdateModeUI(hwndDlg, wmId == IDC_RADIO_AUTO);
                return (INT_PTR)TRUE;
            }

            if ((wmId == IDC_RADIO_LOC_ASK || wmId == IDC_RADIO_LOC_NEXT || wmId == IDC_RADIO_LOC_FOLDER) && wmEvent == BN_CLICKED) {
                if (wmId == IDC_RADIO_LOC_ASK) {
                    state->locationSelection = LocationSelection::Ask;
                } else if (wmId == IDC_RADIO_LOC_NEXT) {
                    state->locationSelection = LocationSelection::NextToAvi;
                } else if (wmId == IDC_RADIO_LOC_FOLDER) {
                    state->locationSelection = LocationSelection::Folder;
                }
                UpdateLocationUI(hwndDlg, state->locationSelection);
                return (INT_PTR)TRUE;
            }

            if (wmId == IDC_COMBO_ENCODER && wmEvent == CBN_SELCHANGE) {
                HWND hEncoderCombo = GetDlgItem(hwndDlg, IDC_COMBO_ENCODER);
                HWND hEncoderCustom = GetDlgItem(hwndDlg, IDC_EDIT_ENC_CUSTOM);
                
                int selIndex = SendMessageW(hEncoderCombo, CB_GETCURSEL, 0, 0);
                wchar_t buffer[64];
                SendMessageW(hEncoderCombo, CB_GETLBTEXT, selIndex, reinterpret_cast<LPARAM>(buffer));

                std::wstring_view bufferView(buffer);
                
                if (bufferView == CodecSets::CustomEncoderLabel) {
                    EnableWindow(hEncoderCustom, TRUE);
                    SetFocus(hEncoderCustom);
                } else {
                    EnableWindow(hEncoderCustom, FALSE);
                }
                
                UpdatePresetTuneUI(hwndDlg, bufferView, L"", L"");
                UpdateQualityUI(hwndDlg, bufferView, L"", true);
                
                return (INT_PTR)TRUE;
            }

            if (wmId == IDC_COMBO_QUAL_MODE && wmEvent == CBN_SELCHANGE) {
                UpdateQualityInputs(hwndDlg);
                ApplyDefaultQualityValues(hwndDlg);
                return (INT_PTR)TRUE;
            }

            switch (wmId) {
                case IDOK: {
                    wchar_t buffer[512];
                    
                    state->selectAuto = (IsDlgButtonChecked(hwndDlg, IDC_RADIO_AUTO) == BST_CHECKED);
                    state->tmAudioHooks = (IsDlgButtonChecked(hwndDlg, IDC_CHECK_TM_AUDIO_WORKAROUNDS) == BST_CHECKED);
                    
                    GetDlgItemTextW(hwndDlg, IDC_EDIT_EXTRA_ARGS, buffer, _countof(buffer));
                    state->extra_args = buffer;
                    
                    GetDlgItemTextW(hwndDlg, IDC_EDIT_LOC_PATH, buffer, _countof(buffer));
                    state->otherLocation = buffer;
                    
                    HWND hEncoderCombo = GetDlgItem(hwndDlg, IDC_COMBO_ENCODER);
                    int selIndex = SendMessageW(hEncoderCombo, CB_GETCURSEL, 0, 0);
                    SendMessageW(hEncoderCombo, CB_GETLBTEXT, selIndex, reinterpret_cast<LPARAM>(buffer));

                    if (std::wstring_view(buffer) == CodecSets::CustomEncoderLabel) {
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

                    HWND hQualCombo = GetDlgItem(hwndDlg, IDC_COMBO_QUAL_MODE);
                    selIndex = SendMessageW(hQualCombo, CB_GETCURSEL, 0, 0);
                    if (selIndex != CB_ERR) {
                        SendMessageW(hQualCombo, CB_GETLBTEXT, selIndex, reinterpret_cast<LPARAM>(buffer));
                        state->qualityMode = QualityUtils::GetQualityModeFromString(buffer);
                    }

                    BOOL success = FALSE;
                    state->qualityValue1 = GetDlgItemInt(hwndDlg, IDC_EDIT_QUAL_VAL1, &success, FALSE);
                    state->qualityValue2 = GetDlgItemInt(hwndDlg, IDC_EDIT_QUAL_VAL2, &success, FALSE);
                    
                    EndDialog(hwndDlg, IDOK);
                    return (INT_PTR)TRUE;
                }
                case IDCANCEL:
                    EndDialog(hwndDlg, IDCANCEL);
                    return (INT_PTR)TRUE;
                case IDC_BTN_LOC_BROWSE: {
                    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                    bool comInitializedHere = SUCCEEDED(hrInit);
                    bool comUsable = SUCCEEDED(hrInit) || hrInit == RPC_E_CHANGED_MODE;

                    if (comUsable) {
                        IFileOpenDialog* pDialog = nullptr;
                        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                                    IID_PPV_ARGS(&pDialog));

                        if (SUCCEEDED(hr)) {
                            DWORD options = 0;
                            pDialog->GetOptions(&options);
                            pDialog->SetOptions(options | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);

                            if (!state->otherLocation.empty()) {
                                IShellItem* pDefaultFolder = nullptr;
                                if (SUCCEEDED(SHCreateItemFromParsingName(state->otherLocation.c_str(), nullptr, IID_PPV_ARGS(&pDefaultFolder)))) {
                                    pDialog->SetFolder(pDefaultFolder);
                                    pDefaultFolder->Release();
                                }
                            }

                            hr = pDialog->Show(hwndDlg);
                            if (SUCCEEDED(hr)) {
                                IShellItem* pResult = nullptr;
                                if (SUCCEEDED(pDialog->GetResult(&pResult))) {
                                    PWSTR path = nullptr;
                                    if (SUCCEEDED(pResult->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                                        state->otherLocation = path;
                                        SetDlgItemTextW(hwndDlg, IDC_EDIT_LOC_PATH, path);
                                        CoTaskMemFree(path);
                                    }
                                    pResult->Release();
                                }
                            }

                            pDialog->Release();
                        }
                    }

                    if (comInitializedHere) {
                        CoUninitialize();
                    }

                    return (INT_PTR)TRUE;
                }
            }
            break;
        }
    }
    return (INT_PTR)FALSE;
}
