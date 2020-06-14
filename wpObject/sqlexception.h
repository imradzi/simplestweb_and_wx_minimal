#pragma once

#if defined(_WIN32) && defined(__MSVC__)
    #include <oledb.h>  // OLE DB include files
    #include <oledberr.h>
#endif

#include <fstream>
#include "wx/string.h"

class StringException : public std::exception {
    wxString s;
public:
    StringException(const wxString &e) : s(e) {}
    ~StringException() {}
    const char *what() const noexcept { return s.mbc_str(); }
    wxString GetMessage() { return s;}
};

namespace SQLException {

	extern bool showWindow;
	extern bool showMessage;
	extern bool showLog;

//	extern std::ofstream *fOutPtr;

	wxString ShowError(const wxString &s);

#if defined(_WIN32) && defined(__MSVC__)
	wxString DumpErrorInfo(IUnknown* pObjectWithError, REFIID IID_InterfaceWithError, bool toReleaseObject=true);
	wxString GetStatusName( DBSTATUS st );
#endif

	class rException {
	public:
		wxString message;
	public:
		rException(const wxString &s): message(s){} // ShowError(s);}
	};
}

