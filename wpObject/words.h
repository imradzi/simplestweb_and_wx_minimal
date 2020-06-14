#pragma once
#include "jsonval.h"
#include "jsonwriter.h"
#include "jsonreader.h"
#include <string>
#include "time.h"
#ifdef __USE_MSSQL__
	#include "oledb.h"
#endif
#include <vector>
#include <memory>

extern int wpFACTOR_TO_CENTS;
extern double wp_nDec;
extern double wp_nDec_display;

extern void SendFeedbackToMain(int id, void *clientData=NULL); // method from thread to ask for feedback into id=0 console, or id=1 = json.

#if defined(_MSC_VER) && (_MSC_VER <= 1600) // vs 2010
inline wxLongLong atoll(const wxString &x) { return _atoi64(x); }
#endif
wxLongLong GetValueInCents(const wxString &v); // this will return to the lowest denomination value;
wxLongLong GetValueInCents(double v); // this will return to the lowest denomination value;
wxLongLong GetValueInCents(long v);

wxLongLong GetValueInCents(const wxString& v);
wxString GetActualValue(const wxLongLong &v, int nDec=-1);
wxString GetActualValue(const wxString &s, int nDec=-1);
wxString GetKeyCodeDescription(int key);
wxString CheckFTSCharacters(const wxString &matchStr);
wxString BuildFTSSearch(const wxString &param);

wxString ReadStringFromFile(const wxString &fileName);
void WriteStringToFile(const wxString &fileName, const wxString &str);
extern wxString chartDirectorLicenseKey;
void InitChartLicense();
void LogTime( time_t &tStart, const wxChar *s );
long GetParamLong( const wchar_t *s, const wchar_t **p );
long GetParamLong( const char *s, const char **p );
const wchar_t *GetParamStr( const wchar_t *s, const wchar_t **p, int delta=2 );
wxString GetParamStr( const char *s, const char **p, int delta=2 );
long atol(const wchar_t *p, int len);
double atof(const wchar_t *p, int len);
long atol(const char *p, int len);
double atof(const char *p, int len);
wxString Format(const wxString &left, const wxString &right, int sz); // format the left and right for printing - in this case;
double RoundIt(double f, double nDec);
double TruncIt(double f, double nDec);
double GetNearestFiveCent(double v, bool truncate);
wxString GetNearestFiveCent(const wxString& t, bool truncate, int precision = 4);
double GetNearestTenCent(double v, bool truncate);
wxString GetNearestTenCent(const wxString& t, bool truncate, int precision = 4);

inline wxString DumpJSON(wxJSONValue &v) {wxJSONWriter writer;wxString s;writer.Write(v, s);return s;}
inline double GetRetirementCost(double principle, int atYear, double rate) {return principle * pow(1 - rate/100.0, atYear);}
inline double GetAnnualDepreciationCost(double principle, int atYear, double rate) {return GetRetirementCost(principle, atYear-1, rate) - GetRetirementCost(principle, atYear, rate);}
inline double GetAnnualDepreciationCostStraightLine(double principle, double nYear) {return principle/nYear;}
inline double GetCurrentAssetValueStraightLine(double principle, double atYear, double nYear ) {return principle - principle/nYear * atYear;}

wxChar ParsePeriod(const wxString &periodStr, long &year, long &periodNo); // returns freq;
//char ParsePeriod(const std::string &periodStr, long &year, long &periodNo); // returns freq;
std::vector<wxString> ParseCSV(const wxString &t);
wxString HTTPTranslateCharacterIn(const wxString &t);

size_t CompressFile(const wxString &filename, const wxString &outFile, int compressionLevel=9);
size_t UnCompressFile(const wxString &filename, const wxString &outFile);

int GetRandom(int b, int count, bool isSize);
double GetRandom(double lo, double hi);
long GetRandom(long lo, long hi);
wxString GetRandomWord(int nCharMax);
wxString GetRandomSentence(int nWordMax);
wxString GetRandomNumberString(int nDigitMax);

char GetCheckDigit(const wxString &d);
bool IsCheckDigitOK(const wxString &d);

wxString MakeBarcodeID(const wxString &receiveID, const wxString &packID, wxChar filler='9');
bool IsBarcodeOK(const wxString &id, wxChar filler='9');
bool ParseBarcode(const wxString &id, wxString &receiveID, wxString &packID, wxChar filler='9');

inline wxString MakeBarcodeWithoutReceiveID(const wxString &stockID, const wxString &packID) { return MakeBarcodeID(stockID, packID, '8'); }
inline bool IsBarcodeWithoutReceiveID(const wxString &id) { return IsBarcodeOK(id, '8'); }
inline bool ParseBarcodeWithoutReceiveID(const wxString &id, wxString &stockID, wxString &packID) { return ParseBarcode(id, stockID, packID, '8'); }

#ifdef __USE_MSSQL__
	std::string GetFormat(DBTIMESTAMP &t);
#endif
/*namespace Tools {
	class Tracer {
		time_t startTime;
		wxString traceName;
		std::ostream *outStr;
	public:
		Tracer(std::ostream *o, const wxChar *n="")):outStr(o), traceName(n) {
			time( &startTime );
		}
		~Tracer();
	};
}
*/

template <class T>
T MapNumericRange(T obs, T obsMin, T obsMax, T prjMin, T prjMax) {
	return
			(obs-obsMin)/(obsMax==obsMin ? 1 : obsMax-obsMin) * (prjMax-prjMin) + prjMin;
}

namespace String {
	std::shared_ptr<wxMemoryBuffer> ZipIt(const std::shared_ptr<wxMemoryBuffer> &str);
	std::shared_ptr<wxMemoryBuffer> UnzipIt(const std::shared_ptr<wxMemoryBuffer> &zipped);
	std::shared_ptr<wxMemoryBuffer> ZipIt(const void *b, size_t len);
	std::shared_ptr<wxMemoryBuffer> UnzipIt(const void *b, size_t len);

	wxMemoryBuffer ZipIt2(const wxMemoryBuffer &str);
	wxMemoryBuffer UnzipIt2(const wxMemoryBuffer &zipped);
	wxMemoryBuffer ZipIt2(const void *b, size_t len);
	wxMemoryBuffer UnzipIt2(const void *b, size_t len);

	std::vector<wxString> StringToVector(const wxString &x, const wxString &delim = "\t");
	wxString VectorToString(const std::vector<wxString> &v, const wxString &delim = "\t");

	wxString GetString(const wxJSONValue &v);
	inline wxString ToString(const wxJSONValue &v) { return GetString(v); }
	wxJSONValue ToJSON(const wxString &t);

	//wxMemoryBuffer ToBuffer(const wxString &s);
	std::shared_ptr<wxMemoryBuffer> ToBuffer(const wxString &s);
	wxMemoryBuffer ToBuffer2(const wxString &s);
	//wxMemoryBuffer JSONtoBuffer(const wxJSONValue &v);
	std::shared_ptr<wxMemoryBuffer> JSONtoBuffer(const wxJSONValue &v);
	wxLongLong ToLongLong(const wxString &s);
	long ToLong(const wxString &s);
	double ToDouble(const wxString &s);
	int CountChar(const wxString &text, wxChar ch);
	wxString GetFileType(const wxString &filename, int &noToSkip);
	bool IsUnicodeText(const wxString &filename);
	wxString LoadFileIntoString(const wxString &fName);

	bool IsValidDate(int year, int month, int day);

	wxString GetNameOnly(const wxChar *s);
	wxString GetNumberOnly(const wxChar *s);
	std::string GetNumber(const std::string &s);
	wxString GetNumber(const wchar_t *s);
	wxString TrimDecimal(const wxString s);

	std::string CombineAt2Ends(const wxChar *left, const wxChar *right, int len);
	void ReplaceSpaceWithDotsDownward(wxChar *start, wxChar *p);
	void Replace(wchar_t *dest, wchar_t from, wchar_t to);
	void Replace(char *dest, char from, char to);

	std::string FindOption(const wxChar *p, const wxChar *sswitch, bool isCaseSensitive=false); // for flat buffer
	const wxChar *FindSwitchValue(const wxChar *p, const wxChar *flag, bool isCaseSensitive=false); // for argv[]

	wxString FormatSpecial(double v);
	wxString FormatNumber( int f, int nDec=2, bool isSwapChar=false );
	wxString FormatNumber( const wxChar *s, int nDec=2, bool isSwapChar=false);
	wxString FormatNumber( double f, int nDec=2, bool isSwapChar=false );
	wxString ReplaceString( const wxChar *orig, const wxChar *key, const wxChar *with);
	inline wxString IntToString(long i) { return wxString::Format("%ld", i); }
	inline wxString DoubleToString(double i) { return wxString::Format("%lf", i); } // by default 6dec pt.
	void CopyChar( wxChar *buf, const wxChar *src, int len=999 );
	void ClearChar( wxChar *buf, int len);
	void ClearChar( wxChar *buf );

	wxString StripBracket(const wxString &c);
	const wchar_t *SkipUntilSpace(const wchar_t *p);
	const char *SkipUntilSpace(const char *p);
	const char *SkipUntil(char x, const char *p);
	const wchar_t *SkipUntil(wchar_t x, const wchar_t *p);
	const char *SkipWhile(char x, const char *p);
	const wchar_t *SkipWhile(wchar_t x, const wchar_t *p);

	const wchar_t *SkipUntilNumber(const wchar_t *p);
	const char *SkipUntilNumber(const char *p);

	const wchar_t *GetNumber(long &j, const wchar_t *p);
	const char *GetNumber(long &j, const char *p);

	const wxChar *SkipNonTabWhiteSpace(const wxChar *p);
	const wchar_t *CopyUntilSpace(wchar_t *toStr, const wchar_t *orig, size_t len=-1);
	const char *CopyUntilSpace(char *toStr, const char *orig, size_t len=-1);
	const wxChar *CopyUntilEqualOrSpace(wxChar *toStr, const wxChar *orig, size_t len=-1);
	const wxChar *CopyAttribute(wxChar *toStr, const wxChar *orig, size_t len=-1);
	const wxChar *CopyTag(wxChar *toStr, const wxChar *orig, size_t len=-1);
	const wxChar *CopyUntilTab(wxChar *toStr, const wxChar *orig, size_t len=-1);
	const wxChar *CopyUntilChar(wxChar *toStr, const wxChar *orig, char delim, size_t len=-1);
	const char *CopyUntilChar(char *toStr, const char *orig, char delim, size_t len);
	const wchar_t *CopyUntil(char v, wchar_t *toStr, const wchar_t *orig, size_t len=-1, bool skipWS=true, bool includeToken=false);
	const char *CopyUntil(char v, char *toStr, const char *orig, size_t len=-1, bool skipWS = true, bool includeToken = false);
	const wchar_t *CopyUntilCharOrSpace(wchar_t *toStr, const wchar_t *orig, wchar_t delim, size_t len=-1);
	const char *CopyUntilCharOrSpace(char *toStr, const char *orig, char delim, size_t len=-1);
	void strncpysz(wxChar *dest, const wxChar *orig, size_t len);
	void strncpysz(char *dest, const char *orig, size_t len);
	int CountWords(const wxChar *orig);
	const wxChar *stristr(const wxChar *u, const wxChar *s);
	//const wchar_t *SkipWhiteSpace(const wchar_t *p);
	//const char *SkipWhiteSpace(const char *p);
	const wxChar *SkipWhiteSpace(const wxChar *p);
	const char *SkipWhiteSpace(const char *p);
	const wxChar *SkipEOL( const wxChar *p );

	wchar_t *Trim(wchar_t *s);
	std::string Trim(const std::string &s);
	char *Trim(char *s);
	char *ToLower(char *s);
	wchar_t *ToLower(wchar_t *s);
	std::string ToLower(const std::string &s);

	std::string GetString(const wxChar *buf, long len);
	void TrimString(std::string &s);
	bool IsEmpty(const wchar_t *s, int len);
	bool IsEmpty(const char *s, int len);
	bool IsEmpty(const wxChar *s);
	bool IsEmpty(const wxString &s);
//	bool IsEmpty(const char *s);
//	bool IsEmpty(const wxChar *s, int len);
//	bool IsEmpty(const wxChar *s);
	bool IsNumeric(const wchar_t *s, int len);
	bool IsNumeric(const char *s, int len);
	bool IsNumeric(const char *s);
	bool IsNumeric(const wchar_t *s);
	bool IsNumeric(const wxString &s);
	bool IsValidNumber(const wxChar *s);
	int Compare(const wxChar *s1, const wxChar *s2);
	class Words {
		wxChar *str;
	public:
		Words(const wxChar *s, size_t len=-1);
		Words(Words &);
		virtual ~Words();
		operator wxChar *() { return str; }
	};

	class WordList {
		wxChar **w;
	int nWords;
	public:
		WordList(const wxChar *str, size_t len=-1);
		virtual ~WordList();
		bool ExistIn(WordList &wL);
		bool ExistIn(const wxChar *s, bool skipFirstWord=true, size_t len=-1);
		const wxChar *operator[](int i);
		int NumWords() { return nWords; }
	};
}

#define EOFFIELDCHAR char(0x1E)
#define EOLINECHAR char(0x1F)

class ReceiptPrinterReport {
	wxString res;
	wxChar eof;
	wxChar eol;
public:
	ReceiptPrinterReport(wxChar df = EOFFIELDCHAR, wxChar dlin=EOLINECHAR) : eof(df), eol(dlin) {}
	void PrintLine() {res.Append(eol);}
	void PrintLine(const wxString &s) {res.Append(s);res.Append(eol);}
	void PrintLine(const wxString &s1, const wxString &s2) {res.Append(s1);res.Append(eof); res.Append(s2); res.Append(eol);}
	void PrintLine(const wxString &s1, const wxString &s2, const wxString &s3) {res.Append(s1); res.Append(eof); res.Append(s2); res.Append(eof); res.Append(s3); res.Append(eol);}
	const wxString GetString() { return res; }
};
