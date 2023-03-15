#include "wx-utils.h"

#include <wx/filename.h>

int wx_dir_exists(char *path) {
        wxFileName p(path);
        return p.DirExists();
}

void wx_get_home_directory(char *path) {
        wxString home = wxFileName::GetHomeDir();
        if (!home.EndsWith(wxFileName::GetPathSeparator())) {
                home.Append(wxFileName::GetPathSeparator());
        }
        #ifdef _WIN32
        wxString str = home;
        int i1, i2, len = str.length();
        for (i1 = 0; i1 < len; i1++) {
            if (str[i1] == '\"') {
                for (i2 = i1; i2 < len - 1; i2++) {
                    str[i2] = str[i2+1];
                }
                len--;
                i1--;
            }
        }        
        home = str;
        #endif        
        strcpy(path, home.mb_str());
}

int wx_create_directory(char *path) { return wxFileName::Mkdir(path); }
