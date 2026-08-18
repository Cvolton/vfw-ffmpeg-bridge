#include <nfd.h>

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>

// adapted from https://github.com/btzy/nativefiledialog-extended/blob/master/test/test_savedialog_native.c

enum class DialogType {
    Save,
    Open,
    SelectFolder,
};

// https://github.com/geode-sdk/geode/blob/140098a5c4402014dbc55718e0e8aad98da48180/loader/src/utils/string.cpp#L122
template <typename T>
std::vector<T> doSplit(std::string_view str, std::string_view split) {
    std::vector<T> res;
    if (!str.empty()) {
        size_t pos = 0;
        while ((pos = str.find(split)) != std::string::npos) {
            res.emplace_back(str.substr(0, pos));
            str.remove_prefix(pos + split.size());
        }
        res.emplace_back(str);
    }
    return res;
}
std::vector<std::string_view> splitView(std::string_view str, std::string_view split) {
    return doSplit<std::string_view>(str, split);
}

const char* pathOrNull(std::string_view s) {
    return s.empty() ? nullptr : s.data();
}

int main(int argc, char** argv) {
    if(argc < 2) {
        printf("Usage: %s <type> [default path] [filter]\n", argv[0]);
        return 2;
    }

    DialogType type = static_cast<DialogType>(atoi(argv[1]));
    std::string defaultPath = argc > 2 ? argv[2] : "";
    std::string filter = argc > 3 ? argv[3] : "";

    auto splitFilter = splitView(filter, "|");

    std::vector<std::pair<std::string, std::string>> filterStrings;
    std::vector<nfdfilteritem_t> filterItems;
    std::string_view lastView;
    size_t idx = 0;
    for(auto& view : splitFilter) {
        if(idx % 2 == 0) {
            lastView = view;
        } else {
            if(view == "*.*") continue;

            lastView = lastView.substr(0, lastView.find_first_of('('));
            while(lastView.ends_with(" ")) {
                lastView.remove_suffix(1);
            }

            if(view.starts_with("*.")) {
                view.remove_prefix(2);
            }

            filterStrings.push_back({std::string(lastView), std::string(view)});
        }
        idx++;
    }

    filterItems.reserve(filterStrings.size());
    for(auto& pair : filterStrings) {
        filterItems.push_back({pair.first.c_str(), pair.second.c_str()});
    }

    NFD_Init();

    std::string defaultFolder = defaultPath.substr(0, defaultPath.find_last_of("/\\"));
    std::string defaultFilename = defaultPath.substr(defaultPath.find_last_of("/\\") + 1);

    nfdchar_t* savePath;
    nfdresult_t result = NFD_ERROR;
    
    switch(type) {
        case DialogType::Save:
            result = NFD_SaveDialog(&savePath, filterItems.data(), filterItems.size(), pathOrNull(defaultFolder), pathOrNull(defaultFilename));
            break;
        case DialogType::Open:
            result = NFD_OpenDialog(&savePath, filterItems.data(), filterItems.size(), pathOrNull(defaultFolder));
            break;
        case DialogType::SelectFolder:
            result = NFD_PickFolder(&savePath, pathOrNull(defaultPath));
            break;
    }

    if (result == NFD_OKAY) {
        puts(savePath);
        NFD_FreePath(savePath);
    } else if (result == NFD_CANCEL) {
        puts("User pressed cancel.");
        return 1;
    } else {
        printf("Error: %s\n", NFD_GetError());
        return 2;
    }

    NFD_Quit();

    return 0;
}