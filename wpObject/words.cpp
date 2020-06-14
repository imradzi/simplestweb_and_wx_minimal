#ifdef _WIN32
#include "winsock2.h"
#endif
#include "wx/wxprec.h"
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif
#include "wx/hashmap.h"
#include "jsonval.h"
#include "jsonwriter.h"
#include "jsonreader.h"
#include "wx/tokenzr.h"
#include "wx/wfstream.h"
#include "wx/txtstrm.h"
#include "wx/sstream.h"
#include "wx/mstream.h"
#include "wx/sstream.h"
#include "wx/wfstream.h"
#include "wx/zipstrm.h"
#include "wx/zstream.h"

#include "words.h"
#include <string>
#include <iostream>
#include <unordered_map>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "sqlexception.h"

const char* ERROR_SERVER_NOT_FOUND = "server not found";
int Length_ERROR_SERVER_NOT_FOUND = 16;

wxString chartDirectorLicenseKey = "RDST-34TK-G7J8-KCGZ-9D55-418A";
// round X to n dec digit


double RoundIt(double x, double n) {
	return floor( x * pow(10.0, n) + 0.5) / pow(10.0, n);
}

double TruncIt(double x, double n) {
    return floor( x * pow(10.0, n)) / pow(10.0, n);
}

int wpFACTOR_TO_CENTS = 10000;
double wp_nDec = 4;
double wp_nDec_display = 2;

wxLongLong GetValueInCents(double v) {
	//wxString fmt = wxString::Format("%%.%dlf", int(wp_nDec));
	return GetValueInCents(wxString::Format("%.*lf", int(wp_nDec),  v));
}

wxLongLong GetValueInCents(long v) {
    return GetValueInCents(wxString::Format("%ld", v));
}

wxLongLong GetValueInCents(const wxString& v) {
	int nDec = wp_nDec;
	wxLongLong d = 0;
	bool decFound = false;
    bool isNegative = false;
	for (wxString::const_iterator it = v.begin(); it != v.end(); it++) {
        if (*it == '-') {
            isNegative=true;
            continue;
        }
		if (*it == '.') {
			decFound=true;
			continue;
		}
		if (decFound) {
			if (--nDec < 0)
				break;
		}
		d = d * 10 + (char(*it) - '0');
	}
    if (isNegative) d = -d;
	if (nDec > 0)
		d *= pow(float(10), nDec);
	return d;
}

int maxFTSLength = -1;
//int maxFTSLength = 80;
int maxFTSToken = -1;
//int maxFTSToken=16;
bool ftsAddAllTokens = true; // set to false to ingore single char tokens;


void WriteStringToFile(const wxString &fileName, const wxString &str) {
	wxFileOutputStream fOut(fileName);
	wxStringInputStream s(str);
	fOut.Write(s);
}

wxString ReadStringFromFile(const wxString &fileName) {
	if (!wxFileExists(fileName)) return "";
	wxFileInputStream fIn(fileName);
	wxStringOutputStream sOut;
	sOut.Write(fIn);
	return sOut.GetString();
}

wxString CheckFTSCharacters(const wxString &matchStr) {
	wxString res;
	int sz = matchStr.Length();
	wxString src;
	if (sz > maxFTSLength && maxFTSLength > 0) {
		src = matchStr.Mid(0, maxFTSLength);
		int nTrimmed = 0;
		for (wxString::reverse_iterator it = src.rbegin(); it != src.rend(); it++) {
			if (*it == ' ') break;
			nTrimmed++;
		}
		sz = maxFTSLength - nTrimmed;
		src = matchStr.Mid(0, sz);
	}
	else {
		src = matchStr;
	}
	bool prevBlank = false;
	for (wxString::const_iterator it = src.begin(); it != src.end(); it++) {
		bool isSpc = wxIsspace(*it);
		if (isSpc && prevBlank) continue;
		wxChar v = *it;
		if (wxIsalnum(v)) {
			res.Append(v);
			prevBlank = isSpc;
			continue;
		}
		switch (v) {
			case ',':
			case '.':
			case '/':
			case '-':
			case ':':
			case ';':
			case '\'':
			case '"':
			case 0x39: // single quote
			case '(':
			case ')':
			case '<':
			case '>':
			case '{':
			case '}':
			case '!':
			case '|':
			case '~':
			case '^':
			case '`':
			case '%':
			case '?':
			case '\\':
			case '@':
			case '&':
			case '$':
			case '*':
			case '#':
			case '=':
			case '[':
			case ']':
			case '+':
				res.Append(" "); isSpc = true;
				break;
			default:
				if (wxIsprint(v))
					res.Append(wxChar(v)); // used to tolower(*it);
				else {
					res.Append(" ");
					isSpc = true;
				}
				break;
		}
		prevBlank = isSpc;
	}

	if (ftsAddAllTokens) {
		return res;
	}

	wxStringTokenizer tok(res, " ");
	wxString res2;
	int i = 0;
	wxString delim;
	while (tok.HasMoreTokens()) {
		wxString t = tok.GetNextToken();
		if (t.Length() > 1) {
			i++;
			if (i>maxFTSToken && maxFTSToken > 0) break;
			res2.Append(delim + t);
			delim = " ";
		}
	}
	return res2;
}

wxString BuildFTSSearch(const wxString &param) {
	wxString res;
	wxString delim;
	wxStringTokenizer tok(param, " \t\n");
	while (tok.HasMoreTokens()) {
		wxString v = tok.GetNextToken();
		v.Replace("\"", "\"\"", true);
		res.Append(delim + "\"" + v + "\"" + "*");
		delim = " ";
	}
	if (res.IsEmpty()) {res = "ALL";}
	return res;
}

wxString GetActualValue(const wxLongLong &value, int nDecPoint) {
	wxLongLong v = value;
	bool neg = false;
	if (v < 0) { neg = true; v = -v; }
	int nDec = nDecPoint < 0 ? wp_nDec : nDecPoint;
	wxString t = v.ToString();
	t.Trim(); t.Trim(false);
    if (v == 0) { t = "000";}
    int len = t.Length();
    int diff=nDec - len;
    while (diff>0) {
        t.Prepend("0");
        diff--;
    }
    int idx = len-nDec;
    if (idx <= 0) {t.insert(t[0]=='-'?1:0, "0.");}
    else {t.insert(idx, ".");}

	if (neg) {t.insert(0, "-");}
	return t;
}

wxString GetActualValue(const wxString &s, int nDecPoint) {

	int nDec = nDecPoint < 0 ? wp_nDec : nDecPoint;

    wxString t = s;
    int iDot = -1;
	bool neg = false;
    if ((iDot = s.Find('.')) != wxNOT_FOUND) {
        if (iDot == 0) {
			t=wxEmptyString;
		}
        else {
            t = s.Mid(0, iDot);
		}
    }
	t.Trim(); t.Trim(false);
    if (t.IsEmpty()) { 
		t = "000";
	}
	else {
		if (t[0] == '-') {
			neg=true;
			t.erase(0,1);
		}
	}
    int len = t.Length();
    int diff=nDec - len;
    while (diff>0) {
        t.Prepend("0");
        diff--;
    }
    int idx = len-nDec;
    if (idx <= 0) {t.insert(0, "0.");}
    else {t.insert(idx, ".");}
	if (neg) {t.insert(0,'-');}
	return t;
}


double GetNearestFiveCent(double v, bool truncate) {
	constexpr int TEN = 10;
	constexpr int HUNDRED = 100;
	constexpr int FIVE = 5;
	constexpr int ZERO = 0;
	constexpr int MIDWAYLO = 3;
	constexpr int MIDWAYUP = 8;

	long cents=long(v*HUNDRED);
	int add=(long(cents) % TEN);
	if (truncate) {
		if (add < FIVE) {add = -add;} // 3,4 - round up to nearest 5
		else {add = (FIVE - add);} // 6,7 - round down to nearest 5
	}
	else {
		if ((add == ZERO) || (add == FIVE)) {add = 0;}
		else if (add < MIDWAYLO) {add = -add;} // 1,2 - round down to 0
		else if (add < FIVE) {add = (FIVE - add);} // 3,4 - round up to nearest 5
		else if (add < MIDWAYUP) {add = (FIVE - add);} // 6,7 - round down to nearest 5
		else {add = (TEN - add);} // 8,9 round up to 0
	}
	cents += add;
	return double(cents)/HUNDRED;
}

wxString GetNearestFiveCent(const wxString &s, bool truncate, int precision) {
	double v = GetNearestFiveCent(wxAtof(s), truncate);
	wxString ff = wxString::Format("%%.%dlf", precision);
	return wxString::Format(ff,v);
}

double GetNearestTenCent(double v, bool truncate10cents) {
	constexpr int DECIMAL_BASE = 10;
	constexpr int CENT_FACTOR = 100;
	constexpr int HALF = 5;

	long cents = long(v * CENT_FACTOR);
	int add = (long(cents) % DECIMAL_BASE);
	if (truncate10cents) {
		add = -add;
	}
	else {
		if (add <= HALF) {
			add = -add; //round down to 0
		}
		else {
			add = (DECIMAL_BASE - add); // round up to 10
		}
	}
	cents += add;
    return double(cents) / CENT_FACTOR;
}

wxString GetNearestTenCent(const wxString& s, bool truncate10cents, int precision) {
	double v = GetNearestTenCent(wxAtof(s), truncate10cents);
	wxString ff = wxString::Format("%%.%dlf", precision);
	return wxString::Format(ff, v);
}



int digitsForReceiveIDLength = 2;
int digitsForPackIDLength = 1;

// banking on the assumption that receiveID or itemID + packID will never exceed 17 digits.
// otherwise the identification of receiveid vs itemid will fail...
// filler=9 -> receiveID
// filler=8 -> itemID.

constexpr int ID_LENGTH = 18;
bool ParseBarcode(const wxString &id, wxString &receiveID, wxString &packID, wxChar filler) {
    if ((id.Length() != ID_LENGTH)) return false;
    int ridLength = wxAtoi(id.Mid(0, digitsForReceiveIDLength));
    int pidLength = wxAtoi(id.Mid(digitsForReceiveIDLength, digitsForPackIDLength));
    int strLen = digitsForReceiveIDLength + digitsForPackIDLength + ridLength + pidLength + 1;
    if (!IsCheckDigitOK(id.Mid(0, strLen))) return false;
    for (wxString::const_iterator it = id.begin() + strLen; it != id.end(); it++) {
        if ((*it != filler)) return false;
    }
    receiveID = id.Mid(digitsForReceiveIDLength + digitsForPackIDLength, ridLength);
    packID = id.Mid(digitsForReceiveIDLength + digitsForPackIDLength + ridLength, pidLength);
    return true;
}

bool IsBarcodeOK(const wxString &id, wxChar filler) {
    if ((id.Length() != ID_LENGTH)) return false;
    int ridLength = wxAtoi(id.Mid(0, digitsForReceiveIDLength));
    int pidLength = wxAtoi(id.Mid(digitsForReceiveIDLength, digitsForPackIDLength));
    int strLen = digitsForReceiveIDLength + digitsForPackIDLength + ridLength + pidLength + 1;
    if (!IsCheckDigitOK(id.Mid(0, strLen))) return false;
    for (wxString::const_iterator it = id.begin() + strLen; it != id.end(); it++) {
        if ((*it != filler)) return false;
	}
	return true;
}

// len + len + recvID + packID + ckdgt + filler
// because of prescription label and how FMZ keep packsizes; have to change to while still
// compatible to the 999 and 888 -> len+len+recvID+packID+chkdgt+qty+filler - filler=7
wxString MakeBarcodeID(const wxString &receiveID, const wxString &packID, wxChar filler) {
    constexpr int maxLen = ID_LENGTH;
    constexpr int maxBarCharLen = 9;

    if ((receiveID.Length() > maxBarCharLen) || (packID.Length() > maxBarCharLen)) return "";
    wxString id = wxString::Format("%0*d%0*d%s%s", digitsForReceiveIDLength, int(receiveID.length()), digitsForPackIDLength, int(packID.length()), receiveID, packID);
    id.Append(GetCheckDigit(id));
    int oldlen = id.Length();
    if ((oldlen < maxLen))
        for (int i = 0; i < (maxLen - oldlen); i++) id.Append(filler);  // just to fill the spaces
    return id;
}

char GetCheckDigit(const wxString &d) {
	constexpr int DECIMAL_BASE = 10;

	bool isEven = true;
	int sumEven = 0;
	int sumOdd = 0;
	for (wxString::const_iterator it = d.begin(); it != d.end(); it++, isEven = !isEven) {
		if (isEven){
			sumEven += (char)*it - '0';
		}
		else {
			sumOdd += (char)*it - '0';
		}
	}

	int mod = (sumEven + sumOdd * 3) % DECIMAL_BASE;
	if (mod != 0) mod = 10 - mod;
	
	return char(mod + '0');
}

bool IsCheckDigitOK(const wxString &str) {
	wxString d = str; d.Trim().Trim(false);
	char chkDigit = d.Trim().Trim(false).Last(); d.RemoveLast();
	return GetCheckDigit(d) == chkDigit;
}

const wxChar *SkipNonTabWhiteSpace(const wxChar *p);
const wxChar *SkipUntilSpace(const wxChar *p);

static const wxChar *decimalPointChar[] = {wxT("."), wxT(",")};
static const wxChar *commaChar[] = {wxT(","), wxT(".")};

static bool isRandInit = false;

int GetRandom(int b, int count, bool /*isSize*/) {
	if (!isRandInit) {
		srand(time(NULL));
		isRandInit=true;
	}
	return rand() % count + b;
}

double GetRandom(double lo, double hi) {
	if (!isRandInit) {
		srand(time(NULL));
		isRandInit=true;
	}
	long r = hi-lo+1;
	return (rand()%r) + lo;
}

long GetRandom(long lo, long hi) {
	if (!isRandInit) {
		srand(time(NULL));
		isRandInit=true;
	}
	long r = hi-lo+1;
	return (rand()%r) + lo;
}

wxString GetRandomNumberString(int nMax) {
	int nc = GetRandom(1L, nMax);
	wxString res;
	const long ofs = '9' - '0';
	for (int i = 0; i < nc; i++) {
		int cof = GetRandom(0L, ofs);
		char c = '0' + cof;
		res.append(c);
	}
	return res;
}

wxString GetRandomWord(int nMax) {
	int nc = GetRandom(2L, nMax);
	wxString res;
	const long ofs = 'z' - 'a';
	for (int i = 0; i < nc; i++) {
		int cof = GetRandom(0L, ofs);
		char c = 'a' + cof;
		res.append(c);
	}
	return res;
}
wxString GetRandomSentence(int nMax) {
	int nc = GetRandom(1L, nMax);
	wxString res;
	for (int i = 0; i < nc; i++) {
		if (i>0) res.Append(" ");
		res.Append(GetRandomWord(8));
	}
	return res;
}

#ifdef __USE_MSSQL__
std::string GetFormat(DBTIMESTAMP &t) {
	char buf[255];
	sprintf(buf, "%04d%02d%02d", t.year, t.month, t.day);
	return buf;
}
#endif

#ifdef _WIN32
bool Touch(const wxChar *s) {

	HANDLE hFile = CreateFile( s,
					GENERIC_READ|GENERIC_WRITE,              // open for reading
					FILE_SHARE_READ,           // share for reading
					NULL,                      // no security
					OPEN_EXISTING,             // existing file only
					FILE_ATTRIBUTE_NORMAL,     // normal file
					NULL);                     // no attr. template

	if (hFile == INVALID_HANDLE_VALUE) return false;

    FILETIME ft;
    SYSTEMTIME st;
    BOOL f;

    GetSystemTime(&st);              // gets current time
    SystemTimeToFileTime(&st, &ft);  // converts to file time format
    f = SetFileTime(hFile,           // sets last-write time for file
        (LPFILETIME) NULL, &ft, &ft);

	CloseHandle(hFile);

    return true;
}
#endif

std::vector<wxString> ParseCSV(const wxString &t) {
	constexpr int MAXSIZE = 4096;
	wxChar oneWord[MAXSIZE]; memset(oneWord, 0, sizeof(wxChar)*MAXSIZE);
	const wxChar *tp = t.c_str();
	wxString delim;
	std::vector<wxString> v;
	bool checkForBlankField = false;
	while (tp) {
		tp = String::SkipWhiteSpace(tp);
		if (*tp == '\0') break;
		if (*tp == ',') {
			tp++;
			tp = String::SkipWhiteSpace(tp);
			if (checkForBlankField) {
				v.push_back(wxT(""));
			}
		}
		checkForBlankField=false;
		if (!*tp) break;
		if (*tp == '"') { // delimit by quote
			tp++;
			if (*tp == '\0') break;
			if (*tp == '"') {
				tp++; // empty field
				v.push_back(wxT(""));
			} else {
				tp = String::CopyUntilChar(oneWord, tp, '"', MAXSIZE);
				if (*tp == '"') {tp++;}
				wxString tmp = oneWord;
				v.push_back(tmp);
			}
		} else { // no delimit by quote
			if (*tp == '\0') break;
			if (*tp == ',') {
				tp++; // empty field
				v.push_back(wxT(""));
			} else {
				tp = String::CopyUntilChar(oneWord, tp, ',', MAXSIZE);
				wxString tmp = oneWord;
				v.push_back(tmp);
			}
			checkForBlankField=true;
		}
	}
	return v;
}

extern void ShowLog(const wxString &);

void LogTime( time_t &tStart, const wxChar *s ) {
	constexpr int SECONDSPERMIN = 60;
	time_t tStop;
	time( &tStop );
	double ttaken = difftime(tStop, tStart);
	wxString ts = s;
	if (ttaken < SECONDSPERMIN) {
		ShowLog("Time taken("+ts+") = "+String::IntToString(long(ttaken)%SECONDSPERMIN)+" sec. ");
	} else {
		ShowLog("Time taken("+ts+") = "+String::IntToString(long(ttaken/SECONDSPERMIN))+" min and "+String::IntToString(long(ttaken)%SECONDSPERMIN)+" sec. ");
	}
}


/*Tools::Tracer::~Tracer() {

	if (outStr == NULL) return;

	time_t stopTime;
	time( &stopTime );
	double elapsed = difftime( stopTime, startTime );
	wxChar *buf = ctime( &startTime );
	buf[wxStrlen(buf)-1] = '\0'; // to remove crlf

	if (!traceName.empty())
		(*outStr) << traceName.c_str() << ": " << std::endl;
	(*outStr) << "\tStart:   " << buf << std::endl;
	buf = ctime( &stopTime ); buf[strlen(buf)-1] = '\0'; // to remove crlf
	(*outStr) << "\tStop:    " << buf << std::endl;
	(*outStr) << "\tElapsed: " << long(elapsed/60) << " min " << (long(elapsed) % 60) << " sec." << std::endl;
}*/

//const wchar_t *GetParamStr( const wchar_t *s, const wchar_t **p, int delta ) {
//	static wchar_t emptyOne = '\0';
//	s = String::SkipWhiteSpace( s+delta );
//	if (String::IsEmpty(s)) {
//		const wchar_t *x = *(p+1);
//		if (!x || *x == '-') return &emptyOne;
//		return *(p+1);
//	}
//	else
//		return s;
//}

wxString GetParamStr( const char *s, const char **p, int delta ) {
	constexpr int MAXSIZE = 2048;
	char start = *(s + delta);
	wxString res;
	if (start == '\'' || start == '"') {
		char strbuff[MAXSIZE];
		res = String::CopyUntilChar(strbuff, s+delta+1, start, MAXSIZE);
	} else {
		res = wxString(String::SkipWhiteSpace( s+delta ));
	}

	if (res.IsEmpty()) {
		const char *x = *(p+1);
		if (x == nullptr || *x == '-') {return wxEmptyString;}
		return wxString(*(p+1));
	}
	return res;
}

//long GetParamLong( const wchar_t *s, const wchar_t **p ) {
//	s = String::SkipWhiteSpace( s+2 );
//	if (String::IsEmpty(s)){
//		const wchar_t *x = *(p+1);
//		if (!x || *x == '-') return 0;
//		return _wtol(*(p+1));
//	} else
//		return _wtol(s);
//}
//
long GetParamLong( const char *s, const char **p ) {
	s = String::SkipWhiteSpace( s+2 );
	if (String::IsEmpty(s)) {
		const char *x = *(p+1);
		if ((x == nullptr) || (*x == '-')) {return 0;}
		return atol(*(p+1));
	}
	return atol(s);
}


wxChar ParsePeriod(const wxString &v, long &yr, long &period) {
	const wxChar *p = v.c_str();
	p = String::SkipWhiteSpace(p);
	period=0;
	wxChar freq = ' ';
	while ((*p != '\0') && isdigit(*p) != 0) {
		period = period * 10 + (*p-'0');
		p++;
	}
    while ((*p != '\0') && !isdigit(*p) != 0) {
        freq = *p;
        p++;
    }
    yr = 0;
    while ((*p != '\0') && isdigit(*p) != 0) {
        yr = yr * 10 + (*p - '0');
        p++;
    }
    return toupper(freq);
}

//char ParsePeriod(const std::string &v, long &yr, long &period) {
//	const char *p = v.c_str();
//	p = String::SkipWhiteSpace(p);
//	period=0;
//	char freq = ' ';
//	while (*p && isdigit(*p)) {
//		period = period * 10 + (*p-'0');
//		p++;
//	}
//	while (*p && !isdigit(*p)) {
//		freq = *p;
//		p++;
//	}
//	yr = 0;
//	while (*p && isdigit(*p)) {
//		yr = yr*10 + (*p-'0');
//		p++;
//	}
//	return toupper(freq);
//}


namespace String {
	std::shared_ptr<wxMemoryBuffer> UnzipIt(const void *data, size_t len) {
		wxMemoryInputStream memIn(data, len);
		wxMemoryOutputStream sout;
		wxZlibInputStream zip(memIn);
		sout.Write(zip);
		std::shared_ptr<wxMemoryBuffer> res(new wxMemoryBuffer);
		size_t unzipLength = sout.GetSize();
		sout.CopyTo(res->GetAppendBuf(unzipLength), unzipLength);
		res->SetDataLen(unzipLength);
		return res;
	}
	std::shared_ptr<wxMemoryBuffer> UnzipIt(const std::shared_ptr<wxMemoryBuffer> &zipped) {
		wxMemoryInputStream memIn(zipped->GetData(), zipped->GetDataLen());
		wxMemoryOutputStream sout;
		wxZlibInputStream zip(memIn);
		sout.Write(zip);
		std::shared_ptr<wxMemoryBuffer> res(new wxMemoryBuffer);
		size_t unzipLength = sout.GetSize();
		sout.CopyTo(res->GetAppendBuf(unzipLength), unzipLength);
		res->SetDataLen(unzipLength);
		return res;
	}
	std::shared_ptr<wxMemoryBuffer> ZipIt(const void *buf, size_t inputLen) {
		auto ZipIt = [](wxMemoryOutputStream &outMem, wxInputStream &inp) {
			wxZlibOutputStream zip(outMem, 9);
			zip.Write(inp);
			zip.Close();
		};

		wxMemoryInputStream inp(buf, inputLen);
		wxMemoryOutputStream outMem;

		ZipIt(outMem, inp);

		size_t len = outMem.GetSize();
		std::shared_ptr<wxMemoryBuffer> outbuf(new wxMemoryBuffer);
		outbuf->AppendData(outMem.GetOutputStreamBuffer()->GetBufferStart(), len);
		return outbuf;
	}

	std::shared_ptr<wxMemoryBuffer> ZipIt(const std::shared_ptr<wxMemoryBuffer> &str) {
		auto ZipIt = [](wxMemoryOutputStream &outMem, wxInputStream &inp) {
			wxZlibOutputStream zip(outMem, 9);
			zip.Write(inp);
			zip.Close();
		};

		wxMemoryInputStream inp(str->GetData(), str->GetDataLen());
		wxMemoryOutputStream outMem;

		ZipIt(outMem, inp);

		size_t len = outMem.GetSize();
		std::shared_ptr<wxMemoryBuffer> outbuf(new wxMemoryBuffer);
		outbuf->AppendData(outMem.GetOutputStreamBuffer()->GetBufferStart(), len);
		return outbuf;
	}

//---
	wxMemoryBuffer UnzipIt2(const void *data, size_t len) {
		wxMemoryInputStream memIn(data, len);
		wxMemoryOutputStream sout;
		wxZlibInputStream zip(memIn);
		sout.Write(zip);
		wxMemoryBuffer res;
		size_t unzipLength = sout.GetSize();
		sout.CopyTo(res.GetAppendBuf(unzipLength), unzipLength);
		res.SetDataLen(unzipLength);
		return res;
	}
	wxMemoryBuffer UnzipIt2(const wxMemoryBuffer &zipped) {
		wxMemoryInputStream memIn(zipped.GetData(), zipped.GetDataLen());
		wxMemoryOutputStream sout;
		wxZlibInputStream zip(memIn);
		sout.Write(zip);
		wxMemoryBuffer res;
		size_t unzipLength = sout.GetSize();
		sout.CopyTo(res.GetAppendBuf(unzipLength), unzipLength);
		res.SetDataLen(unzipLength);
		return res;
	}
	wxMemoryBuffer ZipIt2(const void *buf, size_t inputLen) {
		auto ZipIt = [](wxMemoryOutputStream &outMem, wxInputStream &inp) {
			wxZlibOutputStream zip(outMem, 9);
			zip.Write(inp);
			zip.Close();
		};

		wxMemoryInputStream inp(buf, inputLen);
		wxMemoryOutputStream outMem;

		ZipIt(outMem, inp);

		size_t len = outMem.GetSize();
		wxMemoryBuffer outbuf;
		outbuf.AppendData(outMem.GetOutputStreamBuffer()->GetBufferStart(), len);
		return outbuf;
	}

	wxMemoryBuffer ZipIt2(const wxMemoryBuffer &str) {
		auto ZipIt = [](wxMemoryOutputStream &outMem, wxInputStream &inp) {
			wxZlibOutputStream zip(outMem, 9);
			zip.Write(inp);
			zip.Close();
		};

		wxMemoryInputStream inp(str.GetData(), str.GetDataLen());
		wxMemoryOutputStream outMem;

		ZipIt(outMem, inp);

		size_t len = outMem.GetSize();
		wxMemoryBuffer outbuf;
		outbuf.AppendData(outMem.GetOutputStreamBuffer()->GetBufferStart(), len);
		return outbuf;
	}


//---

	wxString GetString(const wxJSONValue &v) {
		wxString s;
		wxJSONWriter writer; //no style (0)
		writer.Write(v, s);
		return s;
	}

	wxJSONValue ToJSON(const wxString &t) {
		wxJSONReader reader;
		wxJSONValue v;
		if (reader.Parse(t, &v) == 0) {
			return v;
		}
		return wxJSONValue();
	}

	//wxMemoryBuffer ToBuffer(const wxString &s) {
	//	wxMemoryBuffer buf;
	//	if (!s.IsEmpty()) buf.AppendData(s.mb_str().data(), strlen(s.mb_str().data()));
	//	return buf;
	//}

	std::shared_ptr<wxMemoryBuffer> ToBuffer(const wxString &s) {
		std::shared_ptr<wxMemoryBuffer> buf(new wxMemoryBuffer);
		if (!s.IsEmpty()) buf->AppendData(s.mb_str().data(), strlen(s.mb_str().data()));
		return buf;
	}

	wxMemoryBuffer ToBuffer2(const wxString &s) {
		wxMemoryBuffer buf;
		if (!s.IsEmpty()) buf.AppendData(s.mb_str().data(), strlen(s.mb_str().data()));
		return buf;
	}

	//wxMemoryBuffer JSONtoBuffer(const wxJSONValue &v) {
	//	wxMemoryBuffer buf;
	//	wxString s = String::GetString(v);
	//	if (!s.IsEmpty()) buf.AppendData(s.mb_str().data(), strlen(s.mb_str().data()));
	//	return buf;
	//}

	std::shared_ptr<wxMemoryBuffer> JSONtoBuffer(const wxJSONValue &v) {
		std::shared_ptr<wxMemoryBuffer> buf(new wxMemoryBuffer);
		wxString s = String::GetString(v);
		if (!s.IsEmpty()) buf->AppendData(s.mb_str().data(), strlen(s.mb_str().data()));
		return buf;
	}

	wxLongLong ToLongLong(const wxString &s) {
		wxLongLong_t d;
		if (s.ToLongLong(&d)) return wxLongLong(d);
		return 0;
	}

	long ToLong(const wxString &s) {
		long d;
		if (s.ToLong(&d)) return d;
		return 0;
	}

	double ToDouble(const wxString &s) {
		double d;
		if (s.ToDouble(&d)) return d;
		return 0.0;
	}

	std::vector<wxString> StringToVector(const wxString &x, const wxString &delim) {
		wxStringTokenizer tok(x, delim, wxTOKEN_RET_EMPTY_ALL);
		std::vector<wxString> out;
		while (tok.HasMoreTokens()) {
			out.push_back(tok.GetNextToken());
		}
		return out;
	}

	wxString VectorToString(const std::vector<wxString> &v, const wxString &delim) {
		wxString out, d="";
		for (auto const& it : v) {
			out.Append(d);
			out.Append(it);
			d = delim;
		}
		return out;
	}

    int CountChar(const wxString &text, wxChar ch) {
        int n = 0;
        for (wxString::const_iterator it = text.begin(); it != text.end(); it++) {
            if (*it == ch) n++;
        }
        return n;
    }

	wxString StripBracket(const wxString &t) {
		const wxChar *p = t.c_str();
		p = SkipWhiteSpace(p);
		if (*p != '[') return t;
		p++;
		wxChar *buf = new wxChar[t.Length()];
		CopyUntil(']', buf, p);
		wxString strBuf = buf;
		delete[] buf;
		return strBuf;
	}

	wxString TrimDecimal(const wxString s) {
		wxString newStr = s;
		//bool foundDec = false;
		if (!newStr.Contains(".")) return newStr;
		size_t l = newStr.Length();
		size_t newLength = l;
		bool toTruncate = false;
		for (; l > 0; l--) {
			wxChar v = newStr[l-1];
			if (v == '0') {
				newLength = l;
				toTruncate = true;
			} else if (wxIsdigit(v)) {
				newLength = l;
				break;
			} else if (v == '.') {
				newLength = l-1;
				toTruncate = true;
				break;
			}
		}
		return (toTruncate ? newStr.Truncate(newLength) : newStr);
	}

	std::string GetNumber(const std::string &v) {
		const char *s = v.c_str();
		std::string newStr;
		bool findNum = false;
		for (; *s; s++) {
			if (!(isdigit(*s) || *s == '-' || *s == '.')) break;
			if (findNum) {
				newStr += *s;
				continue;
			}
			if (*s == '0' || *s == ' ') {
				const char *n = s+1;
				if (*n != '.') continue;
			}
			newStr += *s;
			findNum = true;
		}
		return newStr;
	}
	wxString GetNumber(const wchar_t *s) {
		wxString newStr;
		bool findNum = false;
		for (; *s; s++) {
			if (!(isdigit(*s) || *s == '-' || *s == '.')) break;
			if (findNum) {
				newStr += *s;
				continue;
			}
			if (*s == '0' || *s == ' ') {
				const wchar_t *n = s+1;
				if (*n != '.') continue;
			}
			newStr += *s;
			findNum = true;
		}
		return newStr;
	}

	bool IsValidDate( int year, int month, int day ) {
		static const int nDayInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
		if (month <= 0 || day <= 0 || year <= 0 || month > 12) return false;
		int maxDay = nDayInMonth[month];
		if (month == 2 && year%4 == 0) maxDay=29;
		return (day >= 1 && day <= maxDay);
	}

	std::string GetString(const char *s, long len) {
		std::string r;
		r.resize(len);
		r.reserve(len);
		strncpy(&r[0], s, len);
		return r;
	}

	void Replace(wchar_t *dest, wchar_t from, wchar_t to) {
		while (*dest) {
			if (*dest == from) *dest=to;
			dest++;
		}
	}

	void Replace(char *dest, char from, char to) {
		while (*dest) {
			if (*dest == from) *dest=to;
			dest++;
		}
	}

//	std::string CombineAt2Ends(const char *left, const char *right, int len) {
//		std::shared_ptr<char> resData(new char[len+1]);
//		if (!resData) throw StringException("cannot allocate memory");
//		char *res = resData.get();
//		memset(res,' ',len); res[len]='\0';
//		int lenLeft=strlen(left);
//		strncpy(res, left, lenLeft);
//		if (lenLeft >= len) {
//			return std::string(res);
//		}
//		res[lenLeft]=' ';
//
//		int lenRight=strlen(right);
//		std::shared_ptr<char> rData(new char[lenRight+1]);
//		if (!rData) throw StringException("cannot allocate memory");
//		char *r = rData.get();
//		strcpy(r, right);
//		Trim(r);
//		lenRight=strlen(r);
//
//		if (lenRight > 0) {
//			char *p = res+len-1;
//			char *s = r+lenRight-1;
//			for (int i=0; i<lenRight; i++)
//				*p-- = *s--;
//		}
//		return std::string(res);
//	}


	void ReplaceSpaceWithDotsDownward(char *start, char *p) {
		while (p > start) {
			if (*p != ' ') p--;
			else break;
		}
		while (p > start) {
			if (*p == ' ') *p-- = '.';
			else break;
		}
	}

/*	const wxChar *FindSwitchValue(const wxChar *p, const wxChar *flag, bool isCaseSensitive) {
		static char buffer[512];

		const wxChar *o;
		if (!isCaseSensitive) o = stristr(p, flag);
		else o = strstr(p, flag);

		if (o==NULL) return NULL;
		o+=strlen(flag);
		while (*o)
			if (*o==' ') o++;
			else break;

		if (*o == '\0') return "";

		bool isQuoted = *o=='"';

		if (isQuoted) o++;

		wxChar *b = buffer;
		while (*o) {
			if (isQuoted) {
				if (*o=='"') break;
			} else {
				if (*o==' ') break;
			}
			*b++ = *o++;
		}
		*b = '\0';
		return buffer;
	}


	std::string FindOption(const wxChar *p, const wxChar *sswitch, bool isCaseSensitive) {

		static char buffer[512];

		const wxChar *o;
		if (!isCaseSensitive) o = stristr(p, sswitch);
		else o = strstr(p, sswitch);

		if (o==NULL) return "";
		o+=strlen(sswitch);
		while (*o)
			if (*o==' ') o++;
			else break;

		if (*o == '\0') return "";

		bool isQuoted = *o=='"';

		if (isQuoted) o++;

		wxChar *b = buffer;
		while (*o) {
			if (isQuoted) {
				if (*o=='"') break;
			} else {
				if (*o==' ') break;
			}
			*b++ = *o++;
		}
		*b = '\0';
		return (buffer);
	}
*/
	wxString FormatSpecial(double v) {
		static wxChar buf[20];
		wxSprintf(buf, wxT("%10.1lf"), v);
		const wxChar *p = SkipWhiteSpace( buf );
		wxStrcpy(buf, p);
		int l = wxStrlen(buf);
		if (buf[l-1] == '0')
			buf[l-2]='\0';
		return buf;
	}

	void CopyChar( char *buf, const char *src, int len ) {
		while (*src) {
			*buf++ = *src++;
			len--;
			if (len <=0) break;
		}
	}
	void ClearChar( wxChar *buf, int len) { memset(buf, ' ', len*sizeof(wxChar)); buf[len]='\0'; }
	void ClearChar( wxChar *buf ) { ClearChar(buf, wxStrlen(buf)); }

	bool IsEmpty(const wchar_t *s) {
		while (*s) {
			if (*s != ' ' && *s != '\t' && *s != '\n') return false;
			s++;
		}
		return true;
	}

	bool IsEmpty(const wxString &s) {
		if (s.IsEmpty()) return true;
        for (wxString::const_iterator it = s.begin(); it != s.end(); it++) {
			if (*it != ' ' && *it != '\t' && *it != '\n') return false;
		}
		return true;
	}

	bool IsEmpty(const char *s) {
		while (*s) {
			if (*s != ' ' && *s != '\t' && *s != '\n') return false;
			s++;
		}
		return true;
	}

	bool IsEmpty(const wchar_t *s, int len) {
		while (*s) {
			if (*s != ' ' && *s != '\t' && *s != '\n') return false;
			s++;
			if (--len <=0) break;
		}
		return true;
	}

	bool IsEmpty(const char *s, int len) {
		while (*s) {
			if (*s != ' ' && *s != '\t' && *s != '\n') return false;
			s++;
			if (--len <=0) break;
		}
		return true;
	}


	bool IsNumeric(const wchar_t *s) {
		while (*s) {
			if (!(wxIsdigit(*s) || *s=='.'))
				return false;
			s++;
		}
		return true;
	}

	bool IsNumeric(const wxString &str) {
		for (wxString::const_iterator it = str.begin(); it != str.end(); it++) {
			if (!(wxIsdigit(*it) || *it=='.' || *it=='-'))
				return false;
		}
		return true;
	}

	bool IsNumeric(const wchar_t *s, int len) {
		while (*s) {
			if (!(isdigit(*s) || *s=='.' || *s == '-'))
				return false;
			s++;
			if (--len <=0) break;
		}
		return true;
	}

	bool IsNumeric(const char *s) {
		while (*s) {
			if (!(isdigit(*s) || *s=='.' || *s == '-'))
				return false;
			s++;
		}
		return true;
	}

	bool IsNumeric(const char *s, int len) {
		while (*s) {
			if (!(isdigit(*s) || *s=='.' || *s=='-'))
				return false;
			s++;
			if (--len <=0) break;
		}
		return true;
	}

	bool IsValidNumber(const wxChar *s) {
		s = SkipWhiteSpace(s);
		bool spaceFound = false;
		bool negative = false;
		for (;*s; s++) {
			if (*s == ' ') spaceFound = true;
			else if ((wxIsdigit(*s) || *s == '.' || *s=='-') && !spaceFound) continue;
			else if (*s == '-' && !negative) {
				negative = true;
				continue;
			}
			else return false;
		}
		return true;
	}

	wchar_t *Trim(wchar_t *s) {
		wchar_t *p = s+wcslen(s)-1;
		while ( (p >= s) && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == 0x0D || *p == 0x0A)) {
			*p = '\0';
			p--;
		}
		return s;
	}

	char *Trim(char *s) {
		char *p = s+strlen(s)-1;
		while ( (p >= s) && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == 0x0D || *p == 0x0A)) {
			*p = '\0';
			p--;
		}
		return s;
	}

	wchar_t *ToLower(wchar_t *s) {
		wchar_t *p = s;
		while ( *p ) {
			*p = tolower(*p);
			p++;
		}
		return s;
	}

	char *ToLower(char *s) {
		char *p = s;
		while ( *p ) {
			*p = tolower(*p);
			p++;
		}
		return s;
	}

	std::string ToLower(const std::string &s) {
		std::string r;
		const char *p = s.c_str();
		for (; *p; p++)
			r += tolower(*p);
		return r;
	}

	std::string Trim(const std::string &s) {
		char *p = new char[s.length()+1];
		if (!p) throw StringException("cannot allocate memory");
		strcpy(p, s.c_str());
		std::string v = Trim(p);
		delete[] p;
		return v;
	}

	void TrimString(std::string &s) {
		size_t l = strlen(Trim( &s[0] ));
		s.resize(l);
	}

	void strncpysz(wxChar *dest, const wxChar *orig, size_t len) {
		size_t l = wxStrlen(orig)+1;
		if (l > len) {
			l = len;
		}
		wxStrncpy(dest, orig, l-1);
		dest[l-1] = '\0';
	}

	void strncpysz(char *dest, const char *orig, size_t len) {
		size_t l = strlen(orig)+1;
		if (l > len) {
			l = len;
		}
		strncpy(dest, orig, l-1);
		dest[l-1] = '\0';
	}

	const wxChar *SkipEOL( const wxChar *p ){
		while (*p && *p != '\n') {
			p++;
		}
		if (*p == '\n') {
			p++;
		}
		return p;
	}

	const wchar_t *SkipWhiteSpace(const wchar_t *p) {
		while ( *p && (*p == ' ' || *p=='\t' || *p == '\r' || *p == '\n') ) {
			p++;
		}
		return p;
	}

	const char *SkipWhiteSpace(const char *p) {
		while ( *p && (*p == ' ' || *p=='\t' || *p == '\r' || *p == '\n') ) {
			p++;
		}
		return p;
	}

	const wxChar *SkipNonTabWhiteSpace(const wxChar *p) {
		while ( *p && *p == ' ' ) {
			p++;
		}
		return p;
	}

	const char *SkipUntil(char x, const char *p) {
		while ( *p && *p != x ) {
			p++;
		}
		if (*p == x) {
			p++;
		}
		return p;
	}
	const wchar_t *SkipUntil(wchar_t x, const wchar_t *p) {
		while ( *p && *p != x ) {
			p++;
		}
		if (*p == x) {
			p++;
		}
		return p;
	}
	const char *SkipWhile(char x, const char *p) {
		while ( *p && *p == x ) {
			p++;
		}
		if (*p == x) {
			p++;
		}
		return p;
	}
	const wchar_t *SkipWhile(wchar_t x, const wchar_t *p) {
		while ( *p && *p == x ) p++;
		if (*p == x) p++;
		return p;
	}


	const char *SkipUntilSpace(const char *p) {
		while ( *p && !(*p == ' ' || *p=='\t' || *p == '\n') ) {
			p++;
		}
		return p;
	}

	const wchar_t *SkipUntilSpace(const wchar_t *p) {
		while ( *p && !(*p == ' ' || *p=='\t' || *p == '\n') ) {
			p++;
		}
		return p;
	}

	const char *SkipUntilNumber(const char *p) {
		while ( *p && !( isdigit(*p) || *p == '.') ) {
			p++;
		}
		return p;
	}

	const wchar_t *SkipUntilNumber(const wchar_t *p) {
		while ( *p && !( isdigit(*p) || *p == '.') ) {
			p++;
		}
		return p;
	}

	const wchar_t *GetNumber(long &n, const wchar_t *p) {
		n = 0;
		while ((isdigit(*p))) {
			n = n * 10 + (*p - '0');
			p++;
		}
		return p;
	}

	const char *GetNumber(long &n, const char *p) {
		n = 0;
		while ((isdigit(*p))) {
			n = n * 10 + (*p - '0');
			p++;
		}
		return p;
	}


	const wchar_t *CopyUntilSpace(wchar_t *toStr, const wchar_t *orig, size_t len) {
		const wchar_t *p = SkipWhiteSpace(orig);
		size_t l=0;
		while ( *p && *p != ' ' && *p != '\t' && *p != '\n' ) {
			if (++l < len || len < 0) *toStr++ = *p;
			p++;
		}
		*toStr='\0';
		return SkipWhiteSpace(p);
	}

	const char *CopyUntilSpace(char *toStr, const char *orig, size_t len) {
		const char *p = SkipWhiteSpace(orig);
		size_t l=0;
		while ( *p && *p != ' ' && *p != '\t' && *p != '\n' ) {
			if (++l < len || len < 0) *toStr++ = *p;
			p++;
		}
		*toStr='\0';
		return SkipWhiteSpace(p);
	}

	const wxChar *CopyUntilEqualOrSpace(wxChar *toStr, const wxChar *orig, size_t len) {
		const wxChar *p = SkipWhiteSpace(orig);
		size_t l=0;
		while ( *p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '=') {
			if (++l < len || len < 0) *toStr++ = *p;
			p++;
		}
		*toStr='\0';
		p = SkipWhiteSpace(p);
		if (*p == '=') p++;
		return SkipWhiteSpace(p);
	}

	const wxChar *CopyAttribute(wxChar *toStr, const wxChar *orig, size_t len) {
		const wxChar *p = SkipWhiteSpace(orig);
		size_t l=0;
		while ( *p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '=' && *p != '>') {
			if (++l < len || len < 0) *toStr++ = *p;
			p++;
		}
		*toStr='\0';
		p = SkipWhiteSpace(p);
		if (*p == '=') p++;
		return SkipWhiteSpace(p);
	}

	const wxChar *CopyTag(wxChar *toStr, const wxChar *orig, size_t len) {
		const wxChar *p = SkipWhiteSpace(orig);
		size_t l=0;
		while ( *p && *p != ' ' && *p != '>' && *p != '\t' && *p != '\n' && !(*p == '/' && *(p+1) == '>')) {
			if (++l < len || len < 0) *toStr++ = *p;
			p++;
		}
		*toStr='\0';
		p = SkipWhiteSpace(p);
//		if ( *p && *p == '/' && *(p+1)=='>') p += 2;
		return SkipWhiteSpace(p);
	}

	const wxChar *CopyUntilTab(wxChar *toStr, const wxChar *orig, size_t len) {
		const wxChar *p = SkipNonTabWhiteSpace(orig);
		size_t l=0;
		while ( *p && *p != '\t' && *p != '\n') {
			if (++l < len || len < 0) *toStr++ = *p;
			p++;
		}
		*toStr='\0';
		if (*p == '\t') p++;
		return SkipNonTabWhiteSpace(p);
	}

	const wxChar *CopyUntilChar(wxChar *toStr, const wxChar *orig, char delim, size_t len) {
		const wxChar *p = orig;
		size_t l=0;
		while ( *p && *p != delim) {
			if (++l < len || len < 0) *toStr++ = *p;
			p++;
		}
		*toStr='\0';
		if (*p == delim) p++;
		return p;
	}

	const char *CopyUntilChar(char *toStr, const char *orig, char delim, size_t len) {
		const char *p = orig;
		size_t l = 0;
		while (*p && *p != delim) {
			if (++l < len || len < 0) *toStr++ = *p;
			p++;
		}
		*toStr = '\0';
		if (*p == delim) p++;
		return p;
	}

	const wchar_t *CopyUntilCharOrSpace(wchar_t *toStr, const wchar_t *orig, wchar_t delim, size_t len) {
		const wchar_t *p = orig;
		size_t l=0;
		while ( *p && *p != delim && *p != ' ' && *p != '\t' && *p != '\n' ) {
			if (++l < len || len < 0) *toStr++ = *p;
			p++;
		}
		*toStr='\0';
		if (*p == delim) p++;
		return p;
	}

	const char *CopyUntilCharOrSpace(char *toStr, const char *orig, char delim, size_t len) {
		const char *p = orig;
		size_t l=0;
		while ( *p && *p != delim && *p != ' ' && *p != '\t' && *p != '\n' ) {
			if (++l < len || len < 0) *toStr++ = *p;
			p++;
		}
		*toStr='\0';
		if (*p == delim) p++;
		return p;
	}


	int Compare(const wxChar *s1, const wxChar *s2) {
		const wxChar *p1 = SkipWhiteSpace(s1);
		const wxChar *p2 = SkipWhiteSpace(s2);
		while (*p1 && *p1) {
			int c = wxStrnicmp(p1, p2, 1);
			if (c!=0) return c;
			p1++; p1 = SkipWhiteSpace(p1);
			p2++; p2 = SkipWhiteSpace(p2);
		}
		return wxStrnicmp(p1,p2,1);
	}

	const wchar_t *CopyUntil(char v, wchar_t *toStr, const wchar_t *orig, size_t len, bool skipWS, bool includeToken) {
		const wchar_t *p = skipWS ? SkipWhiteSpace(orig) : orig;
		size_t l=0;
		while ( *p && *p != v ) {
			if (++l < len || len < 0) *toStr++ = *p;
			p++;
		}
		if (*p == v) {
			if (includeToken) *toStr++ = v;
			p++;
		}
		*toStr='\0';
		return skipWS ? SkipWhiteSpace(p) : p;
	}
	const char *CopyUntil(char v, char *toStr, const char *orig, size_t len, bool skipWS, bool includeToken) {
		const char *p = skipWS ? SkipWhiteSpace(orig) : orig;
		size_t l=0;
		while ( *p && *p != v ) {
			if (++l < len || len < 0) *toStr++ = *p;
			p++;
		}
		if (*p == v) {
			if (includeToken) *toStr++ = v;
			p++;
		}
		*toStr = '\0';
		return skipWS ? SkipWhiteSpace(p) : p;
	}

	int CountWords(const wxChar *orig) {
		const wxChar *p = orig;
		int nWord=0;
		while (*p) {
			p = SkipWhiteSpace(p);
			if (*p) nWord++;
			p = SkipUntilSpace(p);
		}
		return nWord;
	}

	wxString ReplaceString( const wxChar *orig, const wxChar *key, const wxChar *with) {
		wxString v;
		size_t len = wxStrlen(key);
		const wxChar *p = orig;
		for (;;) {
			const wxChar *z = stristr(p, key);
			if (z == NULL) {
				v.Append(p);
				break;
			}
			int l = (z - p) / sizeof(wxChar);
			v.Append( wxString(p, l) );
			v.Append( with );
			p = z+len;
		}
		return v;
	}

	const wxChar *stristr(const wxChar *u, const wxChar *s) {

		if (s==NULL) return u;

		const wxChar *p=u;

		size_t len=wxStrlen(s);

		if (len==0) return u;

		for (unsigned int i=0; i<wxStrlen(u); i++, p++)
			if (wxStrnicmp(p, s,len) == 0) return p;
		return NULL;
	}


/*
	Words::Words(const wxChar *s, size_t len) {
		if (len==-1) len=strlen(s);
		str = new char[len+1];
		if (!str) throw SQLException::rException(wxT("cannot allocate memory"));
		strncpysz(str, s, len);
	}

	Words::Words(Words &s) {
		size_t len=strlen(s.str);
		str = new char[len+1];
		if (!str) throw SQLException::rException(wxT("cannot allocate memory"));
		strncpysz(str, s.str, len);
	}

	Words::~Words() { delete[] str; }

	const wxChar *WordList::operator[](int i) {
		if (i < nWords && i >= 0) return w[i];
		return "\0";
	}

	WordList::WordList(const wxChar *str, size_t len) {
		if (len < 0) len=strlen(str);
		wxChar *v = new char[len+1];
		if (!v) throw SQLException::rException(wxT("cannot allocate memory"));
		strncpy(v, str, len); v[len]='\0';

		nWords=CountWords(v);
		w = new wxChar *[nWords]; //max words
		if (!w) throw SQLException::rException(wxT("cannot allocate memory"));
		for (int i=0; i<nWords; i++) w[i]=NULL;
		const wxChar *p = v;
		for (int i=0; i<nWords; i++) {
			w[i] = new char[40];
			if (!w[i]) throw SQLException::rException(wxT("cannot allocate memory"));
			p = CopyUntilSpace(w[i], p);
		}
		delete[] v; v=NULL;
	}

	WordList::~WordList() {
		for (int i=0; i<nWords; i++)
			delete[] w[i];
		delete[] w; w=NULL;
	}

	bool WordList::ExistIn(WordList &wName) {
	// the rule is the first word in wList must equal with first word in wName PANA == PANADOL....
		if (nWords==0) return true;
		int startPos=1;
		if (*w[0] == '*') startPos=0;
		else if (strncasecmp(w[0], wName[0], strlen(w[0])) != 0) return false;
		wxChar **p = w+1; // skip first one!
		for (int i=1; i<nWords; i++,p++) {
			bool found=false;
			for (int j=startPos; j<wName.NumWords(); j++) {
				if (strncasecmp(*p, wName[j], strlen(*p)) == 0) {
					found=true;
					break;
				}
			}
			if (!found) return false;
		}
		return true;
	}

	bool WordList::ExistIn(const wxChar *s, bool skipFirstWord, size_t len) {
		if (nWords == 0) return true;
		if (len < 0) len=strlen(s);
		wxChar *v = new char[len+1];
		if (!v) throw SQLException::rException(wxT("cannot allocate memory"));
		strncpy(v, s, len); v[len]='\0';
		const wxChar *p=v;
		if (skipFirstWord) p = SkipUntilSpace(v);
		for (int i=0; i<nWords; i++) {
			if (stristr(p, w[i]) == NULL) return false;
		}
		return true;
	}
*/


}


//static char g_buffer[1024];

// for DDD Address comparison.
long atol(const wchar_t *p, int len) {
	long u = 0;
	for (; len > 0; len--, p++) {
		if (*p == ' ' || *p == '\t') {
			if (u != 0) break;
		} else if (wxIsdigit(*p)) u = u*10 + (*p - '0');
		else break;
	}
	return u;
};

long atol(const char *p, int len) {
	long u = 0;
	for (; len > 0; len--, p++) {
		if (*p == ' ' || *p == '\t') {
			if (u != 0) break;
		} else if (isdigit(*p)) u = u*10 + (*p - '0');
		else break;
	}
	return u;
};

double atof(const wchar_t *p, int len) {
	double u = 0;
	int nDecimal = 0;
	for (; len > 0; len--, p++) {
		if (*p == ' ' || *p == '\t') {
			if (u != 0) break;
		} else if (wxIsdigit(*p)) {
			if (nDecimal > 0) {
				u += (*p - '0')/10*nDecimal;
				nDecimal++;
			} else
				u = u*10 + (*p - '0');
		} else if (*p == '.')
			nDecimal=1;
		else break;
	}
	return u;
};

double atof(const char *p, int len) {
	double u = 0;
	int nDecimal = 0;
	double divisor = 10;
	for (; len > 0; len--, p++) {
		if (*p == ' ' || *p == '\t') {
			if (u != 0) break;
		} else if (wxIsdigit(*p)) {
			if (nDecimal > 0) {
				u += double(*p - '0')/divisor;
				nDecimal++;
				divisor *= 10;
			} else
				u = u*10 + (*p - '0');
		} else if (*p == '.')
			nDecimal=1;
		else break;
	}
	return u;
};

wxString String::GetNameOnly(const wxChar *s) {
	wxString b;
	for (; *s; s++) {
		if (wxIsdigit(*s)) continue;
		if (*s == '#') continue;
		if (*s == '-') continue;
		b.Append(*s);
	}
	return b;
}

wxString String::GetNumberOnly(const wxChar *s) {
	wxString b;
	for (; *s; s++) {
		if (isdigit(*s) || *s=='-')
			b.Append(*s);
	}
	return b;
}

wxString String::FormatNumber( int s, int nDec, bool isSwapChar ) {
    wxString v = IntToString(s);
    int len = v.Length();
    for (int i=0; i <= (nDec - len); i++)
        v = "0"+v;
    v.insert(v.Length()-nDec, 1, '.');
    return FormatNumber(v, nDec, isSwapChar);
}

//TODO: Fix 1900.00 - with zero nDec => result still 1900.00 instead of 1900
wxString String::FormatNumber( const wxChar *s, int nDec, bool isSwapChar ) {
	wxString str;
	int cIndex = isSwapChar?1:0;
	str=s;
	int iDot = str.Find(wxChar('.'), true); // find from end;
	if (nDec == 0) return str.Mid(0, iDot);
	int start=str.Length()-1;
	if (iDot >= 0) {
		str[iDot] = *(decimalPointChar[cIndex]);
		start=iDot-1;
		if (nDec > 0) {
			if (int(str.Length())-iDot <= nDec)
				str.Append('0', nDec-(str.Length()-iDot-1));
			else str.Remove(iDot+nDec+1);
		}
	} else if (nDec > 0) {
		str.Append(decimalPointChar[cIndex]);
		str.Append(wxChar('0'), nDec);
	}
	int endingIndex = (str[0] == '-' ? 1:0);
	for (int i=0; start > endingIndex; start--) {
		if (++i >= 3) {
			str.insert(start, commaChar[cIndex]);
			i=0;
		}
	}
	return str;
}

wxString String::FormatNumber( double f, int nDec, bool isSwapChar ) {
	//wxString fmt = wxString::Format(wxT("%%.%dlf"), nDec);
	wxString str = wxString::Format("%.*lf", nDec, f);
	int cIndex = isSwapChar?1:0;
	int iDot = str.Find(wxChar('.'), true); // find from end;
	int start=str.Length()-1;
	if (iDot >= 0) {
		str[iDot] = *(decimalPointChar[cIndex]);
		start=iDot-1;
		if (nDec > 0) {
			if (int(str.Length())-iDot <= nDec)
				str.Append('0', nDec-(str.Length()-iDot-1));
			else str.Remove(iDot+nDec+1);
		}
	} else if (nDec > 0) {
		str.Append(decimalPointChar[cIndex]);
		str.Append(wxChar('0'), nDec);
	}
	int endingIndex = (str[0] == '-' ? 1:0);
	for (int i=0; start > endingIndex; start--) {
		if (++i >= 3) {
			str.insert(start, commaChar[cIndex]);
			i=0;
		}
	}
	return str;
}

int HexStringToInteger(const wxString &x) {
	int a = 0;
	for (unsigned int i=0; i<x.Length(); i++) {
		int v = 0;
		if (x[i] >= '0' && x[i] <= '9')
			v = x[i] - '0';
		else if (x[i] >= 'a' && x[i] <= 'f')
			v = 10 + wxChar(x[i]) - 'a';
		else if (x[i] >= 'A' && x[i] <= 'F')
			v = 10 + wxChar(x[i]) - 'A';
		else break;
		a = a*16 + v;
	}
	return a;
}

wxString HTTPTranslateCharacterIn(const wxString &t) {
	wxString v;
	for (unsigned int i=0; i<t.Length();) {
		if 		(t.Mid(i, 6).IsSameAs(wxT("&quot;"), false))	{v.Append(wxT('\"'));i += 6;}
		else if (t.Mid(i, 6).IsSameAs(wxT("&apos;"), false)) 	{v.Append(wxT('\''));i += 6;}
		else if (t.Mid(i, 5).IsSameAs(wxT("&amp;"),  false))	{v.Append(wxT('&')); i += 5;}
		else if (t.Mid(i, 4).IsSameAs(wxT("&lt;"),   false))	{v.Append(wxT('<')); i += 4;}
		else if (t.Mid(i, 4).IsSameAs(wxT("&gt;"),   false))	{v.Append(wxT('>')); i += 4;}
		else {
			if (t[i] == '%') {
				i++;
				wxString hex = t.Mid(i, 2);
				i+=2;
				char s = (char)HexStringToInteger(hex);
				v.Append(s);
			} else {
				v.Append(t[i]);
				i++;
			}
		}
	}
	return v;
}

// size of page width in compressed, normal, fat, tall
// c = 40;
// n = 33;
// f = 16;
// t = 33;

wxString Format(const wxString &left, const wxString &right, int sz) {
    char *buf = new char[sz+1];
    memset(buf, ' ', sz);
    buf[sz]='\0';

    if (left.IsSameAs("-") || right.IsSameAs("-")) {
        for (int i=0; i<sz; i++)
            buf[i] = '-';
    } else {
        int szLeft = sz-right.Length()-1;
        const wxChar *p = left.c_str();
        for (int i=0; i<szLeft; i++, p++) {
            if (!*p) break;
            buf[i] = *p;
        }
        int k = sz-1;
        for (int i=right.Length()-1; i>=0; i--, k--)
            buf[k] = right[i];
    }
    wxString t(buf);
    delete[] buf;
    return t;
}


//WX_DECLARE_HASH_MAP( long,      // type of the keys
//                     wxString,    // type of the values
//                     wxIntegerHash,     // hasher
//                     wxIntegerEqual,   // key equality predicate
//                     wxIntegerToStringHashMap); // name of the class


static std::unordered_map<long, wxString> _keyCodeMap;
static void InitStringMap() {
    _keyCodeMap[WXK_BACK] = "BACKSPACE";
    _keyCodeMap[WXK_TAB] = "TAB";
    _keyCodeMap[WXK_RETURN] = "RETURN";
    _keyCodeMap[WXK_ESCAPE] = "ESCAPE";
    _keyCodeMap[WXK_SPACE] = "SPACE";
    _keyCodeMap[WXK_DELETE] = "DELETE";
    _keyCodeMap[WXK_START] = "START";
    _keyCodeMap[WXK_LBUTTON] = "LEFT BUTTON";
    _keyCodeMap[WXK_RBUTTON] = "RIGHT BUTTON";
    _keyCodeMap[WXK_CANCEL] = "CANCEL";
    _keyCodeMap[WXK_MBUTTON] = "MIDDLE BUTTON";
    _keyCodeMap[WXK_CLEAR] = "CLEAR";
    _keyCodeMap[WXK_SHIFT] = "SHIFT";
    _keyCodeMap[WXK_ALT] = "ALT";
    _keyCodeMap[WXK_CONTROL] ="CONTROL";
    _keyCodeMap[WXK_MENU] = "MENU";
    _keyCodeMap[WXK_PAUSE] = "PAUSE";
    _keyCodeMap[WXK_CAPITAL] = "CAPITAL";
    _keyCodeMap[WXK_END] = "END";
    _keyCodeMap[WXK_HOME] = "HOME";
    _keyCodeMap[WXK_LEFT] = "LEFT";
    _keyCodeMap[WXK_UP] = "UP";
    _keyCodeMap[WXK_RIGHT] = "RIGHT";
    _keyCodeMap[WXK_DOWN] = "DOWN";
    _keyCodeMap[WXK_SELECT] = "SELECT";
    _keyCodeMap[WXK_PRINT] = "PRINT";
    _keyCodeMap[WXK_EXECUTE] = "EXECUTE";
    _keyCodeMap[WXK_SNAPSHOT] = "SNAPSHOT";
    _keyCodeMap[WXK_INSERT] = "INSERT";
    _keyCodeMap[WXK_HELP] = "HELP";
    _keyCodeMap[WXK_NUMPAD0] = "NUMPAD0";
    _keyCodeMap[WXK_NUMPAD1] = "NUMPAD1";
    _keyCodeMap[WXK_NUMPAD2] = "NUMPAD2";
    _keyCodeMap[WXK_NUMPAD3] = "NUMPAD3";
    _keyCodeMap[WXK_NUMPAD4] = "NUMPAD4";
    _keyCodeMap[WXK_NUMPAD5] = "NUMPAD5";
    _keyCodeMap[WXK_NUMPAD6] = "NUMPAD6";
    _keyCodeMap[WXK_NUMPAD7] = "NUMPAD7";
    _keyCodeMap[WXK_NUMPAD8] = "NUMPAD8";
    _keyCodeMap[WXK_NUMPAD9] = "NUMPAD9";
    _keyCodeMap[WXK_MULTIPLY] = "MULTIPLY";
    _keyCodeMap[WXK_ADD] = "ADD";
    _keyCodeMap[WXK_SEPARATOR] = "SEPARATOR";
    _keyCodeMap[WXK_SUBTRACT] = "SUBTRACT";
    _keyCodeMap[WXK_DECIMAL] = "DECIMAL";
    _keyCodeMap[WXK_DIVIDE] = "DIVIDE";
    _keyCodeMap[WXK_F1] = "F1";
    _keyCodeMap[WXK_F2] = "F2";
    _keyCodeMap[WXK_F3] = "F3";
    _keyCodeMap[WXK_F4] = "F4";
    _keyCodeMap[WXK_F5] = "F5";
    _keyCodeMap[WXK_F6] = "F6";
    _keyCodeMap[WXK_F7] = "F7";
    _keyCodeMap[WXK_F8] = "F8";
    _keyCodeMap[WXK_F9] = "F9";
    _keyCodeMap[WXK_F10] = "F10";
    _keyCodeMap[WXK_F11] = "F11";
    _keyCodeMap[WXK_F12] = "F12";
    _keyCodeMap[WXK_F13] = "F13";
    _keyCodeMap[WXK_F14] = "F14";
    _keyCodeMap[WXK_F15] = "F15";
    _keyCodeMap[WXK_F16] = "F16";
    _keyCodeMap[WXK_F17] = "F17";
    _keyCodeMap[WXK_F18] = "F18";
    _keyCodeMap[WXK_F19] = "F19";
    _keyCodeMap[WXK_F20] = "F20";
    _keyCodeMap[WXK_F21] = "F21";
    _keyCodeMap[WXK_F22] = "F22";
    _keyCodeMap[WXK_F23] = "F23";
    _keyCodeMap[WXK_F24] = "F24";
    _keyCodeMap[WXK_NUMLOCK] = "NUMLOCK";
    _keyCodeMap[WXK_SCROLL] = "SCROLL";
    _keyCodeMap[WXK_PAGEUP] = "PAGEUP";
    _keyCodeMap[WXK_PAGEDOWN] = "PAGEDOWN";
    _keyCodeMap[WXK_NUMPAD_SPACE] = "NUMPAD_SPACE";
    _keyCodeMap[WXK_NUMPAD_TAB] = "NUMPAD_TAB";
    _keyCodeMap[WXK_NUMPAD_ENTER] = "NUMPAD_ENTER";
    _keyCodeMap[WXK_NUMPAD_F1] = "NUMPAD_F1";
    _keyCodeMap[WXK_NUMPAD_F2] = "NUMPAD_F2";
    _keyCodeMap[WXK_NUMPAD_F3] = "NUMPAD_F3";
    _keyCodeMap[WXK_NUMPAD_F4] = "NUMPAD_F4";
    _keyCodeMap[WXK_NUMPAD_HOME] = "NUMPAD_HOME";
    _keyCodeMap[WXK_NUMPAD_LEFT] = "NUMPAD_LEFT";
    _keyCodeMap[WXK_NUMPAD_UP] = "NUMPAD_UP";
    _keyCodeMap[WXK_NUMPAD_RIGHT] = "NUMPAD_RIGHT";
    _keyCodeMap[WXK_NUMPAD_DOWN] = "NUMPAD_DOWN";
    _keyCodeMap[WXK_NUMPAD_PAGEUP] = "NUMPAD_PAGEUP";
    _keyCodeMap[WXK_NUMPAD_PAGEDOWN] = "NUMPAD_PAGEDOWN";
    _keyCodeMap[WXK_NUMPAD_END] = "NUMPAD_END";
    _keyCodeMap[WXK_NUMPAD_BEGIN] = "NUMPAD_BEGIN";
    _keyCodeMap[WXK_NUMPAD_INSERT] = "NUMPAD_INSERT";
    _keyCodeMap[WXK_NUMPAD_DELETE] = "NUMPAD_DELETE";
    _keyCodeMap[WXK_NUMPAD_EQUAL] = "NUMPAD_EQUAL";
    _keyCodeMap[WXK_NUMPAD_MULTIPLY] = "NUMPAD_MULTIPLY";
    _keyCodeMap[WXK_NUMPAD_ADD] = "NUMPAD_ADD";
    _keyCodeMap[WXK_NUMPAD_SEPARATOR] = "NUMPAD_SEPARATOR";
    _keyCodeMap[WXK_NUMPAD_SUBTRACT] = "NUMPAD_SUBTRACT";
    _keyCodeMap[WXK_NUMPAD_DECIMAL] = "NUMPAD_DECIMAL";
    _keyCodeMap[WXK_NUMPAD_DIVIDE] = "NUMPAD_DIVIDE";
    _keyCodeMap[WXK_WINDOWS_LEFT] = "WINDOWS_LEFT";
    _keyCodeMap[WXK_WINDOWS_RIGHT] = "WINDOWS_RIGHT";
    _keyCodeMap[WXK_WINDOWS_MENU] = "WINDOWS_MENU";
    _keyCodeMap[WXK_COMMAND] = "COMMAND";
}

int MakeKeyCode(int code, bool isShift, bool isCtrl, bool isAlt) {
    if (isShift) code+=100000;
    if (isCtrl)  code+=10000;
    if (isAlt)   code+=1000;
    return code;
}

long CheckKeyCode(int code, bool &isShift, bool &isCtrl, bool &isAlt) {
    isShift = isCtrl = isAlt = false;
    if ((code-100000) > 0) {
        isShift = true;
        code -= 100000;
    }
    if ((code-10000) > 0) {
        isCtrl = true;
        code -= 10000;
    }
    if ((code-1000) > 0) {
        isAlt = true;
        code -= 1000;
    }
    return code;
}

wxString GetKeyCodeDescription(int code) {

    // 1000+code = alt
    // 10000+code = ctrl
    // 100000+code = shift;

    if (_keyCodeMap.empty()) InitStringMap();
//    bool isShift=false, isCtrl=false, isAlt=false;

    wxString delim;
    wxString t;
    if ((code-100000) > 0) {
//        isShift = true;
        code -= 100000;
        t = delim+"SHIFT"; delim = "+";
    }
    if ((code-10000) > 0) {
//        isCtrl = true;
        code -= 10000;
        t += delim+"CTRL"; delim = "+";
    }
    if ((code-1000) > 0) {
//        isAlt = true;
        code -= 1000;
        t += delim+"ALT"; delim="+";
    }

    wxString v = _keyCodeMap[code];
    if (v.IsEmpty()) {
        t += delim+char(code);
    } else {
        t += delim+v;
    }
    return t;
}

