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
	strcpy(path, home.mb_str());
}