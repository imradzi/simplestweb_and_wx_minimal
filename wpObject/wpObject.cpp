#ifdef _WIN32
#include "winsock2.h"
#endif
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif
#include "wx/tokenzr.h"
#include "jsonval.h"
#include "wx/wfstream.h"
#include "wx/sstream.h"
#include "wx/zipstrm.h"
#include "wx/zstream.h"

#include <iostream>
#include <algorithm>
#include "WORDS.H"
#include "xmlParser.h"
#include "wpObject.h"
#include "net.h"

#include "jsonval.h"
#include "jsonreader.h"
#include "jsonwriter.h"

extern bool gVerbose;
extern int wpFACTOR_TO_CENTS;
extern double wp_nDec;
extern double wp_nDec_display;
int g_appType;

char wpObject::attributeMark = '\t';
char wpObject::valueMark = '\n';
const wxString wpLogicalPathSeparator = "#";

wxString GetSoftwareVersion() {
	if (gRunningData.wpCustomizedSoftwareName.IsEmpty()) return wpVersionString;
	//const char* wpVersionString = "PharmaPOS Client Ver 2.01 Build 290 [1ede8edc32] Compiled on 2019-07-29 10:38:12 GMT+8";
	// need to remove the first word and append it to custom name.

	if (gRunningData.wpCustomizedSoftwareName.Contains("Compiled ")) return gRunningData.wpCustomizedSoftwareName;

	wxStringTokenizer tok(wpVersionString, " ");
	wxString ver = gRunningData.wpCustomizedSoftwareName;
	tok.GetNextToken(); // skip first word
	while (tok.HasMoreTokens()) {
		ver.Append(" " + tok.GetNextToken());
	}
	return ver;
}

//namespace MessagePrefix {
//	const wxString broadcast = "{bc}";
//	const wxString cont = "{c}";
//	const wxString command = "{cm}";
//	const wxString reply = "{re}";
//	bool IsPrefix(const wxString& x) {
//		static wxString allPrefix = broadcast + cont + command + reply;
//		return allPrefix.Find(x) != wxNOT_FOUND;
//	}
//}

char GetAttributeMark() { return wpObject::attributeMark; }

wxString ReceiveCommandMethod[] = { "purchaseorder","invoice","suspend" };
wxString SalesCommandMethod[] = {"","suspend","quotation"};
wxString SalesTableName[] = { "Sales", "SuspendedSales", "Quotation" };
wxString ReceiveTableName[] = { "PurchaseOrder", "Receive", "SuspendedReceive" };
wxString ReceiveTableKey[] = { "purchaseID", "receiveID", "receiveID" };

int wpObject::maxFTSColumnSize = PersonEntity::eof - PersonEntity::code + 1;


void wpObject::Clear() {
	Init();
	for (auto &&it : mapString) {
		it.second->Clear();
	}
	for (auto &&it: mapDate) {
		*it.second = wxLongLong(wxINT64_MIN); // invalid date;
	}
	for (auto&& it: mapLongLong) {
		*it.second = wxLongLong(0);
	}
	for (auto&& it : mapInt) {
		*it.second = 0;
	}
	for (auto&& it : mapLong) {
		*it.second = 0;
	}
	for (auto&& it : mapDouble) {
		*it.second = 0.0;
	}
	for (auto&& it : mapUUID) {
		it.second->Clear();
	}
	for (auto&& it : mapBool) {
		*it.second = false;
	}
}


// each fields are separated by CRLF; if multiple values _ eg Address _ use TAB

wxString wpObject::ConvertValueMarkToCRLF(const wxString &s) {
	wxString res = s;
	res.Replace(valueMark, "\n");
	return res;
}

wxString wpObject::ConvertCRLFToValueMark(const wxString &s) {
	wxString res = s;
	res.Replace("\n", valueMark);
	return res;
}

// build ftsColumn from content string
void wpObject::BuildFTSColumnFromString(const wxString &content) {
	ftsColumn.clear();
	wxStringTokenizer tk(content, attributeMark, wxTOKEN_RET_EMPTY_ALL);
	int i=0;
	for (; tk.HasMoreTokens(); i++) {
		ftsColumn.push_back(tk.GetNextToken());
	}
	for (; i < GetNumberOfFTSColumn(); i++) {
		ftsColumn.push_back("");
	}
}

// save ftsColumn into content string
void wpObject::ExportFTSColumnToString(wxString &content) const {
	wxString sep;
	content.Clear();
	for (const auto it: ftsColumn) {
		content.Append(sep);
		content.Append(it);
		sep=attributeMark;
	}
}

extern void ShowLog(const wxString&);

void wpObject::CheckKeyExists(const wxString& key) {
	if (keyExists.find(key) == keyExists.end()) {
		keyExists.insert(key);
		return;
	}
	wxString p = "data key for serialization already exists! "+key;
	ShowLog(p);
	throw (p);
}

bool wpObject::IsEmpty() {
	ExportFTSColumn();
	for (const auto &it : mapString) {
		if (!String::IsEmpty(*it.second)) return false;
	}
	for (auto const &it : mapDate) {
		if (it.second->IsValid()) {
			if (it.second->GetValue() != wxLongLong(0)) return false;
		}
	}
	for (auto const& it : mapLongLong) {
		if (*it.second != 0LL) return false;
	}
	for (auto const& it : mapInt) {
		if (*it.second != 0)
			return false;
	}
	for (auto const& it : mapLong) {
		if (*it.second != 0)
			return false;
	}
	for (auto const& it : mapDouble) {
		if (*it.second != 0.0) return false;
	}
	for (auto const& it : mapUUID) {
		if (!it.second->IsEmpty()) return false;
	}
	// mapBool - can't tell from true or false if it's empty
	return true;
}

void wpObject::Clone(wpObject *r) {
	if (!r) return;
	BuildFTSColumn();
	for (const auto &it: mapString) {
		wxString *s = r->FindInString(it.first);
		if (s) *s = it.second->Clone();
	}
	for (auto const it: mapDate) {
		wxDateTime *s = r->FindInDateTime(it.first);
		if (s && it.second->IsValid()) *s = *it.second;
	}
	for (auto const it : mapLongLong) {
		wxLongLong *s = r->FindInLongLong(it.first);
		if (s) *s = *it.second;
	}
	for (auto const it : mapInt) {
		int *s = r->FindInInt(it.first);
		if (s) *s = *it.second;
	}
	for (auto const it : mapLong) {
		long* s = r->FindInLong(it.first);
		if (s) *s = *it.second;
	}
	for (auto const it : mapDouble) {
		double *s = r->FindInDouble(it.first);
		if (s) *s = *it.second;
	}
	for (auto const it : mapBool) {
		bool* s = r->FindInBool(it.first);
		if (s) *s = *it.second;
	}
	for (auto const it : mapUUID) {
		UniversalUniqueID* s = r->FindInUUID(it.first);
		if (s) *s = *it.second;
	}

	r->ftsColumn = ftsColumn;
}

void wpObject::CopyToEmptyFieldOnly(const wpObject &r) {
	for (auto &&it: mapString) {
		if (it.second->IsEmpty()) {
			const wxString *s = r.FindInString(it.first);
			if (s)
				*it.second = *s;
		}
	}
	for (auto &&it: mapDate) {
		if (!it.second->IsValid()) {
			const wxDateTime *s = r.FindInDateTime(it.first);
			if (s && s->IsValid())
				*it.second = *s;
		}
	}
	for (auto &&it : mapLongLong) {
		if (it.second == 0) {
			const wxLongLong *s = r.FindInLongLong(it.first);
			if (s) *it.second = *s;
		}
	}
	for (auto &&it : mapInt) {
		if (it.second == 0) {
			const int *s = r.FindInInt(it.first);
			if (s) *it.second = *s;
		}
	}
	for (auto&& it : mapLong) {
		if (it.second == 0) {
			const long* s = r.FindInLong(it.first);
			if (s) *it.second = *s;
		}
	}
	for (auto&& it : mapBool) {
		const bool* s = r.FindInBool(it.first);
		if (s) *it.second = *s;
	}
	for (auto&& it : mapDouble) {
		if (it.second == 0) {
			const double *s = r.FindInDouble(it.first);
			if (s) *it.second = *s;
		}
	}
	for (auto&& it : mapUUID) {
		if (it.second->IsEmpty()) {
			const UniversalUniqueID *s = r.FindInUUID(it.first);
			if (s) *it.second = *s;
		}
	}
}

wxString *wpObject::FindInString(const wxString &e) {
	auto const it = mapString.find(e);
	if (it == mapString.end()) return NULL;
	return it->second;
}

wxDateTime *wpObject::FindInDateTime(const wxString &e) {
	auto const it = mapDate.find(e);
	if (it == mapDate.end()) return NULL;
	return it->second;
}

wxLongLong *wpObject::FindInLongLong(const wxString &e) {
	auto const it = mapLongLong.find(e);
	if (it == mapLongLong.end()) return NULL;
	return it->second;
}

int *wpObject::FindInInt(const wxString &e) {
	auto const it = mapInt.find(e);
	if (it == mapInt.end()) return NULL;
	return it->second;
}

long* wpObject::FindInLong(const wxString& e) {
	auto const it = mapLong.find(e);
	if (it == mapLong.end()) return NULL;
	return it->second;
}

bool* wpObject::FindInBool(const wxString& e) {
	auto const it = mapBool.find(e);
	if (it == mapBool.end()) return NULL;
	return it->second;
}

double *wpObject::FindInDouble(const wxString &e) {
	auto const it = mapDouble.find(e);
	if (it == mapDouble.end()) return NULL;
	return it->second;
}

UniversalUniqueID* wpObject::FindInUUID(const wxString& e) {
	auto const it = mapUUID.find(e);
	if (it == mapUUID.end()) return NULL;
	return it->second;
}

wxString wpObject::GetKey(void *v) const {
	auto const it = mapPointer.find(v);
	if (it == mapPointer.end()) return wxEmptyString;
	return it->second;
}

bool wpObject::SetValue(const wxString& key, const wxString& value) {
	wxString* x = FindInString(key);
	if (x) { *x = value; return true; }

	wxDateTime* dt = FindInDateTime(key);
	if (dt) {
		wxString::const_iterator it;
		if (dt->ParseDateTime(value, &it)) return true;
		else return false;
	}
	wxLongLong* ll = FindInLongLong(key);
	if (ll) {
		*ll = String::ToLongLong(value);
		return true;
	}
	int* i = FindInInt(key);
	if (i) {
		*i = wxAtoi(value);
		return true;
	}

	long*l = FindInLong(key);
	if (l) {
		*l = wxAtol(value);
		return true;
	}

	bool* b = FindInBool(key);
	if (b) {
		*b = value.IsSameAs("1") || value.IsSameAs("TRUE", false) || value.IsSameAs("ON", false) || value.IsSameAs("YES", false);
		return true;
	}

	double* f = FindInDouble(key);
	if (f) {
		*f = wxAtof(value);
		return true;
	}
	UniversalUniqueID* uuid = FindInUUID(key);
	if (uuid) {
		uuid->Import(value);
		return true;
	}
	return false;
}

const wxString *wpObject::FindInString(const wxString &e) const {
	auto it = mapString.find(e);
	if (it == mapString.end()) return nullptr;
	return it->second;
}

const wxDateTime *wpObject::FindInDateTime(const wxString &e) const{
	auto const it = mapDate.find(e);
	if (it == mapDate.end()) return nullptr;
	return it->second;
}

const wxLongLong *wpObject::FindInLongLong(const wxString &e) const{
	auto const it = mapLongLong.find(e);
	if (it == mapLongLong.end()) return nullptr;
	return it->second;
}

const int *wpObject::FindInInt(const wxString &e) const{
	auto const it = mapInt.find(e);
	if (it == mapInt.end()) return nullptr;
	return it->second;
}

const long *wpObject::FindInLong(const wxString& e) const {
	auto const it = mapLong.find(e);
	if (it == mapLong.end()) return nullptr;
	return it->second;
}

const bool *wpObject::FindInBool(const wxString& e) const {
	auto const it = mapBool.find(e);
	if (it == mapBool.end()) return nullptr;
	return it->second;
}

const double *wpObject::FindInDouble(const wxString &e) const{
	auto const it = mapDouble.find(e);
	if (it == mapDouble.end()) return nullptr;
	return it->second;
}

const UniversalUniqueID *wpObject::FindInUUID(const wxString& e) const {
	auto const it = mapUUID.find(e);
	if (it == mapUUID.end()) return nullptr;
	return it->second;
}

wpObject::wpObject():nodeType(wxXML_ELEMENT_NODE), isDeleted(false), isNewObject(false), isUpdatedRemote(false), updateFromRemote(false), toUpdateRemote(false) {
	ftsColumn.resize(maxFTSColumnSize);
	Init();
	Insert("wpOperation", &ops);
	Insert("wpMethod", &method);
	Insert("wpSearch", &searchParam);
	Insert("wpLinkID", &linkID);
	Insert("wpIsSync", &sync);
	Insert("wpOption", &option);
	Insert("wpReturnCode", &returnCode);
	Insert("wpErrorMessage", &errorMessage);
	Insert("md5signature", &md5signature);
	Insert("lastUpdateDateTime", &lastUpdateDateTime);
	Insert("serverIDtoRun", &serverIDtoRun);
	Insert("groupIDtoRun", &groupIDtoRun);
	Insert("originServer", &originServer);
	Insert("senderID", &senderID);
	Insert("isDeleted", &isDeleted);
	Insert("updateFromRemote", &updateFromRemote);
}

wpObject::wpObject(const wpObject &x): isUpdatedRemote(x.isUpdatedRemote), toUpdateRemote(x.toUpdateRemote) {
	ftsColumn.resize(maxFTSColumnSize);
	Init();
	Insert("wpOperation", &ops);
	Insert("wpMethod", &method);
	Insert("wpSearch", &searchParam);
	Insert("wpLinkID", &linkID);
	Insert("wpIsSync", &sync);
	Insert("wpOption", &option);
	Insert("wpReturnCode", &returnCode);
	Insert("wpErrorMessage", &errorMessage);
	Insert("md5signature", &md5signature);
	Insert("lastUpdateDateTime", &lastUpdateDateTime);
	Insert("serverIDtoRun", &serverIDtoRun);
	Insert("groupIDtoRun", &groupIDtoRun);
	Insert("originServer", &originServer);
	Insert("senderID", &senderID);
	Insert("isDeleted", &isDeleted);
	Insert("updateFromRemote", &updateFromRemote);
	operator=(x);
}

wpObject &wpObject::operator=(const wpObject &x) {
	nodeType = x.nodeType;
	for (const auto &it: x.mapString) {
		wxString *s = FindInString(it.first);
		if (s) *s = *it.second;
	}
	for (auto const &it: x.mapDate) {
		wxDateTime *s = FindInDateTime(it.first);
		if (s && it.second->IsValid()) *s = *it.second;
	}
	for (auto const& it : x.mapLongLong) {
		wxLongLong *s = FindInLongLong(it.first);
		if (s) *s = *it.second;
	}
	for (auto const& it : x.mapInt) {
		int *s = FindInInt(it.first);
		if (s) *s = *it.second;
	}
	for (auto const& it : x.mapLong) {
		long* s = FindInLong(it.first);
		if (s) *s = *it.second;
	}
	for (auto const& it : x.mapDouble) {
		double *s = FindInDouble(it.first);
		if (s) *s = *it.second;
	}
	for (auto const& it : x.mapUUID) {
		UniversalUniqueID* s = FindInUUID(it.first);
		if (s) *s = *it.second;
	}
	for (auto const& it : x.mapBool) {
		bool* s = FindInBool(it.first);
		if (s) *s = *it.second;
	}
	ftsColumn = x.ftsColumn;
	isUpdatedRemote = x.isUpdatedRemote;
	toUpdateRemote = x.toUpdateRemote;
	return *this;
}

bool wpObject::LoadXMLNode(const wxXmlNode *node) {
	Clear();
	if (!node) return false;
	wxString *s;
	wxDateTime *dt;
	wxLongLong *ll;
	UniversalUniqueID* uuid;
	int *i;
	long* l;
	double *d;
	bool *b;
	for (const wxXmlAttribute *p = node->GetAttributes(); p; p=p->GetNext()) {
		if ((s = FindInString(p->GetName())) != NULL) {
			*s = p->GetValue();
		} else if ((dt = FindInDateTime(p->GetName())) != NULL) {
			*dt = String::ToLongLong(p->GetValue());
		} else if ((ll = FindInLongLong(p->GetName())) != NULL) {
			*ll = String::ToLongLong(p->GetValue());
		} else if ((i = FindInInt(p->GetName())) != NULL) {
			*i = wxAtoi(p->GetValue());
		} else if ((l = FindInLong(p->GetName())) != NULL) {
			*l = wxAtol(p->GetValue());
		} else if ((d = FindInDouble(p->GetName())) != NULL) {
			*d = String::ToDouble(p->GetValue());
		} else if ((uuid = FindInUUID(p->GetName())) != NULL) {
			uuid->Import(p->GetValue());
		} else if ((b = FindInBool(p->GetName())) != NULL) {
			wxString v = p->GetValue();
			*b = v.IsSameAs("1") || v.IsSameAs("TRUE", false) || v.IsSameAs("ON", false) || v.IsSameAs("YES", false);
		} else
			LoadUnregisteredXML(p->GetName(), p->GetValue());
	}
	BuildFTSColumn(); // populate ftsColumn;
	return true;
}

bool wpObject::GetXMLNode(wxXmlNode *node) {
	if (!node) return false;
	ExportFTSColumn(); // load content from ftsColumms;
	for (const auto &it : mapString) {
		if (!it.second->IsEmpty()) {
			wxString x = *it.second;
			for (wxString::iterator s_it = x.begin(); s_it != x.end(); s_it++) {
				if (!wxIsprint(*s_it) && *s_it != attributeMark && *s_it != valueMark && *s_it != '\n' && *s_it != '\t')
					*s_it = '?';
			}
			node->AddAttribute(it.first, x);
		}
	}
	for (auto const& it : mapDate) {
		wxDateTime *s = it.second;
		if (s && s->IsValid())
			node->AddAttribute(it.first, s->GetValue().ToString());
	}
	for (auto const& it : mapLongLong) {
		wxLongLong *s = it.second;
		if (s)
			node->AddAttribute(it.first, s->ToString());
	}
	for (auto const& it : mapInt) {
		int *s = it.second;
		if (s)
			node->AddAttribute(it.first, String::IntToString(*s));
	}
	for (auto const& it : mapLong) {
		long* s = it.second;
		if (s)
			node->AddAttribute(it.first, String::IntToString(*s));
	}
	for (auto const& it : mapDouble) {
		double *s = it.second;
		if (s)
			node->AddAttribute(it.first, String::DoubleToString(*s));
	}
	for (auto const& it : mapUUID) {
		UniversalUniqueID* s = it.second;
		if (s && !s->IsEmpty())
			node->AddAttribute(it.first, (*s)());
	}
	for (auto const& it : mapBool) {
		bool* s = it.second;
		if (s)
			node->AddAttribute(it.first, *s ? "TRUE" : "FALSE");
	}
	return true;
}

bool wpObject::LoadJSONString(const wxString &s) {
	wxJSONReader jr;
	wxJSONValue jv;
	if (jr.Parse(s, &jv) == 0) {
		return LoadJSON(jv);
	}
	return false;
}

bool wpObject::LoadJSON(const wxJSONValue &v) {
	if (v.GetType() != wxJSONTYPE_OBJECT) return false;
	Clear();
	wxString *s;
	wxDateTime *dt;
	wxLongLong *ll;
	UniversalUniqueID* uuid;
	int *i;
	long* l;
	double *d;
	bool *b;
	const wxJSONInternalMap *map = v.AsMap();
	for (wxJSONInternalMap::const_iterator it = map->begin(); it != map->end(); it++) {
#ifdef _DEBUG
		wxString keyvalue = it->first;
#endif
		if ((s = FindInString(it->first)) != NULL) {
			*s = it->second.AsString();
		} else if ((dt = FindInDateTime(it->first)) != NULL) {
			*dt = String::ToLongLong(it->second.AsString());
		} else if ((ll = FindInLongLong(it->first)) != NULL) {
			*ll = String::ToLongLong(it->second.AsString());
		} else if ((i = FindInInt(it->first)) != NULL) {
			*i = it->second.AsInt();
		} else if ((l = FindInLong(it->first)) != NULL) {
			*l = it->second.AsLong();
		} else if ((d = FindInDouble(it->first)) != NULL) {
			*d = it->second.AsDouble();
		} else if ((uuid = FindInUUID(it->first)) != NULL) {
			uuid->Import(it->second.AsString());
		} else if ((b = FindInBool(it->first)) != NULL) {
			wxString val = it->second.AsString();
			*b = val.IsSameAs("TRUE", false || val.IsSameAs("YES", false) || val.IsSameAs("1") || val.IsSameAs("ON", false));
		}
	}
	BuildFTSColumn(); // populate ftsColumn;
	return true;
}

wxString wpObject::GetJSONString() {
	wxJSONValue jv = GetJSON();
	wxJSONWriter jw;
	wxString str;
	jw.Write(jv, str);
	return str;
}

wxJSONValue wpObject::GetJSON() {
	ExportFTSColumn(); // load content from ftsColumms;
	wxJSONValue val;
	for (auto const &it : mapString) {
		if (!it.second->IsEmpty()) {
			wxString x = *it.second;
			for (wxString::iterator s_it = x.begin(); s_it != x.end(); s_it++) {
				if (!wxIsprint(*s_it) && *s_it != attributeMark && *s_it != valueMark && *s_it != '\n' && *s_it != '\t')
					*s_it = '?';
			}
			val[it.first] = wxString(x);
		}
	}
	for (auto const& it : mapDate) {
		wxDateTime *s = it.second;
		if (s && s->IsValid())
			val[it.first] = wxString(s->GetValue().ToString());
	}
	for (auto const& it : mapLongLong) {
		wxLongLong *s = it.second;
		if (s)
			val[it.first] = wxString(s->ToString());
	}
	for (auto const& it : mapInt) {
		int *s = it.second;
		if (s)
			val[it.first] = *s;
	}
	for (auto const& it : mapLong) {
		long* s = it.second;
		if (s)
			val[it.first] = *s;
	}
	for (auto const& it : mapDouble) {
		double *s = it.second;
		if (s)
			val[it.first] = *s;
	}
	for (auto const& it : mapUUID) {
		UniversalUniqueID* s = it.second;
		if (s && !s->IsEmpty())
			val[it.first] = (*s)();
	}
	for (auto const& it : mapBool) {
		bool* s = it.second;
		val[it.first] = *s ? "TRUE" : "FALSE";
	}
	return val;
}

PairRecord::PairRecord() {
	Init();
	Insert("pairKey", &key);
	Insert("pairVal", &value);
}

PairRecord::PairRecord(const PairRecord &rec): wpObject(rec) {
	Init();
	Insert("pairKey", &key);
	Insert("pairVal", &value);
	operator=(rec);
}

Parameter::Parameter(): stockParam() {
	Init();
	Insert("tablename", &tableName);
	Insert("id", &id);
	Insert("intParam", &intParam);
	Insert("data", &data);
	Insert("stocktype", &stockParam.stockType);
	Insert("poisoncategory", &stockParam.poisonCategory);
	Insert("ispoison", &stockParam.isPoison);
	Insert("ispseudopoison", &stockParam.isPseudoPoison);
	Insert("showCost", &stockParam.showCost);
}

Parameter::Parameter(const Parameter &p) : wpObject(p),  stockParam(p.stockParam){
	Init();
	Insert("PRM_tablename", &tableName);
	Insert("id", &id);
	Insert("intParam", &intParam);
	Insert("data", &data);
	Insert("stocktype", &stockParam.stockType);
	Insert("poisoncategory", &stockParam.poisonCategory);
	Insert("ispoison", &stockParam.isPoison);
	Insert("ispseudopoison", &stockParam.isPseudoPoison);
	Insert("showCost", &stockParam.showCost);
	operator=(p);
}

TableDependencyRecord::TableDependencyRecord() {
	Init();
	Insert("table", &tableName);
	Insert("column", &columnName);
}

TableDependencyRecord::TableDependencyRecord(const TableDependencyRecord &x): wpObject(x) {
	Init();
	Insert("table", &tableName);
	Insert("column", &columnName); operator=(x);
}

SQLCommandRecord::SQLCommandRecord() {
	Init();
	Insert("sql", &sql);
	Insert("sql2", &sqlSelect);
	Insert("count", &sqlCount);
}

SQLCommandRecord::SQLCommandRecord(const SQLCommandRecord &x) : wpObject(x) {
	Init();
	Insert("sql", &sql);
	Insert("sql2", &sqlSelect);
	Insert("count", &sqlCount);
	operator=(x);
}

PassportRecord::PassportRecord() {
	Init();
	Insert("id", &id);
	Insert("raw", &rawData);
}

PassportRecord::PassportRecord(const PassportRecord &x): wpObject(x) {
	Init();
	Insert("id", &id);
	Insert("raw", &rawData);
	operator=(x);
}

DatabaseRecord::DatabaseRecord() {
	Init();
	Insert("fileName", &dbName);
	Insert("transaction", &transactionName);
	Insert("script-folder", &scriptFolder);
	Insert("www-folder", &wwwFolder);
}

DatabaseRecord::DatabaseRecord(const DatabaseRecord &r) : LexiconRecord(r) {
	Init();
	Insert("fileName", &dbName);
	Insert("transaction", &transactionName);
	Insert("script-folder", &scriptFolder);
	Insert("www-folder", &wwwFolder);
	operator=(r);
}

DatabaseRecord::DatabaseRecord(const wxString &i, const wxString &c, const wxString &n) :LexiconRecord(i, c, n) {
	Insert("fileName", &dbName);
	Insert("transaction", &transactionName);
	Insert("script-folder", &scriptFolder);
	Insert("www-folder", &wwwFolder);
}

DatabaseRecord::DatabaseRecord(long i, const wxString &c, const wxString &n) :LexiconRecord(i, c, n) {
	Insert("fileName", &dbName);
	Insert("transaction", &transactionName);
	Insert("script-folder", &scriptFolder);
	Insert("www-folder", &wwwFolder);
}

BranchRecord::BranchRecord() {
	Init();
	Insert("trans", &transDB);
	Insert("url", &url);
}

BranchRecord::BranchRecord(const BranchRecord &r) : LexiconRecord(r) {
	Init();
	Insert("trans", &transDB);
	Insert("url", &url);
	operator=(r);
}

ShopRecord::ShopRecord() {
	Init();
	Insert("master", &masterDB);
	Insert("url", &url);
}

ShopRecord::ShopRecord(const ShopRecord &r) : LexiconRecord(r) {
	Init();
	Insert("master", &masterDB);
	Insert("url", &url);
	operator=(r);
}

ShopRecord &ShopRecord::operator=(const ShopRecord &x) {
	LexiconRecord::operator=(x);
	branches = x.branches;
	return *this;
}
bool ShopRecord::LoadXMLNode(const wxXmlNode *tg) {
	if (!LexiconRecord::LoadXMLNode(tg)) return false;
	for (const wxXmlNode *t = tg->GetChildren(); t; t = t->GetNext()) {
		if (t->GetName().IsSameAs("branches")) {
			for (const wxXmlNode *ct = t->GetChildren(); ct; ct = ct->GetNext()) {
				std::shared_ptr<BranchRecord> r(new BranchRecord);
				r->LoadXMLNode(ct);
				branches.push_back(r);
			}
		}
	}
	return true;
}

bool ShopRecord::GetXMLNode(wxXmlNode *tg) {
	if (!LexiconRecord::GetXMLNode(tg)) return false;
	if (branches.size() > 0) {
		wxXmlNode *c = new wxXmlNode(tg, nodeType, "branches");
		for (auto &&it : branches) {
			wxXmlNode *ct = new wxXmlNode(c, nodeType, "Row");
			it->GetXMLNode(ct);
		}
	}
	return true;
}

void ShopRecord::Clone(wpObject *p) {
	LexiconRecord::Clone(p);
	ShopRecord *r = dynamic_cast<ShopRecord *>(p);
	if (r) {
		r->branches.clear();
		for (auto const &it:  branches) {
			r->branches.push_back(it);
		}
	}
}


AccountJournalRecord::AccountJournalRecord() {
	Init();
	Insert("invoice", &invoice);
	Insert("date", &date);
	Insert("invAmt", &invAmount);
	Insert("payAmt", &payAmount);
	Insert("remark", &remark);
}

AccountJournalRecord::AccountJournalRecord(const AccountJournalRecord &rec): EntityRecord(rec) {
	Init();
	Insert("invoice", &invoice);
	Insert("date", &date);
	Insert("invAmt", &invAmount);
	Insert("payAmt", &payAmount);
	Insert("remark", &remark);
	operator=(rec);
}

AccountTransactionRecord::AccountTransactionRecord() : wpObject() {
	Init();
	Insert("id", &id);
	Insert("direction", &type);
	Insert("seqno", &seqNo);
	Insert("note", &note);
	Insert("totalamount", &totalAmount);
	Insert("refno", &refNo);
	Insert("remark", &remark);
	Insert("transdate", &transactionDate);
}
AccountTransactionRecord::AccountTransactionRecord(const AccountTransactionRecord &rec): wpObject(rec) {
	Init();
	Insert("id", &id);
	Insert("direction", &type);
	Insert("seqno", &seqNo);
	Insert("note", &note);
	Insert("totalamount", &totalAmount);
	Insert("refno", &refNo);
	Insert("remark", &remark);
	Insert("transdate", &transactionDate);
	operator=(rec);
}

AccountTransactionRecord &AccountTransactionRecord::operator=(const AccountTransactionRecord &r) {
	wpObject::operator=(r);
	entityFrom = r.entityFrom;
	entityTo = r.entityTo;
	list = r.list;
	return *this;
}

bool AccountTransactionRecord::LoadXMLNode(const wxXmlNode *tg) {
	if (!wpObject::LoadXMLNode(tg)) return false;
	for (const wxXmlNode *tR = tg->GetChildren(); tR; tR = tR->GetNext()) {
		if (tR->GetName().IsSameAs("transactions")) {
			for (const wxXmlNode *row = tR->GetChildren(); row; row = row->GetNext()) {
				std::shared_ptr <AccountJournalRecord> rec(new AccountJournalRecord);
				rec->LoadXMLNode(row);
				list.push_back(rec);
			}
		}
		else if (tR->GetName().IsSameAs("fromaccount")) {
			entityFrom.LoadXMLNode(tR);
		}
		else if (tR->GetName().IsSameAs("toaccount")) {
			entityTo.LoadXMLNode(tR);
		}
	}
	return true;

}

bool AccountTransactionRecord::GetXMLNode(wxXmlNode *tg) {
	if (!wpObject::GetXMLNode(tg)) return false;
	if (!list.empty()) {
		wxXmlNode *tR = XMLTag::AddChild(tg, "transactions");
		for (auto &&it : list) {
			it->GetXMLNode(XMLTag::AddChild(tR, "row"));
		}
	}
	if (!entityFrom.IsEmpty()) {
		entityFrom.GetXMLNode(XMLTag::AddChild(tg, "fromaccount"));
	}
	if (!entityTo.IsEmpty()) {
		entityTo.GetXMLNode(XMLTag::AddChild(tg, "toaccount"));
	}
	return true;
}

bool AccountTransactionRecord::IsEmpty() {
	if (!wpObject::IsEmpty()) return false;
	for (auto const &it : list) {
		if (!it->IsEmpty()) return false;
	}
	if (!entityFrom.IsEmpty()) return false;
	if (!entityTo.IsEmpty()) return false;
	return true;
}

/*
	wxString id;
	wxString note;
	wxString refNo;
	CompanyRecord supplier;
	wxString term;
	wxString remark;
	TypeRecord purchaseType;
	wxDateTime transactionDate;
	std::vector<StockTransactionRecord> list;
	std::vector<PaymentRecord> paymentList;
*/
void AccountTransactionRecord::Import(PurchaseRecord &rec, std::function<wxString(wxString)> fnConvertPayBy) {
	Clear();
	double totAmt = 0;
	for (auto const &it : rec.paymentList) {
		std::shared_ptr<AccountJournalRecord> arec(new AccountJournalRecord);
		totAmt += it->GetAmount();
		arec->id = rec.id;
		arec->payAmount = it->GetAmountStr();
		list.push_back(arec);
		refNo = it->reference;
		entityFrom.id = fnConvertPayBy(it->paymentMethod.id); // need to convert to accountingID = cash/cheque;
	}
	totalAmount = wxString::Format("%.4lf", totAmt);
	entityTo.id = rec.supplier.id;
	transactionDate = rec.transactionDate;
	type = "OUT";
}

// not used ; because existing system does not have record payment received.
void AccountTransactionRecord::Import(SalesRecord &/*rec*/, std::function<wxString(wxString)> /*fnConvertPayBy*/) {}


OPSTableRowRecord::OPSTableRowRecord() {
	Init();
	Insert("tableName", &tableName);
	Insert("keyColumnName", &keyColumnName);
	Insert("keyValue", &keyValue);
	Insert("id", &id);
	Insert("parentID", &parentID);
	Insert("grandParentID", &grandParentID);
	Insert("greatGrandParentID", &greatGrandParentID);
	Insert("code", &code);
	Insert("name", &name);
	Insert("filterColumnName", &filterColumnName);
	Insert("filterValue", &filterValue);
	Insert("orderBy", &orderBy);
	Insert("valueFromRegistry", &filterValueViaRegistry);
}
OPSTableRowRecord::OPSTableRowRecord(const OPSTableRowRecord &p): wpObject(p) {
	Init();
	Insert("tableName", &tableName);
	Insert("keyColumnName", &keyColumnName);
	Insert("keyValue", &keyValue);
	Insert("id", &id);
	Insert("parentID", &parentID);
	Insert("grandParentID", &grandParentID);
	Insert("greatGrandParentID", &greatGrandParentID);
	Insert("code", &code);
	Insert("name", &name);
	Insert("filterColumnName", &filterColumnName);
	Insert("filterValue", &filterValue);
	Insert("orderBy", &orderBy);
	Insert("valueFromRegistry", &filterValueViaRegistry);
	operator=(p);
}

OPSTableRowRecord &OPSTableRowRecord::operator=(const OPSTableRowRecord &x) {
	wpObject::operator=(x);
	dependencies = x.dependencies;
	return *this;
}

bool OPSTableRowRecord::LoadXMLNode(const wxXmlNode *tg) {
	if (!wpObject::LoadXMLNode(tg)) return false;
	dependencies.clear();
	for (const wxXmlNode *t = tg->GetChildren(); t; t=t->GetNext()) {
		if (t->GetName().IsSameAs("dependencies")) {
			for (const wxXmlNode *c = t->GetChildren(); c; c=c->GetNext()) {
				std::shared_ptr<TableDependencyRecord> r(new TableDependencyRecord);
				r->LoadXMLNode(c);
				dependencies.push_back(r);
			}
		}
	}
	return true;
}
bool OPSTableRowRecord::GetXMLNode(wxXmlNode *tg) {
	if (!wpObject::GetXMLNode(tg)) return false;
	if (!dependencies.empty()) {
		wxXmlNode *c = new wxXmlNode(tg, nodeType, "dependencies");
		for (auto &&it: dependencies) {
			wxXmlNode *ctg = XMLTag::AddChild(c, it->GetDocumentName());
			it->GetXMLNode(ctg);
		}
	}
	return true;
}

wxJSONValue OPSTableRowRecord::GetJSON() {
	wxJSONValue v;
	return v;
}

bool OPSTableRowRecord::LoadJSON(const wxJSONValue &) {
	return false;
}

StockBalanceSnapShot::StockBalanceSnapShot() {
	Init();
	Insert("stockID", &stockID);
	Insert("qoh", &qoh);
	Insert("averageUnitCost", &averageCost);
	Insert("actualUnitCose", &actualCost);
	Insert("latestUnitCost", &latestCost);
	Insert("qohIn", &qohIn);
	Insert("qohOut", &qohOut);
	Insert("qohAdj", &qohAdj);
}
StockBalanceSnapShot::StockBalanceSnapShot(const StockBalanceSnapShot &dp): wpObject(dp) {
	Init();
	Insert("stockID", &stockID);
	Insert("qoh", &qoh);
	Insert("averageUnitCost", &averageCost);
	Insert("actualUnitCose", &actualCost);
	Insert("latestUnitCost", &latestCost);
	Insert("qohIn", &qohIn);
	Insert("qohOut", &qohOut);
	Insert("qohAdj", &qohAdj);
	operator=(dp);
}

TerminalRecord::TerminalRecord() {
	Init();
	Insert("location", &location);
}

TerminalRecord::TerminalRecord(const TerminalRecord &x): LexiconRecord(x) {
	Init();
	Insert("location", &location);
	operator=(x);
}

ScreenPerspectiveRecord::ScreenPerspectiveRecord() {
	Init();
	Insert("screenName", &screenName);
	Insert("perspective", &perspective);
	Insert("centerPaneName", &centerPaneName);
}

ScreenPerspectiveRecord::ScreenPerspectiveRecord(const ScreenPerspectiveRecord &x) : wpObject(x) {
	Init();
	Insert("screenName", &screenName);
	Insert("perspective", &perspective);
	Insert("centerPaneName", &centerPaneName);
	operator=(x);
}

RequestReport::RequestReport() {
	Init();
	Insert("fromDate", &fromDate);
	Insert("toDate", &toDate);
	Insert("type", &typeOfSales);
}

RequestReport::RequestReport(const RequestReport &x): wpObject(x) {
	Init();
	Insert("fromDate", &fromDate);
	Insert("toDate", &toDate);
	Insert("type", &typeOfSales);
	operator=(x);
}

RequestInfoRecord::RequestInfoRecord() {
	Init();
	Insert("question", &question);
	Insert("answer", &answer);
	Insert("user", &userID);
	Insert("fromDate", &fromDate);
	Insert("toDate", &toDate);
}
RequestInfoRecord::RequestInfoRecord(const RequestInfoRecord &x): wpObject(x) {
	Init();
	Insert("question", &question);
	Insert("answer", &answer);
	Insert("user", &userID);
	Insert("fromDate", &fromDate);
	Insert("toDate", &toDate);
	operator=(x);
}

AttendanceRecord::AttendanceRecord() {
	Init();
	Insert("entity", &entityID);
	Insert("date", &date);
	Insert("ioMode", &ioMode);
	Insert("verifyMode", &verifyMode);
	Insert("workCode", &workCode);
	Insert("remark", &remark);
}

AttendanceRecord::AttendanceRecord(const AttendanceRecord &x): wpObject(x) {
	Init();
	Insert("entity", &entityID);
	Insert("date", &date);
	Insert("ioMode", &ioMode);
	Insert("verifyMode", &verifyMode);
	Insert("workCode", &workCode);
	Insert("remark", &remark);
	operator=(x);
}

wxString LocationRecord::GetTelNo(int idx) const {
	wxString v = GetTelNo();
	wxStringTokenizer tk(v, attributeMark, wxTOKEN_RET_EMPTY);
	for (int i=0; tk.HasMoreTokens(); i++) {
		wxString res = tk.GetNextToken();
		if (i == idx)
			return res;
	}
	return wxEmptyString;
}

EntityRecord::EntityRecord() {
	Init();
	Insert("content", &content);
	Insert("id", &id);
	Insert("docID", &docID);
	Insert("globalID", &globalID);
	Insert("registerdate", &registerDate);
}

EntityRecord::EntityRecord(const EntityRecord& x) : wpObject(x) {
	Init();
	Insert("content", &content);
	Insert("id", &id);
	Insert("docID", &docID);
	Insert("globalID", &globalID);
	Insert("registerdate", &registerDate);
	operator=(x);
}

EntityRecord &EntityRecord::operator=(const EntityRecord &x) {
	wpObject::operator=(x);
	attributes = x.attributes;
	attributeSet = x.attributeSet;
	registerAt = x.registerAt;
	return *this;
}

void EntityRecord::Clear() { wpObject::Clear(); registerAt.Clear();  attributes.clear(); attributeSet.clear(); }
bool EntityRecord::IsEmpty() {
	if (wpObject::IsEmpty()) return registerAt.IsEmpty() && attributes.empty();
	return false;
}

bool EntityRecord::LoadXMLNode(const wxXmlNode *tg) {
	isUpdatedRemote = false;
	if (!wpObject::LoadXMLNode(tg)) return false;
	attributeSet.clear();
	attributes.clear();
	for (const wxXmlNode *t = tg->GetChildren(); t; t=t->GetNext()) {
		if (t->GetName().IsSameAs("registerAt")) {
			registerAt.LoadXMLNode(t);
		}
		else if (t->GetName().IsSameAs("attributes")) {
			for (const wxXmlNode *c = t->GetChildren(); c; c=c->GetNext()) {
				std::shared_ptr <LexiconRecord> r(new LexiconRecord);
				r->LoadXMLNode(c);
				attributeSet.insert(r->id);
				attributes.push_back(r);
			}
		}
	}
	return true;
}

bool EntityRecord::GetXMLNode(wxXmlNode *tg) {
	if (!wpObject::GetXMLNode(tg)) return false;
	if (!registerAt.IsEmpty()) {
		wxXmlNode *c = new wxXmlNode(tg, nodeType, "registerAt");
		registerAt.GetXMLNode(c);
	}
	if (!attributes.empty()) {
		wxXmlNode *c = new wxXmlNode(tg, nodeType, "attributes");
		for (auto &&it: attributes) {
			wxXmlNode *ctg = XMLTag::AddChild(c, it->GetDocumentName());
			it->GetXMLNode(ctg);
		}
	}
	return true;
}

void EntityRecord::Clone(wpObject *p) {
	wpObject::Clone(p);
	EntityRecord *r = dynamic_cast<EntityRecord *>(p);
	if (r) {
		if (!registerAt.IsEmpty()) registerAt.Clone(&r->registerAt);
		if (!attributes.empty()) {
			r->attributes.clear();
			for (auto const &it : attributes) {
				std::shared_ptr <LexiconRecord> v(new LexiconRecord);
				it->Clone(v.get());
				r->attributes.push_back(v);
			}
		}
	}
}

wxString EntityRecord::GetAttributeTypeIDList(const wxString& delimiter) const {
	wxString idList;
	wxString delim = "";
	for (auto const &it : attributes) {
		idList.Append(delim);
		idList.Append(it->id);
		delim = delimiter;
	}
	return idList;
}


CompanyRecord::CompanyRecord() : LocationRecord(), isTaxable(0) {
	Init();
	Insert("discount", &discount);
	Insert("creditTerm", &creditterm);
	Insert("debitTerm", &debitterm);
	Insert("creditLimit", &creditLimit);
	Insert("isTaxable", &isTaxable);
}

CompanyRecord::CompanyRecord(const CompanyRecord &x) : LocationRecord(x), isTaxable(0) {
	Init();
	Insert("discount", &discount);
	Insert("creditTerm", &creditterm);
	Insert("debitTerm", &debitterm);
	Insert("creditLimit", &creditLimit);
	Insert("isTaxable", &isTaxable);
	operator=(x);
}

PersonRecord::PersonRecord() {
	Init();
	Insert("dateOfBirth", &dob);
}

PersonRecord::PersonRecord(const PersonRecord &x): CompanyRecord(x) {
	Init();
	Insert("dateOfBirth", &dob);
	operator=(x);
}

PersonRecord &PersonRecord::operator=(const PersonRecord &x) {
	CompanyRecord::operator=(x);
	bloodType = x.bloodType;
	race = x.race;
	religion = x.religion;
	maritalStatus = x.maritalStatus;
	gender = x.gender;
	job = x.job;
	return *this;
}

bool PersonRecord::LoadXMLNode(const wxXmlNode *tg) {
	if (!CompanyRecord::LoadXMLNode(tg)) return false;
	for (const wxXmlNode *t = tg->GetChildren(); t; t=t->GetNext()) {
		if (t->GetName().IsSameAs("bloodType")) bloodType.LoadXMLNode(t);
		else if (t->GetName().IsSameAs("race")) race.LoadXMLNode(t);
		else if (t->GetName().IsSameAs("religion")) religion.LoadXMLNode(t);
		else if (t->GetName().IsSameAs("maritalStatus")) maritalStatus.LoadXMLNode(t);
		else if (t->GetName().IsSameAs("gender")) gender.LoadXMLNode(t);
		else if (t->GetName().IsSameAs("job")) job.LoadXMLNode(t);
	}
	return true;
}

bool PersonRecord::GetXMLNode(wxXmlNode *tg) {
	if (!CompanyRecord::GetXMLNode(tg)) return false;
	if (!bloodType.IsEmpty())   {wxXmlNode *c = new wxXmlNode(tg, nodeType, "bloodType");  bloodType.GetXMLNode(c);}
	if (!race.IsEmpty())        {wxXmlNode *c = new wxXmlNode(tg, nodeType, "race");       race.GetXMLNode(c);}
	if (!religion.IsEmpty())    {wxXmlNode *c = new wxXmlNode(tg, nodeType, "religion");   religion.GetXMLNode(c);}
	if (!maritalStatus.IsEmpty()) {wxXmlNode *c = new wxXmlNode(tg, nodeType, "maritalStatus"); maritalStatus.GetXMLNode(c);}
	if (!gender.IsEmpty())      {wxXmlNode *c = new wxXmlNode(tg, nodeType, "bender");     gender.GetXMLNode(c); }
	if (!job.IsEmpty())         {wxXmlNode *c = new wxXmlNode(tg, nodeType, "job");        job.GetXMLNode(c); }
	return true;
}

bool PersonRecord::IsEmpty() {
	if (!CompanyRecord::IsEmpty()) return false;
	return true;
}

void PersonRecord::Clone(wpObject *p) {
	CompanyRecord::Clone(p);
	PersonRecord *r = dynamic_cast<PersonRecord *>(p);
	if (r) {
		if (!bloodType.IsEmpty())   {bloodType.Clone(&r->bloodType);}
		if (!race.IsEmpty())        {race.Clone(&r->race);}
		if (!religion.IsEmpty())    {religion.Clone(&r->religion);}
		if (!maritalStatus.IsEmpty()) {maritalStatus.Clone(&r->maritalStatus);}
		if (!gender.IsEmpty())      {gender.Clone(&r->gender); }
		if (!job.IsEmpty())         {job.Clone(&r->job); }
	}
}

void PersonRecord::CopyMyKad(MyKadData &dat) {
	Clear();
	GetName() = dat.name;
	GetIC() = dat.ic;
	wxString addr, delim;
	for (auto const &it: dat.address) {
		addr.Append(delim + it);
		delim = "\n";
	}
	GetAddress() = addr;
	GetRemark() = "NATIONALITY=" + dat.nationality + "\nOLD IC=" + dat.oldic + "\nPOSTCODE=" + dat.postcode + "\nRACE=" + dat.race + "\nRELIGION=" + dat.religion + "\nGENDER=" + dat.sex;
	this->dob = dat.dob;
}

MembershipRecord::MembershipRecord() {
	Init();
	Insert("pointBalance",&pointBalance);
	Insert("enrollNumber",&enrollNumber);
}

MembershipRecord::MembershipRecord(const MembershipRecord &x):PersonRecord(x) {
	Init();
	Insert("pointBalance", &pointBalance);
	Insert("enrollNumber", &enrollNumber);
	operator=(x);
}

MembershipRecord &MembershipRecord::operator=(const MembershipRecord &x) {
	PersonRecord::operator=(x);
	dependencyList = x.dependencyList;
	return *this;
}

bool MembershipRecord::LoadXMLNode(const wxXmlNode *tg) {
	if (!PersonRecord::LoadXMLNode(tg)) return false;
	for (const wxXmlNode *t = tg->GetChildren(); t; t=t->GetNext()) {
		if (t->GetName().IsSameAs("dependencies")) {
			for (const wxXmlNode *ct = t->GetChildren(); ct; ct=ct->GetNext()) {
				dependencyList.push_back(ct->GetAttribute("id"));
			}
		}
	}
	return true;
}

bool MembershipRecord::GetXMLNode(wxXmlNode *tg) {
	if (!PersonRecord::GetXMLNode(tg)) return false;
	if (!dependencyList.empty()) {
		wxXmlNode *c = new wxXmlNode(tg, nodeType, "dependencies");
		for (auto const &it : dependencyList) {
			wxXmlNode *ct = new wxXmlNode(c, nodeType, "Row");
			ct->AddAttribute("id", it);
		}
	}
	return true;
}

void MembershipRecord::Clone(wpObject *p) {
	PersonRecord::Clone(p);
	MembershipRecord *r = dynamic_cast<MembershipRecord *>(p);
	if (r) {
		r->dependencyList.clear();
		for (auto const &it : dependencyList) {
			wxString c = it.Clone();
			r->dependencyList.push_back(c);
		}
	}
}


MembershipPointRecord::MembershipPointRecord() {
	Init();
	Insert("date", &dateOfTransaction);
	Insert("receiptNo",&receiptNo);
	Insert("amount",&amount);
	Insert("points", &points);
	Insert("balance",&balance);
}

MembershipPointRecord::MembershipPointRecord(const MembershipPointRecord &x): wpObject(x) {
	Init();
	Insert("date", &dateOfTransaction);
	Insert("receiptNo", &receiptNo);
	Insert("amount", &amount);
	Insert("points", &points);
	Insert("balance", &balance);
	operator=(x);
}

AccountingRecord::AccountingRecord() {
	Init();
	Insert("date", &dateOfTransaction);
	Insert("refNo",&refNo);
	Insert("debit",&debit);
	Insert("credit", &credit);
	Insert("remark",&remark);
}

AccountingRecord::AccountingRecord(const AccountingRecord &x): wpObject(x) {
	Init();
	Insert("date", &dateOfTransaction);
	Insert("refNo", &refNo);
	Insert("debit", &debit);
	Insert("credit", &credit);
	Insert("remark", &remark);
	operator=(x);
}

PengantinRecord::PengantinRecord() {
	Init();
	Insert("id", &id);
}

PengantinRecord::PengantinRecord(const PengantinRecord &x): wpObject(x) {
	Init();
	Insert("id", &id);
	operator=(x);
}

PengantinRecord &PengantinRecord::operator=(const PengantinRecord &x) {
	wpObject::operator=(x);
	lelaki = x.lelaki;
	perempuan = x.perempuan;
	return *this;
}


bool PengantinRecord::LoadXMLNode(const wxXmlNode *tg) {
	if (!wpObject::LoadXMLNode(tg)) return false;
	for (const wxXmlNode *t = tg->GetChildren(); t; t=t->GetNext()) {
		if (t->GetName().IsSameAs("lelaki")) lelaki.LoadXMLNode(t);
		else if (t->GetName().IsSameAs("perempuan")) perempuan.LoadXMLNode(t);
	}
	return true;
}

bool PengantinRecord::IsEmpty() {
	if (!wpObject::IsEmpty()) return false;
	return lelaki.IsEmpty() && perempuan.IsEmpty();
}

bool PengantinRecord::GetXMLNode(wxXmlNode *tg) {
	if (!wpObject::GetXMLNode(tg)) return false;
	wxXmlNode *c = new wxXmlNode(tg, nodeType, "lelaki"); lelaki.GetXMLNode(c);
	c = new wxXmlNode(tg, nodeType, "perempuan"); perempuan.GetXMLNode(c);
	return true;
}

MakeUpArtistRecord::MakeUpArtistRecord() {
	Init();
	Insert("perSessionRate", &perSessionRate);
	Insert("time", &timeStart);
	Insert("date", &tarikh);
}

MakeUpArtistRecord::MakeUpArtistRecord(const MakeUpArtistRecord &x) :PersonRecord(x) {
	Init();
	Insert("perSessionRate", &perSessionRate);
	Insert("time", &timeStart);
	Insert("date", &tarikh);
	operator=(x);
}

MakeUpArtistRecord &MakeUpArtistRecord::operator=(const MakeUpArtistRecord &x) {
	PersonRecord::operator=(x);
	type=x.type;
	tempat=x.tempat;
	return *this;
}

bool MakeUpArtistRecord::LoadXMLNode(const wxXmlNode *tg) {
	if (!PersonRecord::LoadXMLNode(tg)) return false;
	for (const wxXmlNode *t = tg->GetChildren(); t; t=t->GetNext()) {
		if (t->GetName().IsSameAs("makeUpType")) type.LoadXMLNode(t);
		else if (t->GetName().IsSameAs("location")) tempat.LoadXMLNode(t);
	}
	return true;
}

bool MakeUpArtistRecord::GetXMLNode(wxXmlNode *tg) {
	if (!PersonRecord::GetXMLNode(tg)) return false;
	wxXmlNode *c = new wxXmlNode(tg, nodeType, "makeUpType"); type.GetXMLNode(c);
	c = new wxXmlNode(tg, nodeType, "location"); tempat.GetXMLNode(c);
	return true;
}


bool MakeUpArtistRecord::IsEmpty() {
	if (!PersonRecord::IsEmpty()) return false;
	return type.IsEmpty() && tempat.IsEmpty();
}

wxString MakeUpArtistRecord::ParseAppointmentDateAndTime() {
	if (!tarikh.IsValid()) return wxEmptyString;
	int hr = tarikh.GetHour();
	int min = tarikh.GetMinute();
	return wxString::Format("%02d:%02d", hr, min);
}

wxDateTime &MakeUpArtistRecord::GetAppointmentDateAndTime() {
	if (!tarikh.IsValid()) return tarikh;

	int hr=0, min=0;
	tarikh.ResetTime();
	int *tgt = &hr;
	for (wxString::const_iterator it = timeStart.begin(); it != timeStart.end(); it++) {
		char p = *it;
		if (isdigit(p)) {
			*tgt = *tgt * 10 + (p - '0');
		} else if (p == 'p' || p == 'P') {
			hr = hr+12;
			break;
		} else if (p == 'a' || p == 'A') {
			break;
		} else if (p == ' ') {
			if (tgt == &hr)	tgt = &min;
			else continue;
		} else
			tgt = &min;
	}
	if (hr >= 0 && hr <= 23 && min >= 0 && min <=59) {
		tarikh.SetHour(hr);
		tarikh.SetMinute(min);
	}
	return tarikh;
}


LexiconRecord::LexiconRecord() {
	Init();
	Insert("id", &id);
	Insert("code", &code);
	Insert("name", &name);
	Insert("parentID", &parentID);
	Insert("synonymID", &synonymID);
}

LexiconRecord::LexiconRecord(const LexiconRecord &x) {
	Init();
	Insert("id", &id);
	Insert("code", &code);
	Insert("name", &name);
	Insert("parentID", &parentID);
	Insert("synonymID", &synonymID);
	operator=(x);
}

LexiconRecord::LexiconRecord(const wxString &i, const wxString &c, const wxString &n): id(i), code(c), name(n) {
	Init();
	Insert("id", &id);
	Insert("code", &code);
	Insert("name", &name);
	Insert("parentID", &parentID);
	Insert("synonymID", &synonymID);
}

LexiconRecord::LexiconRecord(long i, const wxString &c, const wxString &n): code(c), name(n) {
	Init();
	Insert("id", &id);
	Insert("code", &code);
	Insert("name", &name);
	Insert("parentID", &parentID);
	Insert("synonymID", &synonymID);
	id = String::IntToString(i);
}

TypeRecord::TypeRecord() {
	Init();
	Insert("limit", &limitValue);
	Insert("default", &defaultValue);
}

TypeRecord::TypeRecord(const TypeRecord &r): LexiconRecord(r) {
	Init();
	Insert("limit", &limitValue);
	Insert("default", &defaultValue);
	operator=(r);
}

TypeRecord::TypeRecord(const wxString &i, const wxString &tID, const wxString &c, const wxString &n):LexiconRecord(i, c, n) {
	Init();
	parentID = tID;
	Insert("limit", &limitValue);
	Insert("default", &defaultValue);
}

TypeRecord::TypeRecord(long i, long tID, const wxString &c, const wxString &n):LexiconRecord(i, c, n) {
	Init();
	Insert("limit", &limitValue);
	Insert("default", &defaultValue);
	parentID = String::IntToString(tID);
}

UserRecord::UserRecord() : registerDate(wxDateTime::Now()) {
	Init();
	Insert("maxDiscountAllowed", &maxDiscountAllowed);
	Insert("password", &encryptedPassword);
	Insert("registerDate", &registerDate);
	Insert("sessionID", &sessionID);
	Insert("menu", &menuXML);
	Insert("app", &applicationName);
	Insert("schema", &schemaName);
	Insert("startupModule", &startUpModule);
}

UserRecord::UserRecord(const UserRecord &x) : PersonRecord(x), registerDate(wxDateTime::Now()) {
	Init();
	Insert("maxDiscountAllowed", &maxDiscountAllowed);
	Insert("password", &encryptedPassword);
	Insert("registerDate", &registerDate);
	Insert("sessionID", &sessionID);
	Insert("menu", &menuXML);
	Insert("app", &applicationName);
	Insert("schema", &schemaName);
	Insert("startupModule", &startUpModule);
	operator=(x);
}

UserRecord &UserRecord::operator=(const UserRecord &x) {
	PersonRecord::operator=(x); // those defined in Insert() don't have to assign here because it's taken care by the root class
	roles = x.roles;
	screens = x.screens;
	rolesSet = x.rolesSet;
	allowedModules = x.allowedModules;
	userRoles = x.userRoles;
	return *this;
}

void UserRecord::Clone(wpObject *p) {
	PersonRecord::Clone(p);
	UserRecord *r = dynamic_cast<UserRecord *>(p);
	if (r) {
		r->roles.clear(); r->screens.clear();
		r->rolesSet.clear(); r->allowedModules.clear();
		for (auto const &it: roles) {
			std::shared_ptr <LexiconRecord> x(new LexiconRecord);
			it->Clone(x.get());
			r->roles.push_back(x);
		}
		for (auto const &it: screens) {
			std::shared_ptr <LexiconRecord> x(new LexiconRecord);
			it->Clone(x.get());
			r->screens.push_back(x);
		}
		for (auto const& it : rolesSet) {
			wxString v = it.Clone();
			r->rolesSet.insert(v);
		}
		for (auto const& it : allowedModules) {
			wxString v = it.Clone();
			r->allowedModules.insert(v);
		}
	}
}

bool UserRecord::LoadXMLNode(const wxXmlNode *tg) {
	if (!PersonRecord::LoadXMLNode(tg)) return false;
	roles.clear();
	rolesSet.clear();
	screens.clear();
	screenSet.clear();
	allowedModules.clear();
	for (const wxXmlNode *t = tg->GetChildren(); t; t=t->GetNext()) {
		if (t->GetName().IsSameAs("roles")) {
			for (const wxXmlNode *e = t->GetChildren(); e; e=e->GetNext()) {
				std::shared_ptr<LexiconRecord> r(new LexiconRecord);
				r->LoadXMLNode(e);
				roles.push_back(r);
				rolesSet.insert(r->code);
			}
		} else if (t->GetName().IsSameAs("screens")) {
			for (const wxXmlNode *e = t->GetChildren(); e; e=e->GetNext()) {
				std::shared_ptr<LexiconRecord> r(new LexiconRecord);
				r->LoadXMLNode(e);
				screens.push_back(r);
				screenSet.insert(r->code);
			}
		} else if (t->GetName().IsSameAs("allowedModules")) {
			for (wxXmlNode *pc = t->GetChildren(); pc; pc = pc->GetNext()) {
				wxString v = pc->GetAttribute("value");
				if (!v.IsEmpty())
					allowedModules.insert(v);
			}
		}
	}
	return true;
}

bool UserRecord::IsRoleEnabled(const wxString &role) const {
	auto const it = rolesSet.find(role);
	return (it != rolesSet.end());
}

bool UserRecord::HasScreen(const wxString &role) const {
	auto const it = screenSet.find(role);
	return (it != screenSet.end());
}

bool UserRecord::IsEmpty() {
	if (!PersonRecord::IsEmpty()) return false;
	for (auto const &it: roles) {
		if (!it->IsEmpty()) return false;
	}
	for (auto const &it: screens) {
		if (!it->IsEmpty()) return false;
	}
	return allowedModules.empty();
}

bool UserRecord::GetXMLNode(wxXmlNode *tg) {
	if (!PersonRecord::GetXMLNode(tg)) return false;
	if (!roles.empty()) {
		wxXmlNode *t = new wxXmlNode(tg, nodeType, "roles");
		for (auto const &it: roles) {
			wxXmlNode *c = new wxXmlNode(t, nodeType, "row");
			it->GetXMLNode(c);
		}
	}
	if (!screens.empty()) {
		wxXmlNode *t = new wxXmlNode(tg, nodeType, "screens");
		for (auto const& it : screens) {
			wxXmlNode *c = new wxXmlNode(t, nodeType, "row");
			it->GetXMLNode(c);
		}
	}
	if (!allowedModules.empty()) {
		wxXmlNode *c = XMLTag::AddChild(tg, "allowedModules");
		for (auto const& it : allowedModules) {
			wxXmlNode *ct = XMLTag::AddChild(c, "row");
			ct->AddAttribute("value", it);
		}
	}
	return true;
}


wxString UserRecord::serverAppUserCode = ":server:";

bool UserRecord::IsAdminUser() const {
	for (auto const &it: roles) {
		if (it->id=="1") return true;
	}
	return false;
}

PriceCategoryRecord::PriceCategoryRecord() {
	Init();
	Insert("price", &price);
	Insert("localPrice", &localSellingPrice);
}

PriceCategoryRecord::PriceCategoryRecord(const PriceCategoryRecord &p): TypeRecord(p) {
	Init();
	Insert("price", &price);
	Insert("localPrice", &localSellingPrice);
	operator=(p);
}

StockTaxAndCharges::StockTaxAndCharges() {
	Init();
	Insert("from", &fromDate);
	Insert("to", &toDate);
	Insert("percent", &percentage);
	Insert("fixed", &fixedValue);
}

StockTaxAndCharges::StockTaxAndCharges(const StockTaxAndCharges &rec): wpObject(rec) {
	Init();
	Insert("from", &fromDate);
	Insert("to", &toDate);
	Insert("percent", &percentage);
	Insert("fixed", &fixedValue);
	operator=(rec);
}

bool StockTaxAndCharges::LoadXMLNode(const wxXmlNode *tg) {
	if (!wpObject::LoadXMLNode(tg)) return false;
	for (const wxXmlNode *t = tg->GetChildren(); t; t=t->GetNext()) {
		if (t->GetName().IsSameAs("type")) {
			type.LoadXMLNode(t);
		}
	}
	return true;
}

bool StockTaxAndCharges::GetXMLNode(wxXmlNode *tg) {
	if (!wpObject::GetXMLNode(tg)) return false;
	wxXmlNode *c = new wxXmlNode(tg, nodeType, "type");
	type.GetXMLNode(c);
	return true;
}

StockTaxAndCharges &StockTaxAndCharges::operator=(const StockTaxAndCharges &x) {
	wpObject::operator=(x);
	type = x.type;
	return *this;
}

void StockTaxAndCharges::Clone(wpObject *p) {
	wpObject::Clone(p);
	StockTaxAndCharges *r = dynamic_cast<StockTaxAndCharges *>(p);
	if (r) {
		if (!type.IsEmpty())   {type.Clone(&r->type);}
	}
}

PackingRecord::PackingRecord() { // pricePurchased(priceSold), quantityPurchased(quantitySold) {
	Init();
	Insert("stockid", &stockID);
	Insert("packSize", &packSize);
	Insert("packDescription", &packDescription);
	Insert("sellingPrice", &sellingPrice);
	Insert("localSellingPrice", &localSellingPrice);
	Insert("maxDiscount", &maxDiscountPercentage);
	Insert("percentAboveCost", &percentAboveCost);
}

PackingRecord::PackingRecord(const PackingRecord &rec) : wpObject(rec) { //pricePurchased(priceSold), quantityPurchased(quantitySold) {
	Init();
	Insert("stockid", &stockID);
	Insert("packSize", &packSize);
	Insert("packDescription", &packDescription);
	Insert("sellingPrice", &sellingPrice);
	Insert("localSellingPrice", &localSellingPrice);
	Insert("maxDiscount", &maxDiscountPercentage);
	Insert("percentAboveCost", &percentAboveCost);
	operator=(rec);
}

PackingRecord &PackingRecord::operator=(const PackingRecord &x) {
	wpObject::operator=(x);
	packType = x.packType;
	priceByCategory = x.priceByCategory;
	return *this;
}

bool PackingRecord::LoadXMLNode(const wxXmlNode *tg) {
	if (!wpObject::LoadXMLNode(tg)) return false;
	for (const wxXmlNode *t = tg->GetChildren(); t; t=t->GetNext()) {
		if (t->GetName().IsSameAs("packType")) {
			packType.LoadXMLNode(t);
		} else if (t->GetName().IsSameAs("priceByCategory")) {
			priceByCategory.clear();
			for (const wxXmlNode *c = t->GetChildren(); c; c=c->GetNext()) {
				std::shared_ptr <PriceCategoryRecord> r(new PriceCategoryRecord);
				r->LoadXMLNode(c);
				priceByCategory.push_back(r);
			}
		}
	}
	return true;
}

bool PackingRecord::GetXMLNode(wxXmlNode *tg) {
	if (!wpObject::GetXMLNode(tg)) return false;
	wxXmlNode *c = new wxXmlNode(tg, nodeType, "packType");
	packType.GetXMLNode(c);
	c = new wxXmlNode(tg, nodeType, "priceByCategory");
	for (auto &&it : priceByCategory) {
		wxXmlNode *ctg = XMLTag::AddChild(c, "row");
		it->GetXMLNode(ctg);
	}
	return true;
}

bool PackingRecord::IsEmpty() {
	if (wpObject::IsEmpty())
		return packType.IsEmpty();
	return false;
}

//wxString PackingRecord::GetAmountString() const {
//    double t = GetAmount();
//    extern double wp_nDec;
//    int i = int(wp_nDec);
//    wxString fmt = wxString::Format("%%.%dlf", i);
//    return wxString::Format(fmt, t);
//}
//
//wxString PackingRecord::GetAmountString() const {
//    double t = GetAmount();
//    extern double wp_nDec;
//    int i = int(wp_nDec);
//    wxString fmt = wxString::Format("%%.%dlf", i);
//    return wxString::Format(fmt, t);
//}
//
//wxString PackingRecord::GetSalesAmountString() const {
//    double t = GetSalesAmount();
//    extern double wp_nDec;
//    int i = int(wp_nDec);
//    wxString fmt = wxString::Format("%%.%dlf", i);
//    return wxString::Format(fmt, t);
//}
//
//wxString PackingRecord::GetPurchaseAmountString() const {
//    double t = GetPurchasedAmount();
//    extern double wp_nDec;
//    int i = int(wp_nDec);
//    wxString fmt = wxString::Format("%%.%dlf", i);
//    return wxString::Format(fmt, t);
//}

bool gStockRecordUseLocalPrice = false;

StockRecord::StockRecord() {
	Init();
	Insert("fastLookupCode", &fastLookupCode);
	Insert("minProfitMargin", &minProfitMargin);
	Insert("synonymTo", &synonymTo);
	Insert("reorderLevel", &reorderLevel);
	Insert("reorderQuantity", &reorderQty);
	Insert("isPoison", &isPoison);
	Insert("isPseudoPoison", &isPseudoPoison);
	Insert("isStockItem", &isStockItem);
	Insert("rewardPointFactor", &rewardPointFactor);
	Insert("qoh", &qoh);
	Insert("qohHold", &qohHold);
	Insert("stockValue", &stockValue);
	Insert("weightedAvgCost", &weightedAvgCost);
	Insert("avgCostPPOS", &averageCostPPOS);
	Insert("latestCost", &latestCost);
	Insert("maxCost", &maxCost);
	Insert("fcc", &fcc);
	Insert("convertPackSize", &convertPacksizeOldData);
	Insert("promotionaldiscount", &promotionalDiscount);
	Insert("dispensingInstruction", &dispensingInstruction);
	Insert("drugWarning", &drugWarning);
}

StockRecord::StockRecord(const StockRecord &x) {
	Init();
	Insert("fastLookupCode", &fastLookupCode);
	Insert("minProfitMargin", &minProfitMargin);
	Insert("synonymTo", &synonymTo);
	Insert("reorderLevel", &reorderLevel);
	Insert("reorderQuantity", &reorderQty);
	Insert("isPoison", &isPoison);
	Insert("isPseudoPoison", &isPseudoPoison);
	Insert("isStockItem", &isStockItem);
	Insert("rewardPointFactor", &rewardPointFactor);
	Insert("qoh", &qoh);
	Insert("qohHold", &qohHold);
	Insert("stockValue", &stockValue);
	Insert("weightedAvgCost", &weightedAvgCost);
	Insert("avgCostPPOS", &averageCostPPOS);
	Insert("latestCost", &latestCost);
	Insert("maxCost", &maxCost);
	Insert("fcc", &fcc);
	Insert("convertPackSize", &convertPacksizeOldData);
	Insert("promotionaldiscount", &promotionalDiscount);
	Insert("dispensingInstruction", &dispensingInstruction);
	Insert("drugWarning", &drugWarning);
	operator=(x);
}

StockRecord &StockRecord::operator=(const StockRecord &x) {
	EntityRecord::operator=(x);
	type = x.type;
	packRecords = x.packRecords;
	taxAndCharges = x.taxAndCharges;
	shelfList = x.shelfList;
	defaultPurchasePack = x.defaultPurchasePack;
	defaultSellingPack = x.defaultSellingPack;
	convertPacksizeOldData = x.convertPacksizeOldData;
	salesReps = x.salesReps;
	return *this;
}

wxString StockRecord::GetDispensingInstruction(const wxString& dosageQty, const wxString& dosageFreq) const {
	wxString res = dispensingInstruction;
	res.Replace("{dosage-qty}", dosageQty);
	res.Replace("{dosage-freq}", dosageFreq);
	return res;
}

double StockRecord::GetTaxPercentage(const wxString &taxTypeID, wxDateTime x) const{
	for (auto const& it : taxAndCharges) {
		if (it->type.id.IsSameAs(taxTypeID)) {
			if (x >= it->fromDate && x <= it->toDate) {
				return wxAtof(it->percentage)/100;
			}
		}
	}
	return 0;
}

bool StockRecord::IsTaxExempted(const wxString &taxTypeID, wxDateTime x) const {
	for (auto const& it : taxAndCharges) {
		if (it->type.id.IsSameAs(taxTypeID)) {
			if (x >= it->fromDate && x <= it->toDate) {
				return false;
			}
		}
	}
	return true;
}

bool StockRecord::LoadXMLNode(const wxXmlNode *tg) {
	if (!EntityRecord::LoadXMLNode(tg)) return false;
	for (const wxXmlNode *t = tg->GetChildren(); t; t=t->GetNext()) {
		if (t->GetName().IsSameAs("type"))
			type.LoadXMLNode(t);
		else if (t->GetName().IsSameAs("defaultPurchasePack")) {
			defaultPurchasePack.LoadXMLNode(t);
		} else if (t->GetName().IsSameAs("defaultSellingPack")) {
			defaultSellingPack.LoadXMLNode(t);
		} else if (t->GetName().IsSameAs("packs")) {
			for (const wxXmlNode *c = t->GetChildren(); c; c=c->GetNext()) {
				std::shared_ptr <PackingRecord> r(new PackingRecord);
				r->LoadXMLNode(c);
				packRecords.push_back(r);
			}
		} else if (t->GetName().IsSameAs("taxAndCharges")) {
			for (const wxXmlNode *c = t->GetChildren(); c; c=c->GetNext()) {
				std::shared_ptr <StockTaxAndCharges> r(new StockTaxAndCharges);
				r->LoadXMLNode(c);
				taxAndCharges.push_back(r);
			}
		} else if (t->GetName().IsSameAs("shelf")) {
			for (const wxXmlNode *c = t->GetChildren(); c; c=c->GetNext()) {
				std::shared_ptr <LexiconRecord> r(new LexiconRecord);
				r->LoadXMLNode(c);
				shelfList.push_back(r);
			}
		}
		else if (t->GetName().IsSameAs("salesrep")) {
			for (const wxXmlNode* c = t->GetChildren(); c; c = c->GetNext()) {
				std::shared_ptr<PersonRecord> r(new PersonRecord);
				r->LoadXMLNode(c);
				salesReps.push_back(r);
			}
		}
	}
	return true;
}

bool StockRecord::GetXMLNode(wxXmlNode *tg) {
	if (!EntityRecord::GetXMLNode(tg)) return false;
	wxXmlNode *ct = new wxXmlNode(tg, nodeType, "type");
	type.GetXMLNode(ct);

	ct = new wxXmlNode(tg, nodeType, "defaultPurchasePack");
	defaultPurchasePack.GetXMLNode(ct);

	ct = new wxXmlNode(tg, nodeType, "defaultSellingPack");
	defaultSellingPack.GetXMLNode(ct);

	if (packRecords.size() > 0) {
		wxXmlNode *childNode = new wxXmlNode(tg, nodeType, "packs");
		for (auto &&it : packRecords) {
			wxXmlNode *pkTag = new wxXmlNode(childNode, nodeType, "row");
			it->GetXMLNode(pkTag);
		}
		childNode = new wxXmlNode(tg, nodeType, "taxAndCharges");
		for (auto&& it : taxAndCharges) {
			wxXmlNode *pkTag = new wxXmlNode(childNode, nodeType, "row");
			it->GetXMLNode(pkTag);
		}
		childNode = new wxXmlNode(tg, nodeType, "shelf");
		for (auto&& it : shelfList) {
			wxXmlNode *pkTag = new wxXmlNode(childNode, nodeType, "row");
			it->GetXMLNode(pkTag);
		}
		childNode = new wxXmlNode(tg, nodeType, "salesrep");
		for (auto&& it : salesReps) {
			wxXmlNode* pkTag = new wxXmlNode(childNode, nodeType, "row");
			it->GetXMLNode(pkTag);
		}
	}
	return true;
}

bool StockRecord::IsEmpty() {
	if (!EntityRecord::IsEmpty()) return false;
	if (!defaultPurchasePack.IsEmpty()) return false;
	if (!defaultSellingPack.IsEmpty()) return false;
	for (auto const& it : packRecords) {
		if (!it->IsEmpty()) return false;
	}
	for (auto const& it : taxAndCharges) {
		if (!it->IsEmpty()) return false;
	}
	for (auto const& it : shelfList) {
		if (!it->IsEmpty()) return false;
	}
	for (auto const& it : salesReps) {
		if (!it->IsEmpty()) return false;
	}
	return type.IsEmpty();
}

bool StockRecord::IsSalesEmpty() {
	if (!EntityRecord::IsEmpty()) return false;
	return true; //GetAmount(packChosen) == 0;
}


void StockRecord::SetPrice(const wxString &p, const wxString &packType) {
	for (auto&& it : packRecords) {
		if (it->packType.code.IsSameAs(packType, true)) {
			it->SetPrice(p);
			return;
		}
	}
	std::shared_ptr<PackingRecord> r(new PackingRecord);
	r->packType.code = packType;
	r->SetPrice(p);
	packRecords.push_back(r);
}

wxString StockRecord::GetPackSize(const wxString &packType) const {
	for (auto const& it : packRecords) {
		if (it->packType.code.IsSameAs(packType, false)) {
			return it->packSize;
		}
	}
	return wxEmptyString;
}

void StockRecord::SetPackSize(const wxString &size, const wxString &packType) {
	for (auto && it: packRecords) {
		if (it->packType.code.IsSameAs(packType, false)) {
			it->packSize = size;
			return;
		}
	}
	std::shared_ptr<PackingRecord> r(new PackingRecord);
	r->packType.code = packType;
	r->packSize = size;
	packRecords.push_back(r);
}

void StockRecord::SetMaxDiscount(const wxString &perc, const wxString &packType) {
	//maxDiscountAllowed = perc;
	for (auto && it : packRecords) {
		if (it->packType.code.IsSameAs(packType, false) || packType.IsEmpty()) {
			it->maxDiscountPercentage = perc;
			return;
		}
	}
}

static wxString emptyString=wxEmptyString;

const wxString StockRecord::GetMaxDiscount(const wxString &packType) const {
	for (auto const & it : packRecords) {
		if (it->packType.code.IsSameAs(packType, true))
			return it->maxDiscountPercentage;
	}
	return emptyString;
}

wxString StockRecord::GetPackList() {
	wxString v, delim;
	for (std::vector<std::shared_ptr<PackingRecord>>::const_reverse_iterator it = packRecords.rbegin();it != packRecords.rend(); it++) { // cannot use reverse range
		if (!(*it)->packSize.IsEmpty()) {
			v.Append(delim);
			v.Append((*it)->packSize);
			delim = ", ";
		}
	}
	return v;
}

double StockRecord::GetAverageCost() const {
	long q = GetQOH();
	if (q == 0) return wxAtof(weightedAvgCost);
	return GetStockValue()/q;
}

double StockRecord::GetCostRef(const wxString& ref) {
	if (ref.IsSameAs("max", false)) {
		return wxAtof(maxCost);
	}
	else if (ref.IsSameAs("average", false)) {
		return wxAtof(weightedAvgCost);
	}
	else if (ref.IsSameAs("latest", false)) {
		return wxAtof(latestCost);
	}
	return 0.0;
}

wxString StockRecord::ShowPackingQuantity(long qLoose) {
	wxString v, delim;
	long rem = qLoose;
// packing records is ordered with biggest at end.
	for (std::vector<std::shared_ptr<PackingRecord>>::const_reverse_iterator it = packRecords.rbegin();it != packRecords.rend(); it++) { // cannot use reverse range
		long packsize = wxAtol((*it)->packSize);
		if (packsize > 0) {
			v.Append(delim);
			v.Append(String::IntToString(rem / packsize));
			rem = rem % packsize;
			delim = "+";
		}
	}
	return v;
}

bool StockRecord::IsNewProduct() const {
	const wxString &t = GetCode();
	if (t.IsEmpty()) return false;
	if (t[0] == '?') return true;
	return false;
}

bool StockRecord::CheckPacking(const wxString &value) {
	if (ftsColumn.empty()) Init();
	for (auto const& it : packRecords) {
		if (it->packType.code.IsSameAs(value)) return true;
	}
	return false;
}

bool StockRecord::CheckBelowCost(const wxString &value, const wxString &packing) const {
	if (wxAtof(value) <= GetStockAverageCost(packing)) return true;
	return false;
}

bool StockRecord::CheckBelowCost(double price, const wxString &packing) const {
	if (price <= GetStockAverageCost(packing)) return true;
	return false;
}

double StockRecord::GetStockAverageCost(const wxString &packing) const {
	return wxAtof(weightedAvgCost)*wxAtof(GetPackSize(packing));
}

wxString StockRecord::GetSellingPrice(const wxString &packType, const std::unordered_set<wxString> *attrib, bool *isGetFromSpecial) const {
	if (isGetFromSpecial) *isGetFromSpecial = false;
	wxString res;
	for (auto const& it : packRecords) {
		if (it->packType.code.IsSameAs(packType, true)) {
			double minPrice=INT_MAX;
			for (auto const &itp : it->priceByCategory) {
				if (attrib && attrib->find(itp->id) != attrib->end()) {
					double p = wxAtof(itp->price);
					if (isGetFromSpecial) *isGetFromSpecial = true;
					if (p < minPrice) {
						minPrice=p;
						res = itp->price;
					}
				}
			}
			if (minPrice >= INT_MAX)
				res = gStockRecordUseLocalPrice ? it->localSellingPrice : it->sellingPrice;
		}
	}
	//res = GetNearestFiveCent(res);
	return res;
}


wxString StockRecord::GetPriceWithGST(const wxString &packType, const wxString &taxGSTtypeID) const {
	auto fnCompute = [this, taxGSTtypeID](const wxString &pr) -> wxString {
		double res = wxAtof(pr)*(1.0 + GetTaxPercentage(taxGSTtypeID));
		return wxString::Format("%.*lf", int(wp_nDec_display), res);
	};
	for (auto const& it : packRecords) {
		if (it->packType.code.IsSameAs(packType, true))
			return fnCompute(gStockRecordUseLocalPrice ? it->localSellingPrice : it->sellingPrice);
	}
	auto const& it = packRecords.begin();
	if (it != packRecords.end()) {
		return fnCompute(gStockRecordUseLocalPrice ? (*it)->localSellingPrice : (*it)->sellingPrice);
	}
	return wxEmptyString;
}

wxString StockRecord::GetPrice(const wxString &packType) const {
	for (auto const& it : packRecords) {
		if (it->packType.code.IsSameAs(packType, true))
			return gStockRecordUseLocalPrice ? it->localSellingPrice : it->sellingPrice;
	}
	auto const &it = packRecords.begin();
	if (it != packRecords.end()) {
		return gStockRecordUseLocalPrice ? (*it)->localSellingPrice : (*it)->sellingPrice;
	}
	return wxEmptyString;
}

const PackingRecord &StockRecord::GetPack(const wxString &packType) const {
	for (auto const& it : packRecords) {
		if (it->packType.code.IsSameAs(packType, true))
			return *it;
	}
	auto const &it = packRecords.begin();
	if (it != packRecords.end()) {
		return *(*it);
	}
	static PackingRecord empty;
	return empty;
}

wxString StockRecord::GetPackID(const wxString &packType) const {
	for (auto const& it : packRecords) {
		if (it->packType.code.IsSameAs(packType, true))
			return it->packType.id;
	}
	auto const &it = packRecords.begin();
	if (it != packRecords.end()) {
		return (*it)->packType.id;
	}
	return wxEmptyString;
}

wxString StockRecord::GetPackDescription(const wxString &packType) const {
	for (auto const& it : packRecords) {
		if (it->packType.code.IsSameAs(packType, true))
			return it->packDescription;
	}
	auto const &it = packRecords.begin();
	if (it != packRecords.end()) {
		return (*it)->packDescription;
	}
	return wxEmptyString;
}

wxLongLong StockRecord::GetPriceInCents(const wxString &packType) const {
	for (auto const& it : packRecords) {
		if (it->packType.code.IsSameAs(packType, true))
			return GetValueInCents(gStockRecordUseLocalPrice ? it->localSellingPrice : it->sellingPrice);
	}
	return wxLongLong(0);
}

wxString StockRecord::GetBiggestPack() const { // get last is the biggest
	int max=0;
	wxString code;
	for (std::vector<std::shared_ptr<PackingRecord>>::const_reverse_iterator it = packRecords.rbegin();it != packRecords.rend(); it++) { // cannot use reverse range
		int p = wxAtol((*it)->packSize);
		if (p > max) {
			max=p;
			code=(*it)->packType.code;
		}
	}
	return code;
}

wxString StockRecord::GetDefaultPack() const { // get default pack '' - if not exist return smallest
	int min = INT_MAX;
	wxString code, smallestcode;
	for (auto const& it : packRecords) {
		int p = wxAtol(it->packSize);
		if (p < min && p > 0) {
			min = p;
			smallestcode = it->packType.code;
		}
		if (it->packType.code.IsEmpty() || it->packType.code.IsSameAs(" "))
			return it->packType.code;
	}
	return smallestcode;
}

wxString StockRecord::GetSmallestPack() const { // get first is the smallest
	int min = INT_MAX;
	wxString code;
	for (auto const& it : packRecords) {
		int p = wxAtol(it->packSize);
		if (p < min && p > 0) {
			min = p;
			code = it->packType.code;
		}
	}
	return code;
}

wxString StockRecord::GetDescription() const {
	wxString v = GetName();
	v.Trim();
	if (!GetWarna().IsEmpty()) {
		v.Append(", ");
		v.Append(GetWarna());
		v.Trim();
	}
	if (!GetPerincian().IsEmpty()) {
		v.Append(", ");
		v.Append(GetPerincian());
		v.Trim();
	}
	return v;
}

HistoricalStockRecord::HistoricalStockRecord() {
	Init();
	Insert("transID", &transactionID);
	Insert("dateOfTrans", &dateOfTransaction);
	Insert("refNo", &refNo);
	Insert("history-qoh", &qoh);
	Insert("location", &location);
	Insert("who", &who);
	Insert("reason", &reason);
	Insert("salesrep", &salesRep);
}

HistoricalStockRecord::HistoricalStockRecord(const HistoricalStockRecord &r) : StockTransactionRecord(r) {
	Init();
	Insert("transID", &transactionID);
	Insert("dateOfTrans", &dateOfTransaction);
	Insert("refNo", &refNo);
	Insert("history-qoh", &qoh);
	Insert("location", &location);
	Insert("who", &who);
	Insert("reason", &reason);
	Insert("salesrep", &salesRep);
	operator=(r);
}

InventoryStatusRecord::InventoryStatusRecord() {
	Init();
	Insert("month", &month);
	Insert("qtyIn", &quantityIn);
	Insert("qtyOut", &quantityOut);
	Insert("qtyAdj", &quantityAdj);
}

InventoryStatusRecord::InventoryStatusRecord(const InventoryStatusRecord &r): StockRecord(r) {
	Init();
	Insert("month", &month);
	Insert("qtyIn", &quantityIn);
	Insert("qtyOut", &quantityOut);
	Insert("qtyAdj", &quantityAdj);
	operator=(r);
}

HistoricalSalesRecord::HistoricalSalesRecord() {
	Init();
	Insert("dateOfTrans", &dateOfTransaction);
	Insert("refNo", &refNo);
	Insert("pack", &pack);
	Insert("quantity", &quantity);
	Insert("price", &price);
	Insert("amount", &amount);
	Insert("bonusPack", &bonusPack);
	Insert("bonusQty", &bonusQty);
	Insert("location", &location);
	Insert("who", &who);
}

HistoricalSalesRecord::HistoricalSalesRecord(const HistoricalSalesRecord &r): MembershipRecord(r) {
	Init();
	Insert("dateOfTrans", &dateOfTransaction);
	Insert("refNo", &refNo);
	Insert("pack", &pack);
	Insert("quantity", &quantity);
	Insert("price", &price);
	Insert("amount", &amount);
	Insert("bonusPack", &bonusPack);
	Insert("bonusQty", &bonusQty);
	Insert("location", &location);
	Insert("who", &who);
	operator=(r);
}

HistoricalPurchaseRecord::HistoricalPurchaseRecord() {
	Init();
	Insert("dateOfTrans", &dateOfTransaction);
	Insert("refNo", &refNo);
	Insert("pack", &pack);
	Insert("quantity", &quantity);
	Insert("bonus", &bonusQty);
	Insert("bonusPack", &bonusPack);
	Insert("price", &price);
	Insert("amount", &amount);
	Insert("phist-discount", &discount);
	Insert("expiry", &expiryDate);
	Insert("batch", &batchNo);
	Insert("location", &location);
	Insert("phist-qoh", &qoh);
	Insert("who", &who);
	Insert("transID", &transactionID);
}

HistoricalPurchaseRecord::HistoricalPurchaseRecord(const HistoricalPurchaseRecord &r): CompanyRecord(r) {
	Init();
	Insert("dateOfTrans", &dateOfTransaction);
	Insert("refNo", &refNo);
	Insert("pack", &pack);
	Insert("quantity", &quantity);
	Insert("bonus", &bonusQty);
	Insert("bonusPack", &bonusPack);
	Insert("price", &price);
	Insert("amount", &amount);
	Insert("phist-discount", &discount);
	Insert("expiry", &expiryDate);
	Insert("batch", &batchNo);
	Insert("location", &location);
	Insert("phist-qoh", &qoh);
	Insert("who", &who);
	Insert("transID", &transactionID);
	operator=(r);
}

double HistoricalPurchaseRecord::GetAverageCost(const StockRecord &srec) const {
	long pksize = wxAtol(srec.GetPackSize(pack));
	long q = wxAtol(quantity);
	long qty = q*pksize + wxAtol(bonusQty)*wxAtol(srec.GetPackSize(bonusPack));
	double value = wxAtof(price) * q;
	if (qty == 0) return 0.0;
	return value / qty;
}

double HistoricalPurchaseRecord::GetNetPrice(const StockRecord &srec) const {
	long pksize = wxAtol(srec.GetPackSize(pack));
	long q = wxAtol(quantity);
	long qty = q*pksize + wxAtol(bonusQty)*wxAtol(srec.GetPackSize(bonusPack));
	double value = wxAtof(price) * q;
	if (qty == 0) return 0.0;
	return value / qty * pksize;
}

PengantinOrderRecord::PengantinOrderRecord() {
	Init();
	Insert("id", &id);
	Insert("tarikhOrder", &tarikhOrder);
	Insert("tarikhInput", &tarikhInput);
}

PengantinOrderRecord::PengantinOrderRecord(const PengantinOrderRecord &x): wpObject(x) {
	Init();
	Insert("id", &id);
	Insert("tarikhOrder", &tarikhOrder);
	Insert("tarikhInput", &tarikhInput);
	operator=(x);
}
PengantinOrderRecord &PengantinOrderRecord::operator=(const PengantinOrderRecord &x) {
	wpObject::operator=(x);
	pengantinRecord = x.pengantinRecord;
	eventList = x.eventList;
	paymentList = x.paymentList;
	return *this;
}

bool PengantinOrderRecord::LoadXMLNode(const wxXmlNode *tg) {
	if (!wpObject::LoadXMLNode(tg)) return false;
	for (const wxXmlNode *t = tg->GetChildren(); t; t=t->GetNext()) {
		if (t->GetName().IsSameAs("pengantin")) {
			pengantinRecord.LoadXMLNode(t);
		} else if (t->GetName().IsSameAs("events")) {
			for (const wxXmlNode *e = t->GetChildren(); e; e=e->GetNext()) {
				std::shared_ptr<EventRecord> rec(new EventRecord);
				rec->LoadXMLNode(e);
				eventList.push_back(rec);
			}
		} else if (t->GetName().IsSameAs("payments")) {
			for (const wxXmlNode *e = t->GetChildren(); e; e=e->GetNext()) {
				std::shared_ptr<PaymentRecord> rec(new PaymentRecord);
				rec->LoadXMLNode(e);
				paymentList.push_back(rec);
			}
		}
	}
	return true;
}

bool PengantinOrderRecord::GetXMLNode(wxXmlNode *tg) {
	if (!wpObject::GetXMLNode(tg)) return false;
	wxXmlNode *t = new wxXmlNode(tg, nodeType, "pengantin");
	pengantinRecord.GetXMLNode(t);
	if (eventList.size() > 0) {
		wxXmlNode *childTag = new wxXmlNode(tg, nodeType, "events");
		for (auto const &it : eventList) {
			wxXmlNode *x = new wxXmlNode(childTag, nodeType, "row");
			it->GetXMLNode(x);
		}
	}
	if (paymentList.size() > 0) {
		wxXmlNode *childTag = new wxXmlNode(tg, nodeType, "payments");
		for (auto const &it : paymentList) {
			wxXmlNode *x = new wxXmlNode(childTag, nodeType, "row");
			it->GetXMLNode(x);
		}
	}
	return true;
}

bool PengantinOrderRecord::IsEmpty() {
	if (!wpObject::IsEmpty()) return false;
	for (auto const& it : eventList)
		if (!it->IsEmpty()) return false;
	for (auto const& it : paymentList)
		if (!it->IsEmpty()) return false;
	return true;
}

PengantinOrderData::PengantinOrderData() {
	Init();
	Insert("group", &group);
	Insert("recordType", &rectype);
}

PengantinOrderData::PengantinOrderData(const PengantinOrderData &x): StockRecord(x) {
	Init();
	Insert("group", &group);
	Insert("recordType", &rectype);
	operator=(x);
}

StockImageRecord::StockImageRecord(){
	Init();
	Insert("stockID", &stockID);
	Insert("width", &width);
	Insert("height", &height);
	Insert("imageNo", &imageNo);
}

StockImageRecord::StockImageRecord(const StockImageRecord &x): wpObject(x) {
	Init();
	Insert("stockID", &stockID);
	Insert("width", &width);
	Insert("height", &height);
	Insert("imageNo", &imageNo);
	operator=(x);
}

PaymentRecord::PaymentRecord() {
	Init();
	Insert("id", &id);
	Insert("salesID", &salesID);
	Insert("amount", &amount);
	Insert("refNo", &reference);
	Insert("paymentDate", &paymentDate);
	Insert("tendered", &tendered);
	Insert("rewardPoint", &rewardPoint);
}

PaymentRecord::PaymentRecord(const PaymentRecord &x): wpObject(x) {
	Init();
	Insert("id", &id);
	Insert("salesID", &salesID);
	Insert("amount", &amount);
	Insert("refNo", &reference);
	Insert("paymentDate", &paymentDate);
	Insert("tendered", &tendered);
	Insert("rewardPoint", &rewardPoint);
	operator=(x);
}

PaymentRecord &PaymentRecord::operator=(const PaymentRecord &x) {
	wpObject::operator=(x);
	paymentMethod = x.paymentMethod;
	payor = x.payor;
	return *this;
}

bool PaymentRecord::LoadXMLNode(const wxXmlNode *tg) {
	if (!wpObject::LoadXMLNode(tg)) return false;
	for (const wxXmlNode *t = tg->GetChildren(); t; t=t->GetNext()) {
		if (t->GetName().IsSameAs("method")) {
			paymentMethod.LoadXMLNode(t);
		} else if (t->GetName().IsSameAs("payor")) {
			payor.LoadXMLNode(t);
		}
	}
	return true;
}

bool PaymentRecord::IsEmpty() {
	if (!wpObject::IsEmpty()) return false;
	return paymentMethod.IsEmpty() && payor.IsEmpty();
}

bool PaymentRecord::GetXMLNode(wxXmlNode *tg) {
	if (!wpObject::GetXMLNode(tg)) return false;
	wxXmlNode *c = new wxXmlNode(tg, nodeType, "method");
	paymentMethod.GetXMLNode(c);

	c = new wxXmlNode(tg, nodeType, "payor");
	payor.GetXMLNode(c);

	return true;
}

EventRecord::EventRecord() {
	Init();
	Insert("id", &id); // have to use small case 'id' because the connection.InsertOrUpdate depends on this field to report new identity
	Insert("name", &name);
	Insert("eventDate", &tarikhEvent);
}

EventRecord::EventRecord(const EventRecord &x): wpObject(x) {
	Init();
	Insert("id", &id); // have to use small case 'id' because the connection.InsertOrUpdate depends on this field to report new identity
	Insert("name", &name);
	Insert("eventDate", &tarikhEvent);
	operator=(x);
}

EventRecord &EventRecord::operator=(const EventRecord &x) {
	wpObject::operator=(x);
	location = x.location;
	eventType = x.eventType;
	artist = x.artist;
	tarikhEvent = x.tarikhEvent;
	itemList = x.itemList;
	return *this;
}

bool EventRecord::LoadXMLNode(const wxXmlNode *tg) {
	if (!wpObject::LoadXMLNode(tg)) return false;
	for (const wxXmlNode *t = tg->GetChildren(); t; t=t->GetNext()) {
		if (t->GetName().IsSameAs("location")) {
			location.LoadXMLNode(t);
		} else if (t->GetName().IsSameAs("eventType")) {
			eventType.LoadXMLNode(t);
		} else if (t->GetName().IsSameAs("makeUpArtist")) {
			artist.LoadXMLNode(t);
		} else if (t->GetName().IsSameAs("stocks")) {
			for (const wxXmlNode *e = t->GetChildren(); e; e=e->GetNext()) {
				std::shared_ptr<StockRecord> rec(new StockRecord);
				rec->LoadXMLNode(e);
				itemList.push_back(rec);
			}
		}
	}
	return true;
}

bool EventRecord::GetXMLNode(wxXmlNode *tg) {
	if (!wpObject::GetXMLNode(tg)) return false;
	wxXmlNode *c = new wxXmlNode(tg, nodeType, "location"); location.GetXMLNode(c);
	c = new wxXmlNode(tg, nodeType, "eventType"); eventType.GetXMLNode(c);
	c = new wxXmlNode(tg, nodeType, "makeUpArtist"); artist.GetXMLNode(c);
	c = new wxXmlNode(tg, nodeType, "stocks");
	for (auto const &it : itemList) {
		wxXmlNode *t = new wxXmlNode(c, nodeType, "row");
		it->GetXMLNode(t);
	}
	return true;
}

bool EventRecord::IsEmpty() {
	if (!wpObject::IsEmpty()) return false;
	for (auto const& it : itemList) {
		if (!it->IsEmpty()) return false;
	}
	return eventType.IsEmpty() && location.IsEmpty() && artist.IsEmpty();
}

wxDateTime PengantinOrderRecord::GetFirstEvent() {
	wxDateTime minDate = wxDateTime::Now();
	minDate.SetYear(9999);
	for (auto const& it : eventList) {
		if (!it->tarikhEvent.IsValid()) continue;
		if (it->tarikhEvent.IsEarlierThan(minDate))
			minDate = it->tarikhEvent;
	}
	return minDate;
}

wxString SearchResultData::GetDescription() const {
	wxString v = "["+code+"] ";
	v.Append(name); v.Trim();
	if (!warna.IsEmpty()) {
		v.Append(", ");
		v.Append(warna);
		v.Trim();
	}
	if (!perincian.IsEmpty()) {
		v.Append(", ");
		v.Append(perincian);
		v.Trim();
	}
	if (!harga.IsEmpty()) {
		v.Append(", RM ");
		v.Append(harga);
		v.Trim();
	}
	return v;
}

SearchParam::SearchParam() {
	Init();
	Insert("search", &search);
	Insert("orderby", &orderBy);
	Insert("consumption", &isConsumption);
	Insert("lowstock", &isLowStock);
	Insert("type", &type);
	Insert("id", &id);
	Insert("method", &method2);
	Insert("option", &option2);
	Insert("key", &key);
	Insert("value", &value);
	Insert("column", &column);
}

SearchParam::SearchParam(const SearchParam &x): wpObject(x) {
	Init();
	Insert("search", &search);
	Insert("orderby", &orderBy);
	Insert("consumption", &isConsumption);
	Insert("lowstock", &isLowStock);
	Insert("type", &type);
	Insert("id", &id);
	Insert("method", &method2);
	Insert("option", &option2);
	Insert("key", &key);
	Insert("value", &value);
	Insert("column", &column);
	operator=(x);
}

StockTransactionRecord::StockTransactionRecord() : StockRecord() {
	Init();
	isSellingPriceManuallyChanged = false;
	Insert("quantity", &quantity);
	Insert("pack", &pack);
	Insert("bonus", &bonusQty);
	Insert("bonusPack", &bonusPack);
	Insert("price", &transactionPrice);
	Insert("cost", &costValue);
	Insert("listPrice", &listPrice);
	Insert("discount-amount", &discountAmount);
	Insert("expiry", &expiryDate);
	Insert("batchNo", &batchNo);
	Insert("receiveID", &receiveDetailID);
	Insert("dateFitting", &tarikhFitting);
	Insert("dateDeliver", &tarikhDeliver);
	Insert("dateReturned", &tarikhReturn);
	Insert("promoterID", &promoterID);
	Insert("suggestedPrice", &srp);
	Insert("amount", &transactionValue);
	Insert("tax", &taxValue);
}

StockTransactionRecord::StockTransactionRecord(const StockTransactionRecord &r) : StockRecord(r) {
	Init();
	isSellingPriceManuallyChanged = false;
	Insert("quantity", &quantity);
	Insert("pack", &pack);
	Insert("bonus", &bonusQty);
	Insert("bonusPack", &bonusPack);
	Insert("price", &transactionPrice);
	Insert("cost", &costValue);
	Insert("listPrice", &listPrice);
	Insert("discount-amount", &discountAmount);
	Insert("expiry", &expiryDate);
	Insert("batchNo", &batchNo);
	Insert("receiveID", &receiveDetailID);
	Insert("dateFitting", &tarikhFitting);
	Insert("dateDeliver", &tarikhDeliver);
	Insert("dateReturned", &tarikhReturn);
	Insert("promoterID", &promoterID);
	Insert("suggestedPrice", &srp);
	Insert("amount", &transactionValue);
	Insert("tax", &taxValue);
	operator=(r);
}

void StockTransactionRecord::CopyTransOnly(const StockTransactionRecord &r) {
	isSellingPriceManuallyChanged = r.isSellingPriceManuallyChanged;
	quantity = r.quantity;
	pack = r.pack;
	bonusQty = r.bonusQty;
	bonusPack = r.bonusPack;
	transactionPrice = r.transactionPrice;
	costValue = r.costValue;
	listPrice = r.listPrice;
	discountAmount = r.discountAmount;
	expiryDate = r.expiryDate;
	batchNo = r.batchNo;
	receiveDetailID = r.receiveDetailID;
	tarikhFitting = r.tarikhFitting;
	tarikhDeliver = r.tarikhDeliver;
	tarikhReturn = r.tarikhReturn;
	promoterID = r.promoterID;
	srp = r.srp;
	transactionValue = r.transactionValue;
	taxValue = r.taxValue;
}

void StockTransactionRecord::CopyStockOnly(const StockRecord &r) {
	StockRecord::operator=(r);
}

double StockTransactionRecord::GetAverageCost() const {
	long qty = wxAtol(quantity)*wxAtol(GetPackSize(pack)) + wxAtol(bonusQty)*wxAtol(GetPackSize(bonusPack));
	double value = wxAtof(transactionValue); // wxAtof(transactionPrice) * wxAtof(quantity);
	if (qty == 0) return 0.0;
	return value/qty;
}

double StockTransactionRecord::GetNetPrice() const {
	long pksize = wxAtol(GetPackSize(pack));
	long q = wxAtol(quantity);
	long qty = q*pksize + wxAtol(bonusQty)*wxAtol(GetPackSize(bonusPack));
	double value = wxAtof(transactionValue); // wxAtof(transactionPrice) * wxAtof(quantity);
	if (qty == 0) return 0.0;
	return value / qty * pksize;
}

double StockTransactionRecord::GetSellingPrice() const {
	long q = wxAtol(quantity);
	double value = wxAtof(transactionValue); // wxAtof(transactionPrice) * wxAtof(quantity);
	if (q == 0) q = 1;
	return value / q;
}

wxString StockTransactionRecord::GetSellingPriceString() const { // with tax
	double t = GetSellingPrice();
	extern double wp_nDec_display;
	int i = int(wp_nDec_display);
	//wxString fmt = wxString::Format("%%.%dlf", i);
	return wxString::Format("%.*lf", i, t);
}

double StockTransactionRecord::GetNetPrice(const StockTransactionRecord &srec) const {
	long pksize = wxAtol(GetPackSize(srec.pack));
	long q = wxAtol(srec.quantity);
	long qty = q*pksize + wxAtol(srec.bonusQty)*wxAtol(GetPackSize(srec.bonusPack));
	double value = wxAtof(srec.transactionValue); // wxAtof(transactionPrice) * wxAtof(quantity);
	if (qty == 0) return 0.0;
	return value / qty * pksize;
}

wxString StockTransactionRecord::GetAmountString() { // with tax
	double t = GetAmount();
	extern double wp_nDec_display;
	int i = int(wp_nDec_display);
	//wxString fmt = wxString::Format("%%.%dlf", i);
	return wxString::Format("%.*lf", i, t);
}

SalesRecord::SalesRecord(): wpObject() {
	Init();
	Insert("id", &id);
	Insert("note", &note);
	Insert("totalAmount", &totalAmount);
	Insert("discountAtTotal", &discountAtTotal);
	Insert("transDate", &transactionDate);
	Insert("term", &term);
	Insert("rounding", &rounding);
	Insert("tax", &taxValue);
	Insert("remark", &remark);
	Insert("sequenceNo", &seqNo);
	Insert("refNo", &refNo);
}

SalesRecord::SalesRecord(const SalesRecord &r) : wpObject(r) {
	Init();
	Insert("id", &id);
	Insert("note", &note);
	Insert("totalAmount", &totalAmount);
	Insert("discountAtTotal", &discountAtTotal);
	Insert("transDate", &transactionDate);
	Insert("term", &term);
	Insert("rounding", &rounding);
	Insert("tax", &taxValue);
	Insert("remark", &remark);
	Insert("sequenceNo", &seqNo);
	Insert("refNo", &refNo);
	operator=(r);
}

SalesRecord &SalesRecord::operator=(const SalesRecord &r) {
	wpObject::operator=(r);
	salesType = r.salesType;
	customer = r.customer;
	promoter = r.promoter;
	terminal = r.terminal;
	user = r.user;
	list = r.list;
	paymentList = r.paymentList;
	return *this;
}

bool SalesRecord::IsEmpty() {
	if (!wpObject::IsEmpty()) return false;
	if (!customer.IsEmpty()) return false;
	if (!promoter.IsEmpty()) return false;
	if (!salesType.IsEmpty()) return false;
	for (auto const& it : list) {
		if (!it->IsEmpty()) return false;
	}
	for (auto const& it : paymentList) {
		if (!it->IsEmpty()) return false;
	}
	return true;
}

wxString SalesRecord::GetAmountString() {
	double amt=0;
	for (auto const& it : list) {
		amt += it->GetSalesAmount();
	}
	return wxString::Format("%.*lf", int(wp_nDec_display), amt);
}

double SalesRecord::GetDiscountAtTotal() {
	double amt = 0;
	for (auto const& it : list) {
		amt += it->GetSalesAmount();
	}
	double paidAmt = 0;
	for (auto const& it : paymentList) {
		paidAmt += it->GetAmount();
	}
	return paidAmt - (amt+wxAtof(rounding));
}

wxString SalesRecord::GetPaidBy() {
	wxString t, sep;
	bool moreThan1Payment = paymentList.size() > 1;
	for (auto const& it : paymentList) {
		if (wxAtoi(it->rewardPoint) < 0) continue;
		if (it->GetTendered() == 0) continue;
		t.Append(sep);
		t.Append(it->paymentMethod.name);
		if (moreThan1Payment) {
			t.Append("(");
			t.Append(TruncateDecimalPoint(it->GetAmountStr()));
			t.Append(")");
		}
		sep = ", ";
	}
	if (t.IsEmpty()) t = "Credit Term";
	return t;
}

bool SalesRecord::LoadXMLNode(const wxXmlNode *tg) {
	if (!wpObject::LoadXMLNode(tg)) return false;
	for (const wxXmlNode *tR = tg->GetChildren(); tR; tR=tR->GetNext()) {
		if (tR->GetName().IsSameAs("stocks")) {
			for (const wxXmlNode *row = tR->GetChildren(); row; row=row->GetNext()) {
				std::shared_ptr<StockTransactionRecord> rec(new StockTransactionRecord);
				rec->LoadXMLNode(row);
				list.push_back(rec);
			}
		} else if (tR->GetName().IsSameAs("payments")) {
			for (const wxXmlNode *row = tR->GetChildren(); row; row=row->GetNext()) {
				std::shared_ptr<PaymentRecord> rec(new PaymentRecord);
				rec->LoadXMLNode(row);
				paymentList.push_back(rec);
			}
		} else if (tR->GetName().IsSameAs("customer")) {
			customer.LoadXMLNode(tR);
		} else if (tR->GetName().IsSameAs("promoter")) {
			promoter.LoadXMLNode(tR);
		} else if (tR->GetName().IsSameAs("user")) {
			user.LoadXMLNode(tR);
		} else if (tR->GetName().IsSameAs("terminal")) {
			terminal.LoadXMLNode(tR);
		} else if (tR->GetName().IsSameAs("salesType")) {
			salesType.LoadXMLNode(tR);
		}
	}
	return true;
}

bool SalesRecord::GetXMLNode(wxXmlNode *tg) {
	if (!wpObject::GetXMLNode(tg)) return false;
	if (!list.empty()) {
		wxXmlNode *tR = XMLTag::AddChild(tg, "stocks");
		for (auto const& it : list) {
			it->GetXMLNode(XMLTag::AddChild(tR, "row"));
		}
	}
	if (!paymentList.empty()) {
		wxXmlNode *tR = XMLTag::AddChild(tg, "payments");
		for (auto const& it : paymentList) {
			it->GetXMLNode(XMLTag::AddChild(tR, "row"));
		}
	}
	if (!customer.IsEmpty()) {
		customer.GetXMLNode(XMLTag::AddChild(tg, "customer"));
	}
	if (!promoter.IsEmpty()) {
		promoter.GetXMLNode(XMLTag::AddChild(tg, "promoter"));
	}
	if (!salesType.IsEmpty()) {
		salesType.GetXMLNode(XMLTag::AddChild(tg, "salesType"));
	}
	if (!terminal.IsEmpty()) {
		terminal.GetXMLNode(XMLTag::AddChild(tg, "terminal"));
	}
	if (!user.IsEmpty()) {
		user.GetXMLNode(XMLTag::AddChild(tg, "user"));
	}
	return true;
}

PurchaseRecord::PurchaseRecord() {
	Init();
	Insert("id", &id);
	Insert("note", &note);
	Insert("term", &term);
	Insert("refNo", &refNo);
	Insert("transDate", &transactionDate);
	Insert("remark", &remark);
}

PurchaseRecord::PurchaseRecord(const PurchaseRecord &r): wpObject(r) {
	Init();
	Insert("id", &id);
	Insert("note", &note);
	Insert("term", &term);
	Insert("refNo", &refNo);
	Insert("transDate", &transactionDate);
	Insert("remark", &remark);
	operator=(r);
}

PurchaseRecord &PurchaseRecord::operator=(const PurchaseRecord &r) {
	wpObject::operator=(r);
	supplier = r.supplier;
	list = r.list;
	purchaseType = r.purchaseType;
	paymentList = r.paymentList;
	return *this;
}

void PurchaseRecord::Clear() { wpObject::Clear(); supplier.Clear(); purchaseType.Clear(); list.clear(); paymentList.clear(); }

bool PurchaseRecord::IsEmpty() {
	if (!wpObject::IsEmpty()) return false;
	if (!supplier.IsEmpty()) return false;
	if (!purchaseType.IsEmpty()) return false;
	for (auto const& it : list) {
		if (!it->IsEmpty()) return false;
	}
	for (auto const& it : paymentList) {
		if (!it->IsEmpty()) return false;
	}
	return true;
}

wxString PurchaseRecord::GetAmountString() const {
	double amt=0;
	for (auto const& it : list) {
		amt += it->GetPurchasedAmount();
	}
	//wxString fmt = wxString::Format("%%.%dlf", int(wp_nDec_display));
	return wxString::Format("%.*lf", int(wp_nDec_display),  amt);
}

bool PurchaseRecord::LoadXMLNode(const wxXmlNode *tg) {
	if (!wpObject::LoadXMLNode(tg)) return false;
	for (const wxXmlNode *tR = tg->GetChildren(); tR; tR=tR->GetNext()) {
		if (tR->GetName().IsSameAs("stocks")) {
			for (const wxXmlNode *row = tR->GetChildren(); row; row=row->GetNext()) {
				std::shared_ptr<StockTransactionRecord> rec(new StockTransactionRecord);
				rec->LoadXMLNode(row);
				list.push_back(rec);
			}
		} else if (tR->GetName().IsSameAs("supplier")) {
			supplier.LoadXMLNode(tR);
		} else if (tR->GetName().IsSameAs("purchaseType")) {
			purchaseType.LoadXMLNode(tR);
		} else if (tR->GetName().IsSameAs("payments")) {
			for (const wxXmlNode *row = tR->GetChildren(); row; row=row->GetNext()) {
				std::shared_ptr<PaymentRecord> rec(new PaymentRecord);
				rec->LoadXMLNode(row);
				paymentList.push_back(rec);
			}
		}
	}
	return true;
}

bool PurchaseRecord::GetXMLNode(wxXmlNode *tg) {
	if (!wpObject::GetXMLNode(tg)) return false;
	if (!list.empty()) {
		wxXmlNode *tR = new wxXmlNode(tg, nodeType, "stocks");
		for (auto const& it : list) {
			wxXmlNode *c = new wxXmlNode(tR, nodeType, "row");
			it->GetXMLNode(c);
		}
	}
	if (!paymentList.empty()) {
		wxXmlNode *tR = new wxXmlNode(tg, nodeType, "payments");
		for (auto const& it : paymentList) {
			wxXmlNode *c = new wxXmlNode(tR, nodeType, "row");
			it->GetXMLNode(c);
		}
	}
	if (!supplier.IsEmpty()) {
		wxXmlNode *t = new wxXmlNode(tg, nodeType, "supplier");
		supplier.GetXMLNode(t);
	}
	if (!purchaseType.IsEmpty()) {
		wxXmlNode *t = new wxXmlNode(tg, nodeType, "purchaseType");
		purchaseType.GetXMLNode(t);
	}
	return true;
}

wxJSONValue PurchaseRecord::GetJSON() {
	wxJSONValue val = wpObject::GetJSON();
	if (!list.empty()) {
		wxJSONValue v;
		for (auto const& it : list) {
			v.Append(it->GetJSON());
		}
		val["stocks"] = v;
	}
	if (!paymentList.empty()) {
		wxJSONValue v;
		for (auto const& it : paymentList) {
			v.Append(it->GetJSON());
		}
		val["payments"] = v;
	}
	if (!supplier.IsEmpty()) val["supplier"] = supplier.GetJSON();
	if (!purchaseType.IsEmpty()) val["purchaseType"] = purchaseType.GetJSON();
	return val;
}

bool PurchaseRecord::LoadJSON(const wxJSONValue &v) {
	bool res = wpObject::LoadJSON(v); // this will call Clear();
	wxJSONValue a = v.ItemAt("stocks");
	if (a.GetType() == wxJSONTYPE_INVALID) return res;
	if (a.GetType() == wxJSONTYPE_ARRAY) {
		for (int i = 0; i<a.Size(); i++) {
			std::shared_ptr<StockTransactionRecord> rec(new StockTransactionRecord);
			rec->LoadJSON(a.ItemAt(i));
			list.push_back(rec);
		}
	}
	a = v.ItemAt("payments");
	if (a.GetType() == wxJSONTYPE_INVALID) return res;
	if (a.GetType() == wxJSONTYPE_ARRAY) {
		for (int i = 0; i<a.Size(); i++) {
			std::shared_ptr<PaymentRecord> rec(new PaymentRecord);
			rec->LoadJSON(a.ItemAt(i));
			paymentList.push_back(rec);
		}
	}
	
	supplier.LoadJSON(v.ItemAt("supplier"));
	purchaseType.LoadJSON(v.ItemAt("purchaseType"));
	return res;
}

SalesRecordList::SalesRecordList() {
	Init();
	Insert("fromDate", &fromDate);
	Insert("toDate", &toDate);
	Insert("limit", &limit);
	Insert("offset", &offset);
	Insert("type", &salesTypeID);
}
SalesRecordList::SalesRecordList(const SalesRecordList &x):wpObject(x) {
	Init();
	Insert("fromDate", &fromDate);
	Insert("toDate", &toDate);
	Insert("limit", &limit);
	Insert("offset", &offset);
	Insert("type", &salesTypeID);
	operator=(x);
}

SalesRecordList &SalesRecordList::operator=(const SalesRecordList &x) {
	wpObject::operator=(x);
	list = x.list;
	return *this;
}

bool SalesRecordList::LoadXMLNode(const wxXmlNode *tg) {
	if (!wpObject::LoadXMLNode(tg)) return false;
	for (const wxXmlNode *tR = tg->GetChildren(); tR; tR=tR->GetNext()) {
		if (tR->GetName().IsSameAs("sales")) {
			for (const wxXmlNode *row = tR->GetChildren(); row; row=row->GetNext()) {
				std::shared_ptr<SalesRecord> rec(new SalesRecord);
				rec->LoadXMLNode(row);
				list.push_back(rec);
			}
		}
	}
	return true;
}

bool SalesRecordList::GetXMLNode(wxXmlNode *node) {
	if (!wpObject::GetXMLNode(node)) return false;
	if (!list.empty()) {
		wxXmlNode *tR = new wxXmlNode(node, nodeType, "sales");
		for (auto const& it : list) {
			wxXmlNode *c = new wxXmlNode(tR, nodeType, "row");
			it->GetXMLNode(c);
		}
	}
	return true;
}

bool SalesRecordList::IsEmpty() {
	if (!wpObject::IsEmpty()) return false;
	for (auto const& it : list) {
		if (!it->IsEmpty()) return false;
	}
	return true;
}

PurchaseRecordList::PurchaseRecordList() {
	Init();
	Insert("fromDate", &fromDate);
	Insert("toDate", &toDate);
	Insert("limit", &limit);
	Insert("offset", &offset);
}

PurchaseRecordList::PurchaseRecordList(const PurchaseRecordList &x): wpObject(x) {
	Init();
	Insert("fromDate", &fromDate);
	Insert("toDate", &toDate);
	Insert("limit", &limit);
	Insert("offset", &offset);
	operator=(x);
}

PurchaseRecordList &PurchaseRecordList::operator=(const PurchaseRecordList &x) {
	wpObject::operator=(x);
	list = x.list;
	return *this;
}

bool PurchaseRecordList::LoadXMLNode(const wxXmlNode *node) {
	if (!wpObject::LoadXMLNode(node)) return false;
	for (const wxXmlNode *p = node->GetChildren(); p; p=p->GetNext()) {
		if (p->GetName().IsSameAs("purchases")) {
			for (const wxXmlNode *t = p->GetChildren(); t; t=t->GetNext()) {
				std::shared_ptr<PurchaseRecord> rec(new PurchaseRecord);
				rec->LoadXMLNode(t);
				list.push_back(rec);
			}
		}
	}
	return true;
}

bool PurchaseRecordList::GetXMLNode(wxXmlNode *node) {
	if (!wpObject::GetXMLNode(node)) return false;
	if (!list.empty()) {
		wxXmlNode *tR = new wxXmlNode(node, nodeType, "purchases");
		for (auto const& it : list) {
			wxXmlNode *c = new wxXmlNode(tR, nodeType, "row");
			it->GetXMLNode(c);
		}
	}
	return true;
}

bool PurchaseRecordList::IsEmpty() {
	if (!wpObject::IsEmpty()) return false;
	for (auto const& it : list) {
		if (!it->IsEmpty()) return false;
	}
	return true;
}

AccountTransactionRecordList::AccountTransactionRecordList() {
	Init();
	Insert("fromDate", &fromDate);
	Insert("toDate", &toDate);
	Insert("limit", &limit);
	Insert("offset", &offset);
}

AccountTransactionRecordList::AccountTransactionRecordList(const AccountTransactionRecordList &x): wpObject(x) {
	Init();
	Insert("fromDate", &fromDate);
	Insert("toDate", &toDate);
	Insert("limit", &limit);
	Insert("offset", &offset);
	operator=(x);
}

AccountTransactionRecordList &AccountTransactionRecordList::operator=(const AccountTransactionRecordList &x) {
	wpObject::operator=(x);
	list = x.list;
	return *this;
}

bool AccountTransactionRecordList::LoadXMLNode(const wxXmlNode *node) {
	if (!wpObject::LoadXMLNode(node)) return false;
	for (const wxXmlNode *p = node->GetChildren(); p; p = p->GetNext()) {
		if (p->GetName().IsSameAs("journals")) {
			for (const wxXmlNode *t = p->GetChildren(); t; t = t->GetNext()) {
				std::shared_ptr<AccountTransactionRecord> rec(new AccountTransactionRecord);
				rec->LoadXMLNode(t);
				list.push_back(rec);
			}
		}
	}
	return true;
}

bool AccountTransactionRecordList::GetXMLNode(wxXmlNode *node) {
	if (!wpObject::GetXMLNode(node)) return false;
	if (!list.empty()) {
		wxXmlNode *tR = new wxXmlNode(node, nodeType, "journals");
		for (auto const& it : list) {
			wxXmlNode *c = new wxXmlNode(tR, nodeType, "row");
			it->GetXMLNode(c);
		}
	}
	return true;
}

bool AccountTransactionRecordList::IsEmpty() {
	if (!wpObject::IsEmpty()) return false;
	for (auto const& it : list) {
		if (!it->IsEmpty()) return false;
	}
	return true;
}

bool FastLookupKeyCodes::LoadXMLNode(const wxXmlNode *node) {
	if (!wpObject::LoadXMLNode(node)) return false;
	for (const wxXmlNode *p = node->GetChildren(); p; p=p->GetNext()) {
		long keyCode=0;
		std::shared_ptr<StockRecord> rec(new StockRecord);
		keyCode = wxAtol(p->GetAttribute("keyCode"));
		for (const wxXmlNode *c = p->GetChildren(); c; c=c->GetNext()) {
			if (c->GetName().IsSameAs("stock")) {//=="Stock") {
				rec->LoadXMLNode(c);
			}
		}
		if (keyCode > 0 && !rec->id.IsEmpty()) {
			list[keyCode] = rec;
		}
	}
	return true;
}

bool FastLookupKeyCodes::GetXMLNode(wxXmlNode *node) {
	if (!wpObject::GetXMLNode(node)) return false;
	for (auto const& it : list) {
		wxXmlNode *c = new wxXmlNode(node, nodeType, "row");
		c->AddAttribute("keyCode", String::IntToString(it.first));
		wxXmlNode *ct = new wxXmlNode(c, nodeType, "stock");
		it.second->GetXMLNode(ct);
	}
	return true;
}

MessageRecord::MessageRecord():wpObject() {
	Init();
	Insert("id", &id);
	Insert("priority", &priority);
	Insert("effectiveDate", &effectiveDate);
	Insert("fromDate", &fromDate);
	Insert("toDate", &toDate);
}

MessageRecord::MessageRecord(const MessageRecord &old):wpObject() {
	Init();
	Insert("id", &id);
	Insert("priority", &priority);
	Insert("effectiveDate", &effectiveDate);
	Insert("fromDate", &fromDate);
	Insert("toDate", &toDate);
	operator=(old);
}

MessageRecord &MessageRecord::operator=(const MessageRecord &x) {
	wpObject::operator=(x);
	messageType = x.messageType;
	messages = x.messages;
	return *this;
}

void MessageRecord::Clear() {
	wpObject::Clear();
	messageType.Clear();
	messages.clear();
}

bool MessageRecord::LoadXMLNode(const wxXmlNode *node) {
	if (!wpObject::LoadXMLNode(node)) return false;
	for (const wxXmlNode *p = node->GetChildren(); p; p=p->GetNext()) {
		if (p->GetName().IsSameAs("messageType")) { //IsSameAs("MessageType") {
			messageType.LoadXMLNode(p);
		} else if (p->GetName().IsSameAs("messages")) { // IsSameAs("Messages") {
			messages.clear();
			for (const wxXmlNode *m = p->GetChildren();m ; m=m->GetNext()) {
				std::shared_ptr<MessageLayer> msgs(new MessageLayer);
				for (const wxXmlNode *cn = m->GetChildren(); cn; cn=cn->GetNext()) {
					const wxXmlAttribute *atr = cn->GetAttributes();
					if (atr)
						msgs->messages.insert(msgs->messages.begin(), atr->GetValue()); // was list::push_front;
				}
				const wxXmlAttribute *atr = m->GetAttributes();
				if (atr)
					msgs->layer = wxAtol(atr->GetValue());
				messages.push_back(msgs);
			}
		}
	}
	return true;
}

wxString MessageRecord::GetStat() {
	int nList = messages.size();
	wxString msg = "no of messages = "+String::IntToString(nList)+"->";
	wxString delim;
	for (auto const& it : messages) {
		msg.Append(delim);
		msg.Append(String::IntToString(it->messages.size()));
		delim=", ";
	}
	return msg;
}


bool MessageRecord::GetXMLNode(wxXmlNode *node) {
	if (!wpObject::GetXMLNode(node)) return false;
	wxXmlNode *c = new wxXmlNode(node, nodeType, "messageType");
	messageType.GetXMLNode(c);

	if (messages.size() > 0) {
		wxXmlNode *ctag = new wxXmlNode(node, nodeType, "messages");
		for (auto const& it : messages) {
			wxXmlNode *rowTag = new wxXmlNode(ctag, nodeType, "layer");
			const MessageLayer &mList = *it;
			rowTag->AddAttribute("Layer", String::IntToString(mList.layer));
			for (auto const& xit : mList.messages) {
				wxXmlNode *cTag = new wxXmlNode(rowTag, nodeType, "row");
				cTag->AddAttribute("row", xit);
			}
		}
	}
	return true;
}

SalesPromotionRecord::SalesPromotionRecord() {
	Init();
	Insert("discount-perc", &discountPercentage);
	Insert("fromDate", &fromDate);
	Insert("toDate", &toDate);
}

SalesPromotionRecord::SalesPromotionRecord(const SalesPromotionRecord &x):wpObject(x) {
	Init();
	Insert("discount-perc", &discountPercentage);
	Insert("fromDate", &fromDate);
	Insert("toDate", &toDate);
	operator=(x);
}

bool SalesPromotionRecord::LoadXMLNode(const wxXmlNode *node) {
	if (!wpObject::LoadXMLNode(node)) return false;
	for (const wxXmlNode *p = node->GetChildren(); p; p=p->GetNext()) {
		if (p->GetName().IsSameAs("type")) //IsSameAs("Type")
			type.LoadXMLNode(p);
	}
	return true;
}

bool SalesPromotionRecord::GetXMLNode(wxXmlNode *node) {
	if (!wpObject::GetXMLNode(node)) return false;
	wxXmlNode *c = new wxXmlNode(node, nodeType, "type");
	type.GetXMLNode(c);
	return true;
}

bool SalesPromotionRecord::IsEmpty() {
	if (!wpObject::IsEmpty()) return false;
	return type.IsEmpty();
}

QuotationRecord::QuotationRecord() {
	Init();
	Insert("Qty", &qty);
	Insert("QtyBonus", &qtyBonus);
	Insert("CostPrice", &costPrice);
	Insert("BonusScheme", &bonusScheme);
	Insert("quot-qoh", &qoh);
}
QuotationRecord::QuotationRecord(const QuotationRecord &r):StockRecord(r) {
	Init();
	Insert("Qty", &qty);
	Insert("QtyBonus", &qtyBonus);
	Insert("CostPrice", &costPrice);
	Insert("BonusScheme", &bonusScheme);
	Insert("quot-qoh", &qoh);
	operator=(r);
}

InventoryRecord::InventoryRecord() {
	Init();
}
InventoryRecord::InventoryRecord(const InventoryRecord &r): wpObject(r) {
	Init();
}


CustHistRecord::CustHistRecord() {
	Init();
	Insert("customerID", &customerID);
	Insert("stockID", &stockID);
	Insert("transDate", &dateOfTransaction);
	Insert("qty", &qty);
	Insert("packing", &packing);
	Insert("price", &price);
	Insert("retailPrice", &retprice);
}

CustHistRecord::CustHistRecord(const CustHistRecord &r): wpObject(r) {
	Init();
	Insert("customerID", &customerID);
	Insert("stockID", &stockID);
	Insert("transDate", &dateOfTransaction);
	Insert("qty", &qty);
	Insert("packing", &packing);
	Insert("price", &price);
	Insert("retailPrice", &retprice);
	operator=(r);
}


ItmHistRecord::ItmHistRecord() {
	Init();
	Insert("supplierID", &supplierID);
	Insert("stockID", &stockID);
	Insert("transDate", &dateOfTransaction);
	Insert("enterDate", &dateEntered);
	Insert("batchNo", &batchNo);
	Insert("expDate", &expiryDate);
	Insert("qty", &qty);
	Insert("packing", &packing);
	Insert("price", &price);
	Insert("bonus", &bonus);
	Insert("bonusPacking", &bonusPacking);
	Insert("refNo", &refNo);
}

ItmHistRecord::ItmHistRecord(const ItmHistRecord &r): wpObject(r) {
	Init();
	Insert("supplierID", &supplierID);
	Insert("stockID", &stockID);
	Insert("transDate", &dateOfTransaction);
	Insert("enterDate", &dateEntered);
	Insert("batchNo", &batchNo);
	Insert("expDate", &expiryDate);
	Insert("qty", &qty);
	Insert("packing", &packing);
	Insert("price", &price);
	Insert("bonus", &bonus);
	Insert("bonusPacking", &bonusPacking);
	Insert("refNo", &refNo);
	operator=(r);
}

ServerKeyRecord::ServerKeyRecord() {
	Init();

	Insert("showMALonInvoice", &showMALonInvoice);
	Insert("pointsToOneRM", &pointsToOneRM);//=200 points to one RM
	Insert("RMtoOnePoint", &RMtoOnePoint);
	Insert("useRMOnlyForPoints", &useRMOnlyForPoints); // no cents
	Insert("roundToNearest5cents", &roundToNearest5cents);
	Insert("roundToNearest5cents_wholesales", &roundToNearest5cents_wholesales);
	Insert("printReceiptByDefault", &printReceiptByDefault);
	Insert("isAutoComputeOtherPackPrices", &isAutoComputeOtherPackPrices);
	Insert("isAutoComputeDefaultPrices", &isAutoComputeDefaultPrices);
	Insert("showCategoryInSalesSummaryPDF", &showCategoryInSalesSummaryPDF);
	Insert("useLocalPrice", &useLocalPrice);
	Insert("showCategoryInSalesSummaryReceiptPrinter", &showCategoryInSalesSummaryReceiptPrinter);
	Insert("referenceCostForSellingPrice", &referenceCostForSellingPrice); // max, latest, average
	Insert("defaultColumnForZeroCostInSales", &defaultColumnForZeroCostInSales); // latestCost, weightedAvgCost, maxCost
	Insert("saveStockPackPrices", &saveStockPackPrices); // to save price by category in StockPackPrices - meaning that this will detach the price for edited item from stockpricecategory factorAboveCost
	Insert("runLocalSearch", &runLocalSearch); // to save price by category in StockPackPrices - meaning that this will detach the price for edited item from stockpricecategory factorAboveCost

	Insert("invoiceFormatPortrait", &invoiceFormatPortrait);

	Insert("gstMarker", &gstMarker); // S
	Insert("gstZeroMarker", &gstZeroMarker); // Z
	Insert("gstRegNo", &gstRegNo);
	Insert("hasGST", &hasGST);
	Insert("whsUseCost", &whsUseCost);// use cost price in wholesales
	Insert("enableCreditLimitCheck", &enableCreditLimitCheck);
	Insert("poisonOrderNote", &poisonOrderNote);
	Insert("quotation_Disclaimer", &quotation_Disclaimer); // print on quotation
	Insert("invoice_Disclaimer", &invoice_Disclaimer); // print on invoice

	Insert("showDiscountOnWholesalesInvoice", &showDiscountOnWholesalesInvoice);
	Insert("ageActiveInMonths", &ageActiveInMonths);
	Insert("idleTimeToStartDemo", &idleTimeToStartDemo);
	Insert("idleTimeToLockScreen", &idleTimeToLockScreen);
	Insert("allowEditInvoiceRecord", &allowEditInvoiceRecord);
	Insert("applicationName", &applicationName);
	Insert("outletRegNo", &outletRegNo);
	Insert("outletCode", &outletCode);
	Insert("outletName", &outletName);
	Insert("outletAddress", &outletAddress);
	Insert("applicationType", &applicationType);
	Insert("adminCanModifyQOH", &adminCanModifyQOH);
	Insert("qohIncludeHoldIn", &qohIncludeHoldIn);
	Insert("qohIncludeHoldOut", &qohIncludeHoldOut);
	Insert("movementIncludeHold", &movementIncludeHold);

	Insert("invoiceShowCustomerAllPages", &invoiceShowCustomerAllPages); // show customer name in all invoice pages;
	Insert("runScriptInDbLocation", &runScriptInDbLocation); // run all script root folder in the same as current DB folder
	Insert("categoryListCrossTab", &categoryListCrossTab); // list of category names separated by ';' for crosstab, those not in this list will be grouped into OTHERS

	Insert("entityServerID", &entityServerID);
	Insert("registrationKey", &registrationKey); // this server ID;
	Insert("serverGroupID", &serverGroupID);

	Insert("doRunHereIfServerNotFound", &doRunHereIfServerNotFound); // run command here if not found
	Insert("serverUUID", &serverUUID);

	Insert("receiptPrinterPointSize", &receiptPrinterPointSize);
	Insert("receiptPrinterSpacing", &receiptPrinterSpacing);
	Insert("showRewardPoints", &showRewardPoints);
	Insert("isTwoLineDescOnReceipt", &isTwoLineDescOnReceipt);
	Insert("showTotalDiscountOnReceipt", &showTotalDiscountOnReceipt);
	Insert("showDiscountPerItemOnReceipt", &showDiscountPerItemOnReceipt);
	Insert("showGSTSummary", &showGSTSummary);
	Insert("noOfLinesEject", &noOfLinesEject);
	Insert("isPriceIncludeTax", &isPriceIncludeTax);
	Insert("sumToNearest", &sumToNearest);
	Insert("truncateSum", &truncateSum);
	Insert("percentMarginAboveCost", &percentMarginAboveCost);

	Insert("autoGenerateMembershipCode", &autoGenerateMembershipCode);
	Insert("autoGenerateSupplierCode", &autoGenerateSupplierCode);
	Insert("autoGenerateItemCode", &autoGenerateItemCode);
	Insert("autoItemCodeLength", &autoItemCodeLength);
	Insert("autoSupplierCodeLength", &autoSupplierCodeLength);
	Insert("autoMembershipCodeLength", &autoMembershipCodeLength);
	Insert("allowDuplicateCode", &allowDuplicateCode);
	Insert("autoCodeCheckPrefix", &autoCodeCheckPrefix);
	Insert("enableCheckPoisonSales", &enableCheckPoisonSales);
	Insert("enableCheckPseudoPoisonSales", &enableCheckPseudoPoisonSales);
	Insert("poisonCategoryMustControl", &poisonCategoryMustControl);

	Insert("retainUserIDonRelogin", &retainUserIDonRelogin); //1 == yes;
	Insert("defaultSalesSmallestPack", &defaultSalesSmallestPack); // 1==yes;
	Insert("qrBeforeGoodBye", &qrBeforeGoodBye);
	Insert("qrString", &qrString);
	Insert("qrImageFactor", &qrImageFactor); // 1=full page, 0.5, half

	Insert("priceCategoryGroupID", &priceCategoryGroupID);
	Insert("entityTypeGroupID", &entityTypeGroupID);
	Insert("poisonCategoryTypeGroupID", &poisonCategoryTypeGroupID);
	Insert("caraBayarGroupID", &caraBayarGroupID);
	Insert("stockTypeGroupID", &stockTypeGroupID);
	Insert("stockTransactionTypeGroupID", &stockTransactionTypeGroupID);
	Insert("payByRewardPoint", &payByRewardPoint);
	Insert("adjustStockTypeID", &adjustStockTypeID);
	Insert("payByCredit", &payByCredit);
	Insert("payByCheque", &payByCheque);
	Insert("payByVoucher", &payByVoucher);
	Insert("payByMasterCard", &payByMasterCard);
	Insert("payByVisa", &payByVisa);
	Insert("payByAmex", &payByAmex);
	Insert("payByCash", &payByCash);
	Insert("creditSalesTypeID", &creditSalesTypeID);
	Insert("cashSalesTypeID", &cashSalesTypeID);
	Insert("IBTTypeID", &IBTTypeID);
	Insert("taxGSTTypeID", &taxGSTTypeID);
	Insert("taxPercentage", &taxPercentage);
	Insert("defaultServiceChargePercentage", &defaultServiceChargePercentage);
	Insert("firstDayOfWeek", &firstDayOfWeek);
	Insert("versionString", &versionString);
	Insert("osVersion", &osVersion);
	Insert("accCashTypeGroupID", &accCashTypeGroupID);
	Insert("accBankTypeGroupID", &accBankTypeGroupID);
	Insert("entityTypeMember", &entityTypeMember);
	Insert("entityTypeSupplier", &entityTypeSupplier);
	Insert("entityTypePromoter", &entityTypePromoter);
	Insert("entityTypeCustomer", &entityTypeCustomer);
	Insert("accJournalTypePurchases", &accJournalTypePurchases);
	Insert("accJournalTypeSales", &accJournalTypeSales);
	Insert("accJournalTypeOthers", &accJournalTypeOthers);
	Insert("accJournalTypeExpenses", &accJournalTypeExpenses);

	Insert("enableAccounting", &enableAccounting);
	Insert("enableHR", &enableHR);
	Insert("enableTicketing", &enableTicketing);
	Insert("enablePrintLabel", &enablePrintLabel);
	Insert("enableMyKad", &enableMyKad);
	Insert("enableBackup", &enableBackup);
	Insert("enableDispensingLabel", &enableDispensingLabel);
	Insert("enablePatientRecord", &enablePatientRecord);
	Insert("enableCatering", &enableCatering);
	Insert("enableOnlineStore", &enableOnlineStore);
	Insert("enableRental", &enableRental);
	Insert("softwareExpiryDate", &softwareExpiryDate);
	Insert("softwareRegisterDate", &softwareRegisterDate);
}

ServerKeyRecord::ServerKeyRecord(const ServerKeyRecord &x): wpObject(x) {
	Init();

	Insert("showMALonInvoice", &showMALonInvoice);
	Insert("pointsToOneRM", &pointsToOneRM);//=200 points to one RM
	Insert("RMtoOnePoint", &RMtoOnePoint);
	Insert("useRMOnlyForPoints", &useRMOnlyForPoints); // no cents
	Insert("roundToNearest5cents", &roundToNearest5cents);
	Insert("roundToNearest5cents_wholesales", &roundToNearest5cents_wholesales);
	Insert("printReceiptByDefault", &printReceiptByDefault);
	Insert("isAutoComputeOtherPackPrices", &isAutoComputeOtherPackPrices);
	Insert("isAutoComputeDefaultPrices", &isAutoComputeDefaultPrices);
	Insert("useLocalPrice", &useLocalPrice);
	Insert("showCategoryInSalesSummaryPDF", &showCategoryInSalesSummaryPDF);
	Insert("showCategoryInSalesSummaryReceiptPrinter", &showCategoryInSalesSummaryReceiptPrinter);
	Insert("referenceCostForSellingPrice", &referenceCostForSellingPrice); // max, latest, average
	Insert("defaultColumnForZeroCostInSales", &defaultColumnForZeroCostInSales); // latestCost, weightedAvgCost, maxCost
	Insert("saveStockPackPrices", &saveStockPackPrices); // to save price by category in StockPackPrices - meaning that this will detach the price for edited item from stockpricecategory factorAboveCost
	Insert("runLocalSearch", &runLocalSearch); // to save price by category in StockPackPrices - meaning that this will detach the price for edited item from stockpricecategory factorAboveCost

	Insert("invoiceFormatPortrait", &invoiceFormatPortrait);

	Insert("gstMarker", &gstMarker); // S
	Insert("gstZeroMarker", &gstZeroMarker); // Z
	Insert("gstRegNo", &gstRegNo);
	Insert("hasGST", &hasGST);
	Insert("whsUseCost", &whsUseCost);// use cost price in wholesales
	Insert("enableCreditLimitCheck", &enableCreditLimitCheck);
	Insert("poisonOrderNote", &poisonOrderNote);
	Insert("quotation_Disclaimer", &quotation_Disclaimer); // print on quotation
	Insert("invoice_Disclaimer", &invoice_Disclaimer); // print on invoice

	Insert("showDiscountOnWholesalesInvoice", &showDiscountOnWholesalesInvoice);
	Insert("ageActiveInMonths", &ageActiveInMonths);
	Insert("idleTimeToStartDemo", &idleTimeToStartDemo);
	Insert("idleTimeToLockScreen", &idleTimeToLockScreen);
	Insert("allowEditInvoiceRecord", &allowEditInvoiceRecord);
	Insert("applicationName", &applicationName);
	Insert("outletRegNo", &outletRegNo);
	Insert("outletCode", &outletCode);
	Insert("outletName", &outletName);
	Insert("outletAddress", &outletAddress);
	Insert("applicationType", &applicationType);
	Insert("adminCanModifyQOH", &adminCanModifyQOH);
	Insert("qohIncludeHoldIn", &qohIncludeHoldIn);
	Insert("qohIncludeHoldOut", &qohIncludeHoldOut);
	Insert("movementIncludeHold", &movementIncludeHold);

	Insert("invoiceShowCustomerAllPages", &invoiceShowCustomerAllPages); // show customer name in all invoice pages;
	Insert("runScriptInDbLocation", &runScriptInDbLocation); // run all script root folder in the same as current DB folder
	Insert("categoryListCrossTab", &categoryListCrossTab); // list of category names separated by ';' for crosstab, those not in this list will be grouped into OTHERS

	Insert("entityServerID", &entityServerID);
	Insert("registrationKey", &registrationKey); // this server ID;
	Insert("serverGroupID", &serverGroupID);

	Insert("doRunHereIfServerNotFound", &doRunHereIfServerNotFound); // run command here if not found
	Insert("serverUUID", &serverUUID);

	Insert("receiptPrinterPointSize", &receiptPrinterPointSize);
	Insert("receiptPrinterSpacing", &receiptPrinterSpacing);
	Insert("showRewardPoints", &showRewardPoints);
	Insert("isTwoLineDescOnReceipt", &isTwoLineDescOnReceipt);
	Insert("showTotalDiscountOnReceipt", &showTotalDiscountOnReceipt);
	Insert("showDiscountPerItemOnReceipt", &showDiscountPerItemOnReceipt);
	Insert("showGSTSummary", &showGSTSummary);
	Insert("noOfLinesEject", &noOfLinesEject);
	Insert("isPriceIncludeTax", &isPriceIncludeTax);
	Insert("sumToNearest", &sumToNearest);
	Insert("truncateSum", &truncateSum);
	Insert("percentMarginAboveCost", &percentMarginAboveCost);

	Insert("autoGenerateMembershipCode", &autoGenerateMembershipCode);
	Insert("autoGenerateSupplierCode", &autoGenerateSupplierCode);
	Insert("autoGenerateItemCode", &autoGenerateItemCode);
	Insert("autoItemCodeLength", &autoItemCodeLength);
	Insert("autoSupplierCodeLength", &autoSupplierCodeLength);
	Insert("autoMembershipCodeLength", &autoMembershipCodeLength);
	Insert("allowDuplicateCode", &allowDuplicateCode);
	Insert("autoCodeCheckPrefix", &autoCodeCheckPrefix);
	Insert("enableCheckPoisonSales", &enableCheckPoisonSales);
	Insert("enableCheckPseudoPoisonSales", &enableCheckPseudoPoisonSales);
	Insert("poisonCategoryMustControl", &poisonCategoryMustControl);

	Insert("retainUserIDonRelogin", &retainUserIDonRelogin); //1 == yes;
	Insert("defaultSalesSmallestPack", &defaultSalesSmallestPack); // 1==yes;
	Insert("qrBeforeGoodBye", &qrBeforeGoodBye);
	Insert("qrString", &qrString);
	Insert("qrImageFactor", &qrImageFactor); // 1=full page, 0.5, half

	Insert("priceCategoryGroupID", &priceCategoryGroupID);
	Insert("entityTypeGroupID", &entityTypeGroupID);
	Insert("poisonCategoryTypeGroupID", &poisonCategoryTypeGroupID);
	Insert("caraBayarGroupID", &caraBayarGroupID);
	Insert("stockTypeGroupID", &stockTypeGroupID);
	Insert("stockTransactionTypeGroupID", &stockTransactionTypeGroupID);
	Insert("payByRewardPoint", &payByRewardPoint);
	Insert("adjustStockTypeID", &adjustStockTypeID);
	Insert("payByCredit", &payByCredit);
	Insert("payByCheque", &payByCheque);
	Insert("payByVoucher", &payByVoucher);
	Insert("payByMasterCard", &payByMasterCard);
	Insert("payByVisa", &payByVisa);
	Insert("payByAmex", &payByAmex);
	Insert("payByCash", &payByCash);
	Insert("creditSalesTypeID", &creditSalesTypeID);
	Insert("cashSalesTypeID", &cashSalesTypeID);
	Insert("IBTTypeID", &IBTTypeID);
	Insert("taxGSTTypeID", &taxGSTTypeID);
	Insert("taxPercentage", &taxPercentage);
	Insert("defaultServiceChargePercentage", &defaultServiceChargePercentage);
	Insert("firstDayOfWeek", &firstDayOfWeek);
	Insert("versionString", &versionString);
	Insert("osVersion", &osVersion);
	Insert("accCashTypeGroupID", &accCashTypeGroupID);
	Insert("accBankTypeGroupID", &accBankTypeGroupID);
	Insert("entityTypeMember", &entityTypeMember);
	Insert("entityTypeSupplier", &entityTypeSupplier);
	Insert("entityTypePromoter", &entityTypePromoter);
	Insert("entityTypeCustomer", &entityTypeCustomer);
	Insert("accJournalTypePurchases", &accJournalTypePurchases);
	Insert("accJournalTypeSales", &accJournalTypeSales);
	Insert("accJournalTypeOthers", &accJournalTypeOthers);
	Insert("accJournalTypeExpenses", &accJournalTypeExpenses);

	Insert("enableAccounting", &enableAccounting);
	Insert("enableHR", &enableHR);
	Insert("enableTicketing", &enableTicketing);
	Insert("enablePrintLabel", &enablePrintLabel);
	Insert("enableMyKad", &enableMyKad);
	Insert("enableBackup", &enableBackup);
	Insert("enableDispensingLabel", &enableDispensingLabel);
	Insert("enablePatientRecord", &enablePatientRecord);
	Insert("enableCatering", &enableCatering);
	Insert("enableOnlineStore", &enableOnlineStore);
	Insert("enableRental", &enableRental);
	Insert("softwareExpiryDate", &softwareExpiryDate);
	Insert("softwareRegisterDate", &softwareRegisterDate);
	operator=(x);
}

ServerKeyRecord &ServerKeyRecord::operator=(const ServerKeyRecord &x) {
	wpObject::operator=(x);
	pointValueWeightList = x.pointValueWeightList;
	customerDisplay = x.customerDisplay;
	printer = x.printer;
	return *this;
}

int ServerKeyRecord::GetAutoCodeLength(const wxString &entityType) {
	if (map.empty()) {
		map["entityTypeStock"] = &autoItemCodeLength;
		map["entityTypeSupplier"] = map["entityTypeCompany"] = map["entityTypeCustomer"] = &autoSupplierCodeLength;
		map["entityTypeMember"] = &autoMembershipCodeLength;
	}
	int *p = map[entityType];
	if (p) return *p;
	return 0;
}

bool ServerKeyRecord::LoadXMLNode(const wxXmlNode *node) {
	if (!wpObject::LoadXMLNode(node)) return false;
	pointValueWeightList.clear();
	for (const wxXmlNode *p = node->GetChildren(); p; p=p->GetNext()) {
		if (p->GetName().IsSameAs("pointValueWeight")) {
			for (const wxXmlNode *c = p->GetChildren(); c; c=c->GetNext() ) {
				const wxXmlAttribute *atr = c->GetAttributes();
				if (atr) {
					pointValueWeightList.push_back(StringPair(atr->GetName(), atr->GetValue()));
				}
			}
		} else if (p->GetName().IsSameAs("customerDisplay")) {
			customerDisplay.LoadXMLNode(p);
		} else if (p->GetName().IsSameAs("receiptPrinter")) {
			printer.LoadXMLNode(p);
		}
	}
	//roundTo5cents = roundToNearest5cents.IsEmpty() || roundToNearest5cents.IsSameAs("TRUE", false) || roundToNearest5cents.IsSameAs("YES", false) || roundToNearest5cents.IsSameAs("1", false);
	//roundTo5centsWholesale = roundToNearest5cents_wholesales.IsEmpty() || roundToNearest5cents_wholesales.IsSameAs("TRUE", false) || roundToNearest5cents_wholesales.IsSameAs("YES", false) || roundToNearest5cents_wholesales.IsSameAs("1", false);

	return true;
}

bool ServerKeyRecord::GetXMLNode(wxXmlNode *node) {
	if (!wpObject::GetXMLNode(node)) return false;
	wxXmlNode *c = XMLTag::AddChild(node, "pointValueWeight");
	for (auto const& it : pointValueWeightList) {
		wxXmlNode *cn = XMLTag::AddChild(c, "Row");
		cn->AddAttribute(it.first, it.second);
	}
	if (!printer.IsEmpty()) {
		wxXmlNode *childTag = XMLTag::AddChild(node, "receiptPrinter");
		printer.GetXMLNode(childTag);
	}
	if (!customerDisplay.IsEmpty()) {
		wxXmlNode *childTag = XMLTag::AddChild(node, "customerDisplay");
		customerDisplay.GetXMLNode(childTag);
	}
	return true;
}


void ServerKeyRecord::Clear() {
	wpObject::Clear();
	pointValueWeightList.clear();
	printer.Clear();
	customerDisplay.Clear();
}

LabelInputParam::LabelInputParam(): useTimeAsItIs(false) {
	Init();
	Insert("label", &label);
	Insert("type", &type);
	Insert("param", &param);
	Insert("value", &value);
	Insert("startflag", &startFlag);
	Insert("lookup", &lookupOps);
	Insert("list", &list);
	Insert("default", &defaultValue);
	Insert("useTimeAsItIs", &useTimeAsItIs);
}
LabelInputParam::LabelInputParam(const LabelInputParam &p) : wpObject(p), useTimeAsItIs(false) {
	Init();
	useTimeAsItIs = p.useTimeAsItIs;
	Insert("label", &label);
	Insert("type", &type);
	Insert("param", &param);
	Insert("value", &value);
	Insert("startflag", &startFlag);
	Insert("lookup", &lookupOps);
	Insert("list", &list);
	Insert("default", &defaultValue);
	Insert("useTimeAsItIs", &useTimeAsItIs);
	operator=(p);
}

MenuDialogParam::MenuDialogParam() {
	Init();
	Insert("title", &title);
}

MenuDialogParam::MenuDialogParam(const MenuDialogParam &p): wpObject(p) {
	Init();
	Insert("title", &title);
	operator=(p);
}

MenuDialogParam &MenuDialogParam::operator=(const MenuDialogParam &p) {
	wpObject::operator=(p);
	paramList = p.paramList;
	return *this;
}

wxJSONValue MenuDialogParam::GetJSON() {
	wxJSONValue v = wpObject::GetJSON();
	wxJSONValue pList;
	for (auto const& it : paramList) {
		pList.Append(it->GetJSON());
	}
	v["param-list"] = pList;
	return v;
}

bool MenuDialogParam::LoadJSON(const wxJSONValue& v) {
	if (!wpObject::LoadJSON(v)) return false;

	wxJSONValue a = v.ItemAt("param-list");
	if (a.GetType() == wxJSONTYPE_INVALID) return false;
	if (a.GetType() == wxJSONTYPE_ARRAY) {
		for (int i = 0; i < a.Size(); i++) {
			std::shared_ptr<LabelInputParam> rec(new LabelInputParam);
			if (!rec->LoadJSON(a.ItemAt(i))) return false;
			paramList.push_back(rec);
		}
	}
	return true;
}

bool MenuDialogParam::LoadXMLNode(const wxXmlNode *x) {
	if (!wpObject::LoadXMLNode(x)) return false;
	for (wxXmlNode *p = x->GetChildren(); p; p = p->GetNext()) {
		if (p->GetName().IsSameAs("field")) {
			std::shared_ptr<LabelInputParam> inp(new LabelInputParam);
			inp->LoadXMLNode(p);
			paramList.push_back(inp);
		}
	}
	return true;
}

bool MenuDialogParam::GetXMLNode(wxXmlNode *x) {
	if (!wpObject::GetXMLNode(x)) return false;
	for (auto const& it : paramList) {
		wxXmlNode *c = XMLTag::AddChild(x, "field");
		it->GetXMLNode(c);
	}
	return true;
}

void MenuDialogParam::Clear() {
	wpObject::Clear();
	paramList.clear();
}

MenuActionParam::MenuActionParam() {
	Init();
	Insert("name", &name);
	Insert("script", &scriptName);
	Insert("type", &outputType);
	Insert("orientation", &orientation);
	Insert("sheet-type", &sheetType);
	Insert("command-before", &cmdBefore);
	Insert("command-after", &cmdAfter);
	Insert("user-param", &userParam);
	Insert("sql-link", &sqlLink);
}

MenuActionParam::MenuActionParam(const MenuActionParam &p): wpObject(p) {
	Init();
	Insert("name", &name);
	Insert("script", &scriptName);
	Insert("type", &outputType);
	Insert("orientation", &orientation);
	Insert("sheet-type", &sheetType);
	Insert("command-before", &cmdBefore);
	Insert("command-after", &cmdAfter);
	Insert("user-param", &userParam);
	Insert("sql-link", &sqlLink);
	operator=(p);
}

MenuActionParam &MenuActionParam::operator=(const MenuActionParam &p) {
	wpObject::operator=(p);
	dialog = p.dialog;
	return *this;
}
wxJSONValue MenuActionParam::GetJSON() {
	wxJSONValue v = wpObject::GetJSON();
	v["dialog"] = dialog.GetJSON();
	return v;
}
bool MenuActionParam::LoadJSON(const wxJSONValue& v) {
	if (wpObject::LoadJSON(v)) {
		return dialog.LoadJSON(v.ItemAt("dialog"));
	}
	return false;
}

bool MenuActionParam::LoadXMLNode(const wxXmlNode *x) {
	if (!wpObject::LoadXMLNode(x)) return false;
	for (wxXmlNode *p = x->GetChildren(); p; p = p->GetNext()) {
		if (p->GetName().IsSameAs("dialog"))
			dialog.LoadXMLNode(p);
	}
	return true;
}

bool MenuActionParam::GetXMLNode(wxXmlNode *x) {
	if (!wpObject::GetXMLNode(x)) return false;
	if (!dialog.IsEmpty()) {
		wxXmlNode *c = XMLTag::AddChild(x, "dialog");
		dialog.GetXMLNode(c);
	}
	return true;
}

void MenuActionParam::Clear() {
	wpObject::Clear();
	dialog.Clear();
}

void MenuActionParam::AddInputDateParameters(const wxDateTime &fromDate, const wxDateTime &toDate, int useTimeAsItIs) {
	bool existingFrom = false, existingTo = false;
	for (auto const& it : dialog.paramList) {
		wxString paramName = it->param;
		if (it->type.IsSameAs("date", false)) {
			if (it->startFlag.IsSameAs("begin", false)) {
				existingFrom = true;
				it->value = fromDate.GetValue().ToString();
				it->useTimeAsItIs = useTimeAsItIs;
			} else if (it->startFlag.IsSameAs("end", false)) {
				existingTo = true;
				it->value = toDate.GetValue().ToString();
				it->useTimeAsItIs = useTimeAsItIs;
			}
		}
	}
	if (!existingFrom) {
		std::shared_ptr<LabelInputParam> p(new LabelInputParam);
		p->type = "date"; p->startFlag = "begin"; p->value = fromDate.GetValue().ToString();
		p->useTimeAsItIs = useTimeAsItIs;
		dialog.paramList.push_back(p);
	}
	if (!existingTo) {
		std::shared_ptr<LabelInputParam> p(new LabelInputParam);
		p->type = "date"; p->startFlag = "end"; p->value = toDate.GetValue().ToString();
		p->useTimeAsItIs = useTimeAsItIs;
		dialog.paramList.push_back(p);
	}
}

bool MenuActionParam::GetInputDateParameters(wxDateTime &fromDate, wxDateTime &toDate) {
	bool foundFrom = false, foundTo = false;
	for (auto const& it : dialog.paramList) {
		//wxString paramName = it->param;
		if (it->type.IsSameAs("date", false)) {
			long long l;
			it->value.ToLongLong(&l);
			wxLongLong ll(l);
			wxDateTime dt(ll);
			if (it->startFlag.IsSameAs("begin", false)) {
				if (!it->useTimeAsItIs)
					dt.ResetTime();
				fromDate = dt;
				foundFrom = true;
			}
			else if (it->startFlag.IsSameAs("end", false)) {
				if (!it->useTimeAsItIs) {
					dt.SetHour(23);
					dt.SetMinute(59);
					dt.SetSecond(59);
					dt.SetMillisecond(999);
				}
				toDate = dt;
				foundTo = true;
			}
		}
	}
	return foundFrom && foundTo;
}

void MenuActionParam::SetParameter(const wxString &pName, const wxString &val) {
	for (auto const& it : dialog.paramList) {
		if (it->param.IsSameAs(pName)) {
			it->value = val;
			return;
		}
	}
	std::shared_ptr<LabelInputParam> p(new LabelInputParam);
	p->param = pName;
	p->value = val;
	dialog.paramList.push_back(p);
}

wxString MenuActionParam::GetParameter(const wxString &pName) {
	for (auto const& it : dialog.paramList) {
		if (it->param.IsSameAs(pName))
			return it->value;
	}
	return "";
}

void MenuActionParam::ReplaceStatement(wxString &sqlCmd) {
	for (auto const& it : dialog.paramList) {
		wxString r = "=" + it->param;
		sqlCmd.Replace(r, it->value);
	}
}

void MenuActionParam::SetDefault() {
	for (auto const& it : dialog.paramList)
		if (it->value.IsEmpty())
			it->value = it->defaultValue;
}

void MenuActionParam::SetParameters(wpSQLStatement *stt) {
	if (stt->GetParamCount() <= 0) return;
	for (auto const& it : dialog.paramList) {
		int idx = stt->GetParamIndex(it->param);
		if (idx > 0) {
			ShowLog(it->param + " = " + it->value);
			if (it->type.IsSameAs("date", false)) {
				long long l;
				it->value.ToLongLong(&l);
				wxLongLong ll(l);
				wxDateTime dt(ll);
				if (it->startFlag.IsSameAs("begin", false))
					dt.ResetTime();
				else if (it->startFlag.IsSameAs("end", false)) {
					dt.SetHour(23);
					dt.SetMinute(59);
					dt.SetSecond(59);
					dt.SetMillisecond(999);
				}
				stt->BindNumericDateTime(idx, dt);
			}
			else {
				stt->Bind(idx, it->value);
			}
		}
	}
	int idx;
	if ((idx = stt->GetParamIndex("@userparam")) > 0) {
		ShowLog("@userparam = " + userParam);
		stt->Bind(idx, userParam);
	}
}


ServerEventMessage::ServerEventMessage():isBroadcast(false), isMobileNotification(false) {
	Init();
	Insert("code", &code);
	Insert("path", &path);
	Insert("isbc", &isBroadcast);
	Insert("ismobilenofitication", &isMobileNotification);
}

ServerEventMessage::ServerEventMessage(const ServerEventMessage &c): wpObject(c), isBroadcast(c.isBroadcast), isMobileNotification(c.isMobileNotification) {
	Init();
	Insert("code", &code);
	Insert("path", &path);
	Insert("isbc", &isBroadcast);
	Insert("ismobilenofitication", &isMobileNotification);
	operator=(c);
}

ServerEventMessage &ServerEventMessage::operator=(const ServerEventMessage &x) {
	wpObject::operator=(x);
	messages = x.messages;
	tokenList = x.tokenList;
	return *this;
}

void ServerEventMessage::AddMessage(const wxString &key, const wxString &value) {
	std::shared_ptr<MessageRecord> r(new MessageRecord);
		r->key = key; r->value = value;
	messages.push_back(r);
}

bool ServerEventMessage::LoadXMLNode(const wxXmlNode *x) {
	if (!wpObject::LoadXMLNode(x)) return false;
	messages.clear();
	for (wxXmlNode *p= x->GetChildren(); p; p = p->GetNext()) {
		if (p->GetName().IsSameAs("messages")) {
			for (wxXmlNode *pc = p->GetChildren(); pc; pc = pc->GetNext()) {
				std::shared_ptr<MessageRecord> r(new MessageRecord);
				for (wxXmlAttribute *atr = pc->GetAttributes(); atr; atr=atr->GetNext()) {
					if (atr->GetName().IsSameAs("key")) r->key = atr->GetValue();
					else if (atr->GetName().IsSameAs("value")) r->value = atr->GetValue();
				}
				messages.push_back(r);
			}
		} else if (p->GetName().IsSameAs("tokens")) {
			tokenList.clear();
			for (wxXmlNode* pc = p->GetChildren(); pc; pc = pc->GetNext()) {
				wxXmlAttribute* atr = pc->GetAttributes();
				if (atr) {
					tokenList.push_back(atr->GetValue());
				}
			}
		}
	}
	return true;
}

bool ServerEventMessage::GetXMLNode(wxXmlNode *x) {
	if (!wpObject::GetXMLNode(x)) return false;
	if (!messages.empty()) {
		wxXmlNode *c = XMLTag::AddChild(x, "messages");
		for (auto const& it : messages) {
			wxXmlNode *ct = XMLTag::AddChild(c, "row");
			ct->AddAttribute("key", it->key);
			ct->AddAttribute("value", it->value);
		}
	}
	if (!tokenList.empty()) {
		wxXmlNode* c = XMLTag::AddChild(x, "tokens");
		for (auto const& it : tokenList) {
			wxXmlNode* ct = XMLTag::AddChild(c, "row");
			ct->AddAttribute("value", it);
		}
	}
	return true;
}

ServerInfo::ServerInfo(): wpObject() {
	Init();
	Insert("outletName",&outletName);
	Insert("outletCode",&outletCode);
	Insert("registrationKey",&registrationKey);
	Insert("groupID",&groupID);
}
ServerInfo::ServerInfo(const ServerInfo& rec): wpObject(rec) {
	Init();
	Insert("outletName", &outletName);
	Insert("outletCode", &outletCode);
	Insert("registrationKey", &registrationKey);
	Insert("groupID", &groupID);
	operator=(rec);
}

LoginSessionRecord::LoginSessionRecord(uintptr_t ctx, const wxString &sKey): portNo(0), isCallBack(0), listeningPortNo(0), isHTTP(false), status(ConnectionFlag::Disconnected), connectionCtx(ctx), sessionKey(sKey), serviceNo(0), needUpdate(0) {
	Init();
	Insert("id", &id);
	Insert("timeIn", &timeIn);
	Insert("timeOut", &timeOut);
	Insert("expiryDate", &expiryDate);
	Insert("ipAddress", &ipAddress);
	Insert("listeningPortNo", &listeningPortNo);
	Insert("remark", &remark);
	Insert("appName", &applicationName);
	Insert("sessionKey", &sessionKey);
	Insert("serverregkey", &serverRegistrationKey);
	Insert("serverUUID", &serverUUID);
	Insert("certificate", &certificate);
	Insert("systemUser", &systemUser);
	Insert("serverName", &serverName);
	Insert("clientOsVersion", &clientOsVersion);
	Insert("serverOsVersion", &serverOsVersion);
	Insert("serverAppVersion", &serverAppVersion);
	Insert("clientAppVersion", &clientAppVersion);
	Insert("connectFromIP", &connectFromAddress);
	Insert("serviceNo", &serviceNo);
	Insert("portNo", &portNo);
	Insert("callback", &isCallBack);
	Insert("needUpdate", &needUpdate);
	Insert("outletName", &outletName);
	Insert("outletCode", &outletCode);
	Insert("groupid", &groupID);
	Insert("peerID", &peerID);
	status = LoginSessionRecord::Disconnected;
	isHTTP = false;
	timeIn = wxDateTime::UNow();
	expiryDate = timeIn.Add(wxDateSpan(0, 0, 0, 2));  // expires in 2 days;
}

LoginSessionRecord::LoginSessionRecord(const LoginSessionRecord &rec):wpObject(rec), portNo(rec.portNo), isCallBack(rec.isCallBack),
listeningPortNo(rec.listeningPortNo), isHTTP(rec.isHTTP), status(rec.status), connectionCtx(rec.connectionCtx), sessionKey(rec.sessionKey), serviceNo(rec.serviceNo), needUpdate(rec.needUpdate) {
	Init();
	Insert("id", &id);
	Insert("timeIn", &timeIn);
	Insert("timeOut", &timeOut);
	Insert("expiryDate", &expiryDate);
	Insert("ipAddress", &ipAddress);
	Insert("listeningPortNo", &listeningPortNo);
	Insert("remark", &remark);
	Insert("appName", &applicationName);
	Insert("sessionKey", &sessionKey);
	Insert("serverregkey", &serverRegistrationKey);
	Insert("serverUUID", &serverUUID);
	Insert("certificate", &certificate);
	Insert("systemUser", &systemUser);
	Insert("serverName", &serverName);
	Insert("clientOsVersion", &clientOsVersion);
	Insert("serverOsVersion", &serverOsVersion);
	Insert("serverAppVersion", &serverAppVersion);
	Insert("clientAppVersion", &clientAppVersion);
	Insert("connectFromIP", &connectFromAddress);
	Insert("serviceNo", &serviceNo);
	Insert("portNo", &portNo);
	Insert("callback", &isCallBack);
	Insert("needUpdate", &needUpdate);
	Insert("outletName", &outletName);
	Insert("outletCode", &outletCode);
	Insert("groupid", &groupID);
	Insert("peerID", &peerID);
	operator=(rec);
}

LoginSessionRecord::~LoginSessionRecord() {}

// purposely not copy databaseSelected;
LoginSessionRecord &LoginSessionRecord::operator=(const LoginSessionRecord &rec) {
	wpObject::operator=(rec);
	status = rec.status;
	isHTTP = rec.isHTTP;
	connectionCtx = rec.connectionCtx;
	user = rec.user;
	terminal = rec.terminal;
	databaseList = rec.databaseList;
	portNo = rec.portNo;
	isCallBack = rec.isCallBack;
	listeningPortNo = rec.listeningPortNo;
	serviceNo = rec.serviceNo;
	needUpdate = rec.needUpdate;
	otherServers = rec.otherServers;
	return *this;
}

//"PharmaPOS Client\nVer 1.52 Build 85 [1088b41132]\nCompiled on 2015-07-26 18:08:18 GMT+8"
int LoginSessionRecord::GetBuildNo(const wxString &s) {
	wxStringTokenizer tok(s);
	while (tok.HasMoreTokens()) {
		wxString t = tok.GetNextToken();
		if (t.IsSameAs("Build", false)) {
			if (tok.HasMoreTokens()) {
				t = tok.GetNextToken();
				return wxAtol(t);
			}
		}
	}
	return 0;
}

wxString LoginSessionRecord::GetVersion(const wxString &s) {
	wxStringTokenizer tok(s);
	wxString v, sep;
	int n=5;
	while (tok.HasMoreTokens()) {
		wxString t = tok.GetNextToken();
		if (t.IsSameAs("Ver", false) || n < 5) {
			v.Append(sep+t);
			sep=" ";
			if (--n <= 1) break;
		}
	}
	return v;
}

double LoginSessionRecord::GetVersionNo(const wxString &s) {
	wxString v = GetVersion(s); // return Ver 1.61 Build 99
	wxStringTokenizer tok(v, " ");
	if (tok.HasMoreTokens()) {
		tok.GetNextToken(); // Ver
		if (tok.HasMoreTokens()) {
			wxString ver = tok.GetNextToken();
			return wxAtof(ver);
		}
	}
	return 0.0;
}

bool LoginSessionRecord::LoadJSON(const wxJSONValue& v) {
	bool res = wpObject::LoadJSON(v);
	if (!res) return false;
	if (!user.LoadJSON(v.ItemAt("user"))) return false;
	if (!terminal.LoadJSON(v.ItemAt("terminal"))) return false;
	if (!databaseSelected.LoadJSON(v.ItemAt("database_selected"))) return false;

	wxJSONValue a = v.ItemAt("other_servers");
	if (a.GetType() == wxJSONTYPE_INVALID) return res;
	otherServers.clear();
	for (int i = 0; i < a.Size(); i++) {
		std::shared_ptr<ServerInfo> sess(new ServerInfo());
		if (!sess->LoadJSON(a[i])) return false;
		otherServers.push_back(sess);
	}
	return true;
}

wxJSONValue LoginSessionRecord::GetJSON() {
	wxJSONValue val = wpObject::GetJSON();
	val["user"] = user.GetJSON();
	val["terminal"] = terminal.GetJSON();
	val["database_selected"] = databaseSelected.GetJSON();
	if (!otherServers.empty()) {
		wxJSONValue v;
		for (auto const& it : otherServers) { // causing recursion...
			v.Append(it->GetJSON());
		}
		val["other_servers"] = v;
	}
	return val;
}

bool LoginSessionRecord::LoadXMLNode(const wxXmlNode *x) {
	if (!wpObject::LoadXMLNode(x)) return false;
	for (wxXmlNode *p = x->GetChildren(); p; p = p->GetNext()) {
		if (p->GetName().IsSameAs("user"))
			user.LoadXMLNode(p);
		if (p->GetName().IsSameAs("terminal"))
			terminal.LoadXMLNode(p);
		if (p->GetName().IsSameAs("selectedDB"))
			databaseSelected.LoadXMLNode(p);
		if (p->GetName().IsSameAs("databases")) {
			databaseList.clear();
			for (wxXmlNode *c = p->GetChildren(); c; c=c->GetNext()) {
				std::shared_ptr<DatabaseRecord> r(new DatabaseRecord);
				r->LoadXMLNode(c);
				databaseList.push_back(r);
			}
		}
		if (p->GetName().IsSameAs("other_servers")) {
			otherServers.clear();
			for (wxXmlNode* c = p->GetChildren(); c; c = c->GetNext()) {
				std::shared_ptr<ServerInfo> r(new ServerInfo());
				r->LoadXMLNode(c);
				otherServers.push_back(r);
			}
		}

	}
	return true;
}

void LoginSessionRecord::CreateCertificate() {
	certificate = FNVHash64(GetComputerIdentity());
	certificate.Append(":");
	certificate.Append(wxDateTime::Now().GetValue().ToString());
	certificate = GetEncryptedString(certificate);
}

bool LoginSessionRecord::GetXMLNode(wxXmlNode *x) {
	if (!wpObject::GetXMLNode(x)) return false;
	if (!user.IsEmpty()) {
		wxXmlNode *c = XMLTag::AddChild(x, "user");
		user.GetXMLNode(c);
	}
	if (!terminal.IsEmpty()) {
		wxXmlNode *c = XMLTag::AddChild(x, "terminal");
		terminal.GetXMLNode(c);
	}
	if (!databaseSelected.IsEmpty()) {
		wxXmlNode *c = XMLTag::AddChild(x, "selectedDB");
		databaseSelected.GetXMLNode(c);
	}
	if (!databaseList.empty()) {
		wxXmlNode *c = XMLTag::AddChild(x, "databases");
		for (auto const& it : databaseList) {
			it->GetXMLNode(new wxXmlNode(c, nodeType, "row"));
		}
	}
	if (!otherServers.empty()) {
		wxXmlNode* c = XMLTag::AddChild(x, "other_servers");
		for (auto const& it : otherServers) {
			it->GetXMLNode(new wxXmlNode(c, nodeType, "row"));
		}
	}
	return true;
}

// purposely not clearing databaseSelected;
void LoginSessionRecord::Clear() {
	wpObject::Clear(); user.Clear(); terminal.Clear(); databaseList.clear(); otherServers.clear();
	status = Handshaken; // default to WebSocket ready.
	isHTTP = false;
}
bool LoginSessionRecord::IsEmpty() {
	if (!wpObject::IsEmpty()) return false;
	return user.IsEmpty() && terminal.IsEmpty() && databaseList.empty() && otherServers.empty();
}


MyKadData::MyKadData() {
	Init();
	Insert("name", &name);
	Insert("ic", &ic);
	Insert("oldic", &oldic);
	Insert("sex", &sex);
	Insert("state", &stateOfBirth);
	Insert("nationality", &nationality);
	Insert("race", &race);
	Insert("religion", &religion);
	Insert("postcode", &postcode);
	Insert("dob", &dob);
	Insert("validityDate", &validityDate);
}

MyKadData::MyKadData(const MyKadData &rec): wpObject(rec) {
	Init();
	Insert("name", &name);
	Insert("ic", &ic);
	Insert("oldic", &oldic);
	Insert("sex", &sex);
	Insert("state", &stateOfBirth);
	Insert("nationality", &nationality);
	Insert("race", &race);
	Insert("religion", &religion);
	Insert("postcode", &postcode);
	Insert("dob", &dob);
	Insert("validityDate", &validityDate);
	operator=(rec);
}

MyKadData &MyKadData::operator=(const MyKadData &rec) {
	wpObject::operator=(rec);
	address = rec.address;
	return *this;
}


bool MyKadData::LoadXMLNode(const wxXmlNode *x) {
	if (!wpObject::LoadXMLNode(x)) return false;
	for (wxXmlNode *p = x->GetChildren(); p; p = p->GetNext()) {
		if (p->GetName().IsSameAs("address")) {
			address.clear();
			for (wxXmlNode *c = p->GetChildren(); c; c = c->GetNext()) {
				LexiconRecord r;
				r.LoadXMLNode(c);
				address.push_back(r.name);
			}
		}
	}
	return true;
}

bool MyKadData::GetXMLNode(wxXmlNode *x) {
	if (!wpObject::GetXMLNode(x)) return false;
	if (!address.empty()) {
		wxXmlNode *c = XMLTag::AddChild(x, "address");
		for (auto const& it : address) {
			LexiconRecord r; r.name = it;
			r.GetXMLNode(new wxXmlNode(c, nodeType, "row"));
		}
	}
	return true;
}

void MyKadData::Clear() { wpObject::Clear(); address.clear(); }
bool MyKadData::IsEmpty() {
	if (!wpObject::IsEmpty()) return false;
	return address.empty();
}


ActivateSoftwareRecord::ActivateSoftwareRecord() {
	Init();
	Insert("accounting", &accounting);
	Insert("ticketing", &ticketing);
	Insert("catering", &catering);
	Insert("onlineStore", &onlineStore);
	Insert("rental", &rental);
	Insert("stockTracking", &stockTracking);
	Insert("mykad", &mykad);
	Insert("backup", &backup);
	Insert("expiry", &softwareExpiry);
	Insert("dispensing", &dispensing);
}
ActivateSoftwareRecord::ActivateSoftwareRecord(const ActivateSoftwareRecord &rec): wpObject(rec) {
	Init();
	Insert("accounting", &accounting);
	Insert("ticketing", &ticketing);
	Insert("catering", &catering);
	Insert("onlineStore", &onlineStore);
	Insert("rental", &rental);
	Insert("stockTracking", &stockTracking);
	Insert("mykad", &mykad);
	Insert("backup", &backup);
	Insert("expiry", &softwareExpiry);
	Insert("dispensing", &dispensing);
	operator=(rec);
}

AuthenticationRecord::AuthenticationRecord() {
    Init();
    Insert("id", &id);
    Insert("userid", &userid);
    Insert("password", &password);
    Insert("email", &email);
    Insert("telephone", &telephone);
    Insert("fbId", &fbId);
    Insert("googleId", &googleId);
    Insert("token", &token);
    Insert("message", &message);
}

AuthenticationRecord::AuthenticationRecord(const AuthenticationRecord &rec): wpObject(rec) {
    Init();
    Insert("id", &id);
    Insert("userid", &userid);
    Insert("password", &password);
    Insert("email", &email);
    Insert("telephone", &telephone);
    Insert("fbId", &fbId);
    Insert("googleId", &googleId);
    Insert("token", &token);
    Insert("message", &message);
    operator=(rec);
}

RegisterSoftwareRecord::RegisterSoftwareRecord() {
	Init();
	Insert("id", &id);
	Insert("uuid", &uuid);
	Insert("userName", &userName);
	Insert("password", &password);
	Insert("outlet", &outletName);
	Insert("outletcode", &outletCode);
	Insert("groupid", &groupID);
	Insert("address", &address);
	Insert("telephone", &telephone);
	Insert("email", &email);
	Insert("CID", &cidHash);
	Insert("server-id", &registrationKey);
	Insert("expdate", &expiryDate);
	Insert("dbFolder", &dbFolder);
	Insert("dbTransFolder", &dbTransFolder);
	Insert("version", &version);
	Insert("regdate", &registrationDate);
	Insert("lastlogin", &lastLoginDate);
	Insert("ul-keys", &ulKeys);
	Insert("ul-localKeys", &ulLocalKeys);
	Insert("messages", &messages);
	Insert("messageDetail", &messageDetail);
	Insert("key-accounting", &activationKey.accounting);
	Insert("key-ticketing", &activationKey.ticketing);
	Insert("key-catering", &activationKey.catering);
	Insert("key-onlineStore", &activationKey.onlineStore);
	Insert("key-rental", &activationKey.rental);
	Insert("key-stockTracking", &activationKey.stockTracking);
	Insert("key-mykad", &activationKey.mykad);
	Insert("key-backup", &activationKey.backup);
	Insert("key-expiry", &activationKey.softwareExpiry);
	Insert("key-dispensing", &activationKey.dispensing);
	Insert("key-patientRecord", &activationKey.patientRecord);
	Insert("key-hr", &activationKey.hr);
}
RegisterSoftwareRecord::RegisterSoftwareRecord(const RegisterSoftwareRecord &rec): wpObject(rec) {
	Init();
	Insert("id", &id);
	Insert("uuid", &uuid);
	Insert("userName", &userName);
	Insert("password", &password);
	Insert("outlet", &outletName);
	Insert("outletcode", &outletCode);
	Insert("groupid", &groupID);
	Insert("address", &address);
	Insert("telephone", &telephone);
	Insert("email", &email);
	Insert("CID", &cidHash);
	Insert("server-id", &registrationKey);
	Insert("regdate", &registrationDate);
	Insert("dbFolder", &dbFolder);
	Insert("dbTransFolder", &dbTransFolder);
	Insert("version", &version);
	Insert("lastlogin", &lastLoginDate);
	Insert("ul-keys", &ulKeys);
	Insert("ul-localKeys", &ulLocalKeys);
	Insert("messages", &messages);
	Insert("messageDetail", &messageDetail);
	Insert("key-accounting", &activationKey.accounting);
	Insert("key-ticketing", &activationKey.ticketing);
	Insert("key-catering", &activationKey.catering);
	Insert("key-onlineStore", &activationKey.onlineStore);
	Insert("key-rental", &activationKey.rental);
	Insert("key-stockTracking", &activationKey.stockTracking);
	Insert("key-mykad", &activationKey.mykad);
	Insert("key-backup", &activationKey.backup);
	Insert("key-expiry", &activationKey.softwareExpiry);
	Insert("key-dispensing", &activationKey.dispensing);
	Insert("key-patientRecord", &activationKey.patientRecord);
	Insert("key-hr", &activationKey.hr);
	operator=(rec);
}

WorkShiftRecord::WorkShiftRecord(): wpObject(), createdBy(NULL), closeBy(NULL) {
	Init();
	Insert("id", &id);
	Insert("timeStart", &timeStart);
	Insert("timeEnd", &timeEnd);
	Insert("openingBalance", &openBalance);
	Insert("closingBalance", &closingBalance);
}

WorkShiftRecord::WorkShiftRecord(const WorkShiftRecord &rec): wpObject(rec), createdBy(rec.createdBy), closeBy(rec.closeBy) {
	Init();
	Insert("id", &id);
	Insert("timeStart", &timeStart);
	Insert("timeEnd", &timeEnd);
	Insert("openingBalance", &openBalance);
	Insert("closingBalance", &closingBalance);
	operator=(rec);
}

WorkShiftRecord &WorkShiftRecord::operator=(const WorkShiftRecord &rec) {
	wpObject::operator=(rec);
	createdBy = rec.createdBy;
	closeBy = rec.closeBy;
	return *this;
}

bool WorkShiftRecord::LoadXMLNode(const wxXmlNode *x) {
	if (!wpObject::LoadXMLNode(x)) return false;
	for (wxXmlNode *p = x->GetChildren(); p; p = p->GetNext()) {
		if (p->GetName().IsSameAs("createdBy"))
			createdBy.LoadXMLNode(p);
		if (p->GetName().IsSameAs("closeBy"))
			closeBy.LoadXMLNode(p);
	}
	return true;
}

bool WorkShiftRecord::GetXMLNode(wxXmlNode *x) {
	if (!wpObject::GetXMLNode(x)) return false;
	if (!createdBy.IsEmpty()) {
		wxXmlNode *c = XMLTag::AddChild(x, "createdBy");
		createdBy.GetXMLNode(c);
	}
	if (!closeBy.IsEmpty()) {
		wxXmlNode *c = XMLTag::AddChild(x, "closeBy");
		closeBy.GetXMLNode(c);
	}
	return true;
}

void WorkShiftRecord::Clear() {wpObject::Clear(); createdBy.Clear(); closeBy.Clear();}
bool WorkShiftRecord::IsEmpty() {
	if (!wpObject::IsEmpty()) return false;
	return createdBy.IsEmpty() && closeBy.IsEmpty();
}


/*
<?xml version="1.0" encoding="utf_8"?>
<GSTAuditFile>
	<Companies>
		<Company>
			<BusinessName>ABC SDN BHD</BusinessName>
			<BusinessRN>654321_V</BusinessRN>
			<GSTNumber>IDGST:10001/2015</GSTNumber>
			<PeriodStart>2015_12_01</PeriodStart>
			<PeriodEnd>2015_12_31</PeriodEnd>
			<GAFCreationDate>2015_12_31</GAFCreationDate>
			<ProductVersion>2015_12_31</ProductVersion>
			<GAFVersion>2015_12_31</GAFVersion>
		</Company>
	</Companies>
	<Purchases>
		<Purchase>
			<SupplierName>MEI MEI SDN BHD</SupplierName>
			<SupplierBRN>123456_G</SupplierBRN>
			<InvoiceDate>2015_12_19</InvoiceDate>
			<InvoiceNumber>STV/012324/8</InvoiceNumber>
			<ImportDeclarationNo />
			<LineNumber>1</LineNumber>
			<ProductDescription>Purchase of shark fins</ProductDescription>
			<PurchaseValueMYR>300</PurchaseValueMYR>
			<GSTValueMYR>18</GSTValueMYR>
			<TaxCode>TX</TaxCode>
			<FCYCode>XXX</FCYCode>
			<PurchaseFCY>0</PurchaseFCY>
			<GSTFCY>0</GSTFCY>
		</Purchase>
	</Purchases>
	<Supplies>
		<Supply>
			<CustomerName>PQR SDN BHD</CustomerName>
			<CustomerBRN>867890_B</CustomerBRN>
			<InvoiceDate>2015_12_21</InvoiceDate>
			<InvoiceNumber>2353</InvoiceNumber>
			<LineNumber>1</LineNumber>
			<ProductDescription>Rental of residential House</ProductDescription>
			<SupplyValueMYR>1000</SupplyValueMYR>
			<GSTValueMYR>0</GSTValueMYR>
			<TaxCode>ES</TaxCode>
			<Country />
			<FCYCode>XXX</FCYCode>
			<SupplyFCY>0</SupplyFCY>
			<GSTFCY>0</GSTFCY>
		</Supply>
	</Supplies>
	<Ledger>
		<LedgerEntry>
			<TransactionDate>2015_12_18</TransactionDate>
			<AccountID>10000</AccountID>
			<AccountName>BANK</AccountName>
			<TransactionDescription>Payment for fish crackers</TransactionDescription>
			<Name>THAI FISH CRACKERS</Name>
			<TransactionID />
			<SourceDocumentID>TTref784316</SourceDocumentID>
			<SourceType>AP</SourceType>
			<Debit>0</Debit>
			<Credit>1802</Credit>
			<Balance>8198</Balance>
		</LedgerEntry>
	</Ledger>
	<Footer>
		<TotalPurchaseCount>4</TotalPurchaseCount>
		<TotalPurchaseAmount>3960</TotalPurchaseAmount>
		<TotalPurchaseAmountGST>123.6</TotalPurchaseAmountGST>
		<TotalSupplyCount>5</TotalSupplyCount>
		<TotalSupplyAmount>8000</TotalSupplyAmount>
		<TotalSupplyAmountGST>120</TotalSupplyAmountGST>
		<TotalLedgerCount>39</TotalLedgerCount>
		<TotalLedgerDebit>24343.60</TotalLedgerDebit>
		<TotalLedgerCredit>24343.60</TotalLedgerCredit>
		<TotalLedgerBalance>0.00</TotalLedgerBalance>
	</Footer>
</GSTAuditFile>
*/
