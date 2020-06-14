#pragma once
#include "WORDS.H"
#include "xmlParser.h"
#include "wx/xml/xml.h"
#include "wx/sstream.h"
#include "jsonval.h"
#include "wx/msgqueue.h"
#include "wx/buffer.h"
#include "wpSQLDatabase.h"

#include <list>
#include <vector>
#include <memory>
#include <unordered_set>
#include <unordered_map>

struct RunningData {
	long portNo;
	bool isRunningAsService;
	wxString serviceName, wpCustomizedSoftwareName;
	wxString serverName, userID, password;
	bool useSSL, toInitCustomerDisplay;
	bool useSSLforRegistration;
	bool useSSLforLiveConnection;
	wxString workingFolder;
	bool isDistributedDB;
	wxString loadingParam;
	wxString databaseName, transName;
	wxString localMasterName, localTransName;
	wxString exeDirectory;
	wxString executeJob;
	wxString importJob;
	long numberOfWorkerThread, nWebSocketServer;
	wxString logFileName, workDir, configName, printLine, styleFileName;
	wxString syncToServer;
public:
	RunningData() :portNo(32256), isRunningAsService(false), serviceName("PharmaPOS Server"), serverName("127.0.0.1"), useSSL(false), useSSLforRegistration(true),
		useSSLforLiveConnection(true), isDistributedDB(false), localMasterName(""), localTransName(""), numberOfWorkerThread(10), nWebSocketServer(1), configName("config.xml"), styleFileName("styles.js") {}
};

extern RunningData gRunningData;

extern bool gStockRecordUseLocalPrice;
extern const wxString wpLogicalPathSeparator;
//namespace MessagePrefix {
//	extern const wxString broadcast; // server sending broadcast to all clients
//	extern const wxString cont; // continuation of previous message
//	extern const wxString command; // server sending command to this specific connection
//	extern const wxString reply; // replying to the above command
//
//	bool IsPrefix(const wxString& s);
//}


extern wxString ReceiveCommandMethod[];// = {"po", "invoice","suspend"};
extern wxString SalesCommandMethod[];// = {"","suspend","quotation"};
extern wxString SalesTableName[];// = { "Sales", "SuspendedSales", "Quotation" };
extern wxString ReceiveTableName[];// = { "PurchaseOrder", "Receive", "SuspendedReceive" };
extern wxString ReceiveTableKey[];// = { "purchaseID", "receiveID", "receiveID" };

namespace TableType {
	enum SalesTypes { Sales, SuspendedSales, Quotation };
	enum ReceiveTypes { PO, Receive, SuspendedInvoice };
}

inline const wxString GetSalesTableName(TableType::SalesTypes t) { return SalesTableName[t]; }
inline const wxString GetReceiveTableName(TableType::ReceiveTypes t) { return ReceiveTableName[t]; }
inline const wxString GetReceiveTableKey(TableType::ReceiveTypes t) { return ReceiveTableKey[t]; }

namespace InvoiceFormat {
	enum InvoiceFormatTypes { Invoice, DeliveryNote, PoisonOrder };
}
extern int wpFACTOR_TO_CENTS;
extern const char *wpVersionString;
extern const char *wpVersionNo;

wxString Translate(const wxString &);
wxString TruncateDecimalPoint(const wxString &);
wxString GetKeyCodeDescription(int code);
int MakeKeyCode(int code, bool isShift, bool isCtrl, bool isAlt);
long CheckKeyCode(int code, bool &isShift, bool &isCtrl, bool &isAlt);
wxString GetSoftwareVersion();

struct mg_connection;

namespace GenericEntity {
    enum Fields {code, name, regNo, remark, description, eof};
}

// address should follow generic
namespace AddressableEntity {
    enum Fields {code, name, regNo, email, telNo, faxNo, contact, address, postcode, state, country, gpsCoord, remark, eof};
}

// person should follow address
namespace PersonEntity {
    enum Fields {code, name, ic, email, telNo, faxNo, contact, address, postcode, state, country, gpsCoord, allergy, medicalHistory, longTermMedication, remark, eof};
}

// should follow generic
namespace InventoryEntity {
    enum Fields {code, name, regNo, colour, detail, barCode, brand, genericName, strength, remark, alternateName, additionalPacking, eof};
}

namespace TypeOfEntity {
	enum Types { Generic, Address, Person, Inventory };
}

class wpObject {
	static int maxFTSColumnSize;
protected:
	wxXmlNodeType nodeType;
	virtual void BuildFTSColumn() {} // load content into ftsColumn
	virtual void ExportFTSColumn() {} // build content from ftsColumn
	virtual void ClearFTSColumn() { ftsColumn.clear(); ftsColumn.resize(maxFTSColumnSize); }
public:
	enum EntityType { UserDef = 0, Entity, Address, Person, Inventory };
	wxString ops;
	wxString method;
	wxString sync;
	wxString searchParam;
	UniversalUniqueID linkID;
	wxString option;
	wxString returnCode;
	wxString errorMessage;
	bool isNewObject;
	bool isDeleted;
	static char attributeMark;
	static char valueMark;

	std::vector<wxString> ftsColumn;

	bool isUpdatedRemote; // return from update on remote server and return is OK
	bool updateFromRemote; // request to update from remote server
	bool toUpdateRemote; // attempt to update remote server.

	wxString md5signature;
	wxDateTime lastUpdateDateTime;
	wxString serverIDtoRun, groupIDtoRun, originServer, senderID;
	virtual int GetNumberOfFTSColumn() const { return 0; }
	virtual void Init() { isDeleted = isNewObject = false;  }
	void ClearTracking() { serverIDtoRun = groupIDtoRun = originServer = senderID = ""; }
protected:
	std::unordered_map<wxString, wxString*> mapString;
	std::unordered_map<wxString, wxDateTime*> mapDate;
	std::unordered_map<wxString, wxLongLong*> mapLongLong;
	std::unordered_map<wxString, int*> mapInt;
	std::unordered_map<wxString, long*> mapLong;
	std::unordered_map<wxString, double*> mapDouble;
	std::unordered_map<wxString, UniversalUniqueID*> mapUUID;
	std::unordered_map<wxString, bool*> mapBool;
	std::unordered_map<void*, wxString> mapPointer;
	//wxPointerToStringHashMap mapPointer;

	std::unordered_set<wxString> keyExists;

	void ExportFTSColumnToString(wxString& content) const;
	void BuildFTSColumnFromString(const wxString& content);
	void CheckKeyExists(const wxString& s);
public:
	wxString *FindInString(const wxString &e);
	wxDateTime *FindInDateTime(const wxString &e);
	wxLongLong *FindInLongLong(const wxString &);
	int *FindInInt(const wxString &);
	long *FindInLong(const wxString&);
	double *FindInDouble(const wxString &);
	UniversalUniqueID *FindInUUID(const wxString &);
	bool* FindInBool(const wxString&);

	const wxString *FindInString(const wxString &e) const;
	const wxDateTime *FindInDateTime(const wxString &e) const;
	const wxLongLong *FindInLongLong(const wxString &) const;
	const int *FindInInt(const wxString &) const;
	const long *FindInLong(const wxString&) const;
	const double *FindInDouble(const wxString &) const;
	const UniversalUniqueID *FindInUUID(const wxString &) const;
	const bool *FindInBool(const wxString &) const;
public:
	void Insert(const wxString& s, wxString* p) { CheckKeyExists(s);  mapString[s] = p; mapPointer[p] = s; }
	void Insert(const wxString &s, wxDateTime *p) { CheckKeyExists(s); mapDate[s] = p; mapPointer[p] = s; }
	void Insert(const wxString &s, wxLongLong *p) { CheckKeyExists(s); mapLongLong[s] = p; mapPointer[p] = s; }
	void Insert(const wxString &s, long *p) { CheckKeyExists(s); mapLong[s] = p; mapPointer[p] = s; }
	void Insert(const wxString &s, int* p) { CheckKeyExists(s); mapInt[s] = p; mapPointer[p] = s; }
	void Insert(const wxString &s, double *p) { CheckKeyExists(s); mapDouble[s] = p; mapPointer[p] = s; }
	void Insert(const wxString &s, UniversalUniqueID *p) { CheckKeyExists(s); mapUUID[s] = p; mapPointer[p] = s; }
	void Insert(const wxString &s, bool* p) { CheckKeyExists(s); mapBool[s] = p; mapPointer[p] = s; }
	wxString GetKey(void *d) const;
	bool SetValue(const wxString& key, const wxString& val);
	virtual bool IsEmpty();
	virtual const EntityType GetEntityType() { return UserDef; }
public:
	wpObject();
	wpObject(const wpObject &x);
	wpObject &operator=(const wpObject &x);
	virtual ~wpObject() {}
	virtual void Clone(wpObject *);
	void SetNodeType(wxXmlNodeType t) { nodeType = t; }
	virtual void CopyToEmptyFieldOnly(const wpObject &);
	virtual const wxString GetDocumentName() const = 0;// { return "wpObject"; }
	virtual void Clear();
	static wxString ConvertValueMarkToCRLF(const wxString &s);
	static wxString ConvertCRLFToValueMark(const wxString &s);

	virtual void LoadUnregisteredXML(const wxString &/*first*/, const wxString &/*second*/) {}
	virtual void GetUnregisteredXML(XMLTag &) const {}
	wxString GetXMLString() {
		XMLTag doc;
		GetXMLDoc(doc);
		return doc.GetString();
	}
	bool GetXMLDoc(wxXmlDocument &doc) {
		wxXmlNode *node = new wxXmlNode(nodeType, GetDocumentName());
		doc.SetRoot(node);
		return GetXMLNode(node);
	}
	virtual bool GetXMLNode(wxXmlNode *node);
	bool GetXML(wxXmlNode* node) {
		return GetXMLNode(node);
	}
	bool GetXML(XMLTag& t) { return GetXMLDoc(t); }

	virtual wxJSONValue GetJSON();
	wxString GetJSONString();



	bool LoadXMLString(const wxString& s) {
		XMLTag doc;
		if (doc.Parse(s))
			return LoadXMLDoc(doc);
		return false;
	}
	bool LoadXMLDoc(const wxXmlDocument &doc) {
		const wxXmlNode *node = doc.GetRoot();
		return LoadXMLNode(node);
	}
	virtual bool LoadXMLNode(const wxXmlNode *node);
	virtual void GetUnregisteredXMLNode(wxXmlNode *) const {}

	bool LoadXML(const wxXmlNode *node) {
		return LoadXMLNode(node);
	}
	bool LoadXML(const XMLTag &tg) { return LoadXMLDoc(tg); }

	virtual bool LoadJSON(const wxJSONValue &v);
	bool LoadJSONString(const wxString &v);
};

struct PairRecord: public wpObject {
    virtual const wxString GetDocumentName() const { return "PairRecord"; }
	wxString key, value;
public:
	PairRecord();
	PairRecord(const PairRecord &rec);
};

struct StockSearchParam {
	wxLongLong stockType;
	wxLongLong poisonCategory;
	int isPoison;
	int isPseudoPoison;
	int showCost;
public:
	StockSearchParam() :stockType(0), poisonCategory(0), isPoison(0), isPseudoPoison(0), showCost(0) {}
};

struct Parameter : public wpObject {
    wxString tableName;
    wxString id;
	wxString intParam;
	wxString data;
	StockSearchParam stockParam;
public:
	Parameter();
	Parameter(const Parameter &p);
    virtual const wxString GetDocumentName() const { return "Parameter"; }
};

struct TableDependencyRecord: public wpObject {
	wxString tableName, columnName;
public:
	TableDependencyRecord();
	TableDependencyRecord(const TableDependencyRecord &x);
    virtual const wxString GetDocumentName() const { return "TableDependencyRecord"; }
};

struct OPSTableRowRecord: public wpObject {
    wxString tableName;
    wxString keyColumnName;
    wxString keyValue;
    wxString id;
	wxString parentID, grandParentID, greatGrandParentID;
	wxString code;
    wxString name;
    wxString filterColumnName;
    wxString filterValue;
    wxString orderBy;
    wxString filterValueViaRegistry;
	std::vector<std::shared_ptr<TableDependencyRecord>> dependencies;
public:
	OPSTableRowRecord();
    OPSTableRowRecord(const OPSTableRowRecord &p);
    virtual const wxString GetDocumentName() const { return "OPSTableRowRecord"; }
	OPSTableRowRecord &operator=(const OPSTableRowRecord &x);
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
    void Clear() { wpObject::Clear(); dependencies.clear();}
    virtual bool IsEmpty() {
		if (wpObject::IsEmpty())
			return dependencies.empty();
		return false;
	}
	virtual wxJSONValue GetJSON();
	virtual bool LoadJSON(const wxJSONValue &v);
};

struct StockBalanceSnapShot: public wpObject {
	wxString
		stockID,
		qoh,
		averageCost,
		actualCost,
		latestCost,
		qohIn,
		qohOut,
		qohAdj;
public:
	StockBalanceSnapShot();
	StockBalanceSnapShot(const StockBalanceSnapShot &);
    virtual const wxString GetDocumentName() const { return "StockBalanceSnapShot"; }

};

template <class T> struct RecordList : public wpObject {
public:
	wxString id;
    std::vector<std::shared_ptr<T>> list;
public:
	RecordList() { Init(); Insert("id", &id); }
	RecordList(const RecordList& r) : wpObject(r) { Init(); Insert("id", &id); list = r.list; }
    RecordList &operator=(const RecordList &x) { wpObject::operator=(x); list=x.list; return *this; }
    virtual const wxString GetDocumentName() const { return "RecordList"; }
    void Clear() { wpObject::Clear(); list.clear(); }
    bool LoadXMLNode(const wxXmlNode *node) {
        if (!wpObject::LoadXMLNode(node)) return false;
        for (const wxXmlNode *childNodes = node->GetChildren(); childNodes; childNodes = childNodes->GetNext()) {
			if (childNodes->GetName().IsSameAs("List",false)) {
				for (const wxXmlNode *gchildNodes = childNodes->GetChildren(); gchildNodes; gchildNodes = gchildNodes->GetNext()) {
                    std::shared_ptr<T> rec(new T);
                    rec->LoadXMLNode(gchildNodes);
                    list.push_back(rec);
                }
            }
        }
		return true;
    }
    bool GetXMLNode(wxXmlNode *node) {
        if (!wpObject::GetXMLNode(node)) return false;
        if (list.size() > 0) {
            wxXmlNode *child = new wxXmlNode(node, nodeType, "List");
            for (typename std::vector<std::shared_ptr<T>>::const_iterator it = list.begin(); it != list.end(); it++) {
				std::shared_ptr<T> p = *it;
                p->GetXMLNode(new wxXmlNode(child, nodeType, "Row"));
            }
        }
		return true;
    }
    bool IsEmpty() {
        if (!wpObject::IsEmpty()) return false;
		for (typename std::vector<std::shared_ptr<T>>::const_iterator it = list.begin(); it != list.end(); it++) {
			std::shared_ptr<T> p = *it;
			if (!p->IsEmpty()) return false;
		}
        return true;
    }
	virtual wxJSONValue GetJSON() {
		wxJSONValue val = wpObject::GetJSON();
        if (list.size() > 0) {
			wxJSONValue v;
            for (typename std::vector<std::shared_ptr<T>>::const_iterator it = list.begin(); it != list.end(); it++) {
				std::shared_ptr<T> p = *it;
				v.Append(p->GetJSON());
            }
			val["List"] = v;
        }
		return val;
	}
	virtual bool LoadJSON(const wxJSONValue &v) {
        bool res = wpObject::LoadJSON(v);
		wxJSONValue a = v.ItemAt("List");
		if (a.GetType() == wxJSONTYPE_INVALID) return res;
		if (a.GetType() == wxJSONTYPE_ARRAY) {
			for (int i=0; i<a.Size(); i++) {
				std::shared_ptr<T> rec(new T);
				rec->LoadJSON(a.ItemAt(i));
				list.push_back(rec);
			}
		}
		return res;
	}
};

struct SQLCommandRecord: public wpObject {
	wxString sql, sqlSelect, sqlCount;
public:
	SQLCommandRecord();
	SQLCommandRecord(const SQLCommandRecord &x);
    virtual const wxString GetDocumentName() const { return "SQLCommand"; }
};

struct PassportRecord : public wpObject {
	wxString id;
	wxString rawData;
public:
	PassportRecord();
	PassportRecord(const PassportRecord &x);
	virtual const wxString GetDocumentName() const { return "PassportRecord"; }
};
struct ScreenPerspectiveRecord: public wpObject {
    wxString screenName, perspective;
    wxString centerPaneName;
public:
    ScreenPerspectiveRecord();
    ScreenPerspectiveRecord(const ScreenPerspectiveRecord &x);
    virtual const wxString GetDocumentName() const { return "ScreenPerspective"; }
};

struct RequestReport : public wpObject {
    wxDateTime fromDate, toDate;
	wxString typeOfSales;
public:
    RequestReport();
    RequestReport(const RequestReport &x);
    virtual const wxString GetDocumentName() const { return "RequestReport"; }
};

struct RequestInfoRecord: public wpObject {
public:
    wxString question;
    wxString answer;
    wxString userID;
    wxDateTime fromDate;
    wxDateTime toDate;
public:
    RequestInfoRecord();
    RequestInfoRecord(const RequestInfoRecord &x);
    virtual const wxString GetDocumentName() const { return "RequestInfo"; }
};

// could make it more flexible for multiple log in and out.
struct AttendanceRecord: public wpObject {
public:
	wxString entityID;
    wxDateTime date;
    wxString ioMode;
    wxString verifyMode;
	wxString workCode;
    wxString remark;
public:
    AttendanceRecord();
    AttendanceRecord(const AttendanceRecord &x);
    virtual const wxString GetDocumentName() const { return "AttendanceRecord"; }
};

struct LexiconRecord : public wpObject {
    wxString id;
    wxString code, name;
	wxString parentID;
	wxString synonymID;
public:
    LexiconRecord();
    LexiconRecord(const LexiconRecord &);
    LexiconRecord(const wxString &i, const wxString &c=wxEmptyString, const wxString &n=wxEmptyString);
    LexiconRecord(long i, const wxString &c=wxEmptyString, const wxString &n=wxEmptyString);
    virtual const wxString GetDocumentName() const { return "Lexicon"; }
};

struct DatabaseRecord : public LexiconRecord {
	wxString transactionName;
	wxString dbName;
	wxString scriptFolder;
	wxString wwwFolder;
public:
	DatabaseRecord();
	DatabaseRecord(const DatabaseRecord &r);
	DatabaseRecord(const wxString &i, const wxString &c = wxEmptyString, const wxString &n = wxEmptyString);
	DatabaseRecord(long i, const wxString &c = wxEmptyString, const wxString &n = wxEmptyString);
    virtual const wxString GetDocumentName() const { return "DatabaseRecord"; }
};

struct BranchRecord : public LexiconRecord {
	wxString transDB;
	wxString url;
public:
	BranchRecord();
	BranchRecord(const BranchRecord &r);
	virtual const wxString GetDocumentName() const { return "BranchRecord"; }
};

struct ShopRecord : public LexiconRecord {
	wxString masterDB;
	wxString url;
	std::vector<std::shared_ptr<BranchRecord>> branches;
public:
	ShopRecord();
	ShopRecord(const ShopRecord &r);
	virtual const wxString GetDocumentName() const { return "ShopRecord"; }
	ShopRecord &operator=(const ShopRecord &x);
	virtual bool LoadXMLNode(const wxXmlNode *);
	virtual bool GetXMLNode(wxXmlNode *);
	void Clear() { LexiconRecord::Clear(); branches.clear(); }
	void Clone(wpObject *p);
};

struct EntityRecord: public wpObject {
private:
	wxString content;
	void BuildFTSColumn() { wpObject::BuildFTSColumnFromString(content); } // load content into ftsColumn
	void ExportFTSColumn() { wpObject::ExportFTSColumnToString(content); } // build content from ftsColumn
public:
	wxString id;
	wxString docID;
	wxString globalID;
	LexiconRecord registerAt; // id of entityRecord
	wxDateTime registerDate;
	std::vector<std::shared_ptr<LexiconRecord>> attributes;
	std::unordered_set<wxString> attributeSet;
public:
    EntityRecord();
    EntityRecord(const EntityRecord &x);
    EntityRecord &operator=(const EntityRecord &x);
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	void Clear();
	virtual bool IsEmpty();

	virtual void Clone(wpObject *r);
	virtual int GetNumberOfFTSColumn() const { return GenericEntity::eof - GenericEntity::code; }

	virtual wxString &GetCode() { return ftsColumn[GenericEntity::code]; }
	virtual wxString &GetName() { return ftsColumn[GenericEntity::name]; }
	virtual wxString &GetRegistrationNo() { return ftsColumn[GenericEntity::regNo]; }

	virtual wxString GetCode() const { return ftsColumn[GenericEntity::code]; }
	virtual wxString GetName() const { return ftsColumn[GenericEntity::name]; }
	virtual wxString GetRegistrationNo() const { return ftsColumn[GenericEntity::regNo]; }

	virtual wxString &GetRemark() { return ftsColumn[GenericEntity::remark]; }
	virtual wxString GetRemark() const { return ftsColumn[GenericEntity::remark]; }

	wxString GetContent() { ExportFTSColumn();  return content; }
	void SetContent(const wxString& c) { content = c; BuildFTSColumn();}

	virtual const wxString GetDocumentName() const { return "Entity"; }
	virtual const EntityType GetEntityType() { return Entity; }
	wxString GetAttributeTypeIDList(const wxString &delim=",") const;
};


// assuming that code, name and regno are in the same position for all types of records.
struct LocationRecord : public EntityRecord {
public:
	LocationRecord() : EntityRecord() { Init(); }
	LocationRecord(const LocationRecord& x) : EntityRecord(x) { Init(); }
    virtual const wxString GetDocumentName() const { return "Location"; }
	virtual const EntityType GetEntityType() { return Address; }

	virtual int GetNumberOfFTSColumn() const { return AddressableEntity::eof - AddressableEntity::code; }
	virtual wxString &GetRemark() { return ftsColumn[AddressableEntity::remark]; }
	virtual wxString GetRemark() const { return ftsColumn[AddressableEntity::remark]; }

	wxString &GetTelNo() { return ftsColumn[AddressableEntity::telNo]; }
	wxString &GetFaxNo() { return ftsColumn[AddressableEntity::faxNo]; }
	wxString &GetContact() { return ftsColumn[AddressableEntity::contact]; }
	wxString &GetAddress() { return ftsColumn[AddressableEntity::address]; }
	wxString &GetPostCode() { return ftsColumn[AddressableEntity::postcode]; }
	wxString &GetState() { return ftsColumn[AddressableEntity::state]; }
	wxString &GetEmail() { return ftsColumn[AddressableEntity::email]; }

	wxString GetTelNo() const { return ftsColumn[AddressableEntity::telNo]; }
	wxString GetTelNo(int idx) const;
	wxString GetFaxNo() const { return ftsColumn[AddressableEntity::faxNo]; }
	wxString GetContact() const { return ftsColumn[AddressableEntity::contact]; }
	wxString GetAddress() const { return ftsColumn[AddressableEntity::address]; }
	wxString GetPostCode() const { return ftsColumn[AddressableEntity::postcode]; }
	wxString GetState() const { return ftsColumn[AddressableEntity::state]; }
	wxString GetEmail() const { return ftsColumn[AddressableEntity::email]; }

};

struct CompanyRecord: public LocationRecord {
	wxString discount;
	wxString creditterm, debitterm, creditLimit;
	int isTaxable;
public:
    CompanyRecord();
    CompanyRecord(const CompanyRecord &x);
	wxString &GetRegNo() { return GetRegistrationNo(); }
	wxString GetRegNo() const { return GetRegistrationNo(); }
    virtual const wxString GetDocumentName() const { return "Company"; }
	bool IsGSTApplicable() const { if (id.IsEmpty()) return true; else return isTaxable > 0; }
};

struct MyKadData : public wpObject {
	wxString name, ic, oldic, sex, stateOfBirth, nationality, race, religion, postcode;
	std::vector<wxString> address;
	wxDateTime dob, validityDate;
public:
	MyKadData();
	MyKadData(const MyKadData &rec);
	virtual const wxString GetDocumentName() const { return "MyKadData"; }
	MyKadData &operator=(const MyKadData &);
	virtual bool LoadXMLNode(const wxXmlNode *);
	virtual bool GetXMLNode(wxXmlNode *tg);
	void Clear();
	virtual bool IsEmpty();
};

struct PersonRecord: public CompanyRecord {
    wxDateTime dob;
    LexiconRecord bloodType, race, religion, maritalStatus, gender, job;
public:
    PersonRecord();
    PersonRecord(const PersonRecord &x);
    PersonRecord &operator=(const PersonRecord &x);
	virtual const EntityType GetEntityType() { return Person; }

	void CopyMyKad(MyKadData &dat);
	virtual void Clone(wpObject *p);
	virtual int GetNumberOfFTSColumn() const { return PersonEntity::eof - PersonEntity::code; }
	virtual wxString &GetRemark() { return ftsColumn[PersonEntity::remark]; }
	virtual wxString GetRemark() const { return ftsColumn[PersonEntity::remark]; }

	wxString &GetIC() { return GetRegistrationNo(); }
	wxString &GetAllergy() { return ftsColumn[PersonEntity::allergy]; }
	wxString &GetMedicalHistory() { return ftsColumn[PersonEntity::medicalHistory]; }
	wxString &GetLongTermMedication() { return ftsColumn[PersonEntity::longTermMedication]; }

	wxString GetIC() const { return GetRegistrationNo(); }
	wxString GetAllergy() const { return ftsColumn[PersonEntity::allergy]; }
	wxString GetMedicalHistory() const { return ftsColumn[PersonEntity::medicalHistory]; }
	wxString GetLongTermMedication() const { return ftsColumn[PersonEntity::longTermMedication]; }

    virtual const wxString GetDocumentName() const { return "Person"; }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
    void Clear() { CompanyRecord::Clear(); bloodType.Clear(); race.Clear();religion.Clear();maritalStatus.Clear();gender.Clear(); }
    virtual bool IsEmpty();
};

struct MembershipRecord: public PersonRecord {
    wxString pointBalance;
	wxString enrollNumber;
    std::vector<wxString> dependencyList;
public:
    MembershipRecord();
    MembershipRecord(const MembershipRecord&x);
    virtual const wxString GetDocumentName() const { return "Membership"; }
    double GetDiscount() { return wxAtof(discount); }
	double GetPointValue() { return wxAtof(pointBalance); }
    MembershipRecord &operator=(const MembershipRecord &x);
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
    void Clear() { PersonRecord::Clear(); dependencyList.clear(); }
	virtual void Clone(wpObject *p);
};

struct MembershipPointRecord : public wpObject {
    wxDateTime dateOfTransaction;
    wxString receiptNo;
    wxString amount;
    wxString points;
    wxString balance;
public:
    MembershipPointRecord();
    MembershipPointRecord(const MembershipPointRecord &);
    virtual const wxString GetDocumentName() const { return "MembershipPoint"; }
};

struct AccountingRecord : public wpObject {
	wxDateTime dateOfTransaction;
	wxString refNo;
	wxString debit;
	wxString credit;
	wxString remark;
public:
	AccountingRecord();
	AccountingRecord(const AccountingRecord &);
    virtual const wxString GetDocumentName() const { return "AccountingRecord"; }
};

struct PengantinRecord : public wpObject {
	wxString id;
    PersonRecord lelaki;
    PersonRecord perempuan;

	//virtual void BuildFTSColumn() { wpObject::BuildFTSColumn(); lelaki.BuildFTSColumn(); perempuan.BuildFTSColumn(); } // load content into ftsColumn
	//virtual void ExportFTSColumn() { wpObject::ExportFTSColumn(); lelaki.ExportFTSColumn(); perempuan.ExportFTSColumn(); } // build content from ftsColumn

public:
    PengantinRecord();
    PengantinRecord(const PengantinRecord &x);
    PengantinRecord &operator=(const PengantinRecord &x);

    virtual const wxString GetDocumentName() const { return "Pengantin"; }
    void Clear() { wpObject::Clear(); lelaki.Clear(); perempuan.Clear(); }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
    virtual bool IsEmpty();
};

struct TypeRecord : public LexiconRecord {
	wxString limitValue, defaultValue;
public:
    TypeRecord();
    TypeRecord(const TypeRecord &);
    TypeRecord(const wxString &i, const wxString &tID, const wxString &c=wxEmptyString, const wxString &n=wxEmptyString);
    TypeRecord(long i, long tID, const wxString &c=wxEmptyString, const wxString &n=wxEmptyString);
    virtual const wxString GetDocumentName() const { return "Types"; }
};

struct PriceCategoryRecord : public TypeRecord {
	wxString price;
	wxString localSellingPrice;
	wxString& percentAboveCost() { return limitValue; }
public:
	PriceCategoryRecord();
	PriceCategoryRecord(const PriceCategoryRecord &);
	virtual const wxString GetDocumentName() const { return "PriceCategoryRecord"; }
};

struct TerminalRecord: public LexiconRecord {
    wxString location;
public:
    TerminalRecord();
    TerminalRecord(const TerminalRecord &);
    virtual const wxString GetDocumentName() const { return "Terminal"; }
};

struct MakeUpArtistRecord : public PersonRecord {
private:
    wxString perSessionRate;
public:
	wxString timeStart;
    LexiconRecord type;
    wxDateTime tarikh;
    LocationRecord tempat;
public:
	wxDateTime &GetAppointmentDateAndTime();
    wxString ParseAppointmentDateAndTime();
public:
    MakeUpArtistRecord();
    MakeUpArtistRecord(const MakeUpArtistRecord &);
    MakeUpArtistRecord &operator=(const MakeUpArtistRecord &x);

	virtual const wxString GetDocumentName() const { return "MakeupArtist"; }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	const wxString GetRate() const { return perSessionRate; }
	void SetRate(const wxString &rate) { perSessionRate = rate; }
	double GetAmount() const { return wxAtof(perSessionRate); }
	wxLongLong GetAmountInCents() const { return GetValueInCents(perSessionRate); }
	void Clear() {PersonRecord::Clear(); type.Clear(); tempat.Clear(); tarikh = wxDateTime::Now();}
    virtual bool IsEmpty();
	//virtual void BuildFTSColumn() { PersonRecord::BuildFTSColumn(); tempat.BuildFTSColumn();} // load content into ftsColumn
	//virtual void ExportFTSColumn() { PersonRecord::ExportFTSColumn(); tempat.ExportFTSColumn();} // build content from ftsColumn

};

struct UserRecord : public PersonRecord {
	std::unordered_set<long> userRoles; // non-migratable - just ids of lexicons in roles;

	wxString encryptedPassword;
	wxString sessionID;
	wxDateTime registerDate;
	wxString menuXML;
	wxString applicationName;
	wxString schemaName;
	wxString startUpModule;
	std::unordered_set<wxString> allowedModules;

    wxString maxDiscountAllowed;
    std::vector<std::shared_ptr<LexiconRecord>> roles;
    std::vector<std::shared_ptr<LexiconRecord>> screens;
    std::unordered_set<wxString> rolesSet, screenSet;
public:
	static wxString serverAppUserCode;
    UserRecord();
    UserRecord(const UserRecord &);
    UserRecord &operator=(const UserRecord &x);
    virtual const wxString GetDocumentName() const { return "Users"; }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	virtual void Clear() { PersonRecord::Clear(); roles.clear(); screens.clear(); rolesSet.clear(); screenSet.clear(); allowedModules.clear(); userRoles.clear();}
	virtual void Clone(wpObject *p);
	bool IsServerAppUser() const { return GetCode().IsSameAs(serverAppUserCode); }
	bool IsAdminUser() const;
    bool IsRoleEnabled(const wxString &role) const;
    bool HasScreen(const wxString &role) const;
    bool CanEditStock() const {return IsRoleEnabled("Stock") || IsAdminUser(); }
    bool CanViewCost() const {return IsRoleEnabled("Cost") || IsAdminUser(); }
    bool CanOpenDrawer() const { return IsRoleEnabled("OpenDrawer") || IsRoleEnabled("Alt-O") || IsAdminUser(); }
    bool CanDoSalesReport() const { return IsRoleEnabled("SalesReport") || IsAdminUser(); }
	bool CanDoClosingReport() const { return IsRoleEnabled("ClosingReport") || IsAdminUser(); }
	bool CanShowExplorer() const {return IsRoleEnabled("DataExplorer") || IsAdminUser();}
    bool CanShowReceipt() const {return IsRoleEnabled("ReceiptViewer") || IsAdminUser();}
	bool CanDoWholeSales() const {return IsRoleEnabled("Wholesales") || IsAdminUser();}
    bool CanDoDiscountAtTotal() const {return IsRoleEnabled("DiscountAtTotal") || IsAdminUser();}
	bool CanModifyQOH() const;
    bool CanDoNegativeSales() const {return IsRoleEnabled("NegativeSales") || IsAdminUser();}
    bool CanReceiveReturnSales() const {return IsRoleEnabled("ReturnByCustomer") || IsAdminUser();}
    bool CanSellBelowCost() const {return IsRoleEnabled("BelowCostSales") || IsAdminUser();}
	bool CanOverrideMaxDiscount() const {return IsRoleEnabled("OverrideMaxDiscount") || IsAdminUser();}
    bool CanDisableReceiptPrinter() const { return IsRoleEnabled("DisableReceipt") || IsAdminUser(); }
    bool CanDeleteSales() const { return IsRoleEnabled("DeleteSales") || IsAdminUser(); }
    bool CanDeletePurchase() const { return IsRoleEnabled("DeletePurchase") || IsAdminUser(); }
	bool CanDeleteJournal() const { return IsRoleEnabled("DeleteJournal") || IsAdminUser(); }
	bool CanReceiveStock() const { return IsRoleEnabled("ReceiveStock") || IsAdminUser(); }
    bool CanDoInventory() const { return IsRoleEnabled("Inventory") || IsRoleEnabled("ModifyQOH") || IsAdminUser(); }
	bool CanValidateQOH() const { return IsRoleEnabled("ValidateQOH") || IsAdminUser(); }
	bool CanEditSales() const { return IsRoleEnabled("EditSales") || IsAdminUser(); }
	bool CanAdjustTransDate() const { return IsRoleEnabled("AdjustSalesDate") || IsAdminUser(); }
	double GetMaxDiscount() const { return IsAdminUser() ? 100.00 : wxAtof(maxDiscountAllowed); }
    virtual bool IsEmpty();

	bool IsWholesales() const {return HasScreen("Wholesales");}
	bool IsPointOfSales() const {return HasScreen("PointOfSales");}
	bool IsReceiveStock() const {return HasScreen("ReceiveStock");}
	bool IsTicketingSales() const { return HasScreen("TicketingSales"); }

};

struct PackingRecord : public wpObject {
	wxString stockID;
    LexiconRecord packType;
    wxString packSize, packDescription;
    wxString sellingPrice, localSellingPrice;
    wxString maxDiscountPercentage;
	wxString percentAboveCost;
	//wxString qohAdjust, costPrice;
	std::vector<std::shared_ptr<PriceCategoryRecord>> priceByCategory;
public:
    PackingRecord();
    PackingRecord(const PackingRecord &);
    PackingRecord &operator=(const PackingRecord &x);
    virtual const wxString GetDocumentName() const { return "Packing"; }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
    virtual bool IsEmpty();
    virtual void Clear() { wpObject::Clear(); packType.Clear();}

	void SetPrice(const wxString &p) { gStockRecordUseLocalPrice ? localSellingPrice=p : sellingPrice=p; }
	const wxString& GetPrice() const { return gStockRecordUseLocalPrice ? localSellingPrice : sellingPrice; }
	wxLongLong GetPriceInCents() const { return GetValueInCents(GetPrice()); }
};

bool inline PackRecordCompareDescending (const std::shared_ptr<PackingRecord> &first, const std::shared_ptr<PackingRecord> &second) { return wxAtol(first->packSize) > wxAtol(second->packSize); }
bool inline PackRecordCompareAscending (const std::shared_ptr<PackingRecord> &first, const std::shared_ptr<PackingRecord> &second) { return wxAtol(first->packSize) < wxAtol(second->packSize); }

struct StockTaxAndCharges :public wpObject {
	TypeRecord type;
	wxDateTime fromDate, toDate;
	wxString percentage, fixedValue;
public:
	StockTaxAndCharges();
	StockTaxAndCharges(const StockTaxAndCharges &);
    StockTaxAndCharges &operator=(const StockTaxAndCharges &x);
    virtual const wxString GetDocumentName() const { return "StockTaxAndCharges"; }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	virtual void Clear() { wpObject::Clear(); type.Clear();}
	virtual void Clone(wpObject *p);

};

struct StockRecord: public EntityRecord {
	wxString dispensingInstruction, drugWarning;
public:
	wxString noOfImages, fastLookupCode;

	wxString reorderLevel, reorderQty;
	int isPoison, isPseudoPoison, isStockItem;
    wxString weightedAvgCost; // always at the lowest sellable unit.
	wxString latestCost, maxCost;
    wxString averageCostPPOS;
	wxString rewardPointFactor;
    wxString qoh, stockValue, qohHold;
	wxString promotionalDiscount;
	wxString fcc;
	wxString displayName;

    LexiconRecord type;
    LexiconRecord defaultSellingPack;
    LexiconRecord defaultPurchasePack;
    wxString minProfitMargin;
    wxString synonymTo;
    std::vector<std::shared_ptr<PackingRecord>> packRecords;
	std::vector<std::shared_ptr<StockTaxAndCharges>> taxAndCharges;
	std::vector<std::shared_ptr<LexiconRecord>> shelfList;
	std::vector<std::shared_ptr<PersonRecord>> salesReps;
	int convertPacksizeOldData;
public:
    StockRecord();
    StockRecord(const StockRecord &);
    StockRecord &operator=(const StockRecord &x);
	virtual const wxString GetDocumentName() const { return "Stock"; }
	virtual const EntityType GetEntityType() { return Inventory; }

	double GetTaxPercentage(const wxString &taxTypeID, wxDateTime x=wxDateTime::UNow()) const;
	bool IsTaxExempted(const wxString &taxTypeID, wxDateTime x=wxDateTime::UNow()) const;
	virtual int GetNumberOfFTSColumn() const { return InventoryEntity::eof - InventoryEntity::code; }
	virtual wxString &GetRemark() { return ftsColumn[InventoryEntity::remark]; }
	virtual wxString GetRemark() const { return ftsColumn[InventoryEntity::remark]; }
	wxString &GetAlternateName() { return ftsColumn[InventoryEntity::alternateName]; }
	wxString &GetWarna() { return ftsColumn[InventoryEntity::colour]; }
	wxString &GetPerincian() { return ftsColumn[InventoryEntity::detail]; }
	wxString &GetBarCode() { return ftsColumn[InventoryEntity::barCode]; }
	wxString &GetBrand() { return ftsColumn[InventoryEntity::brand]; }
	wxString &GetGenericName() { return ftsColumn[InventoryEntity::genericName]; }
	wxString &GetStrength() { return ftsColumn[InventoryEntity::strength]; }
	wxString &GetWhsPacking() { return ftsColumn[InventoryEntity::additionalPacking]; }

	wxString &GetDetail1() { return GetWarna(); }
	wxString &GetDetail2() { return GetPerincian(); }
	wxString GetDisplayName() { return displayName; }
	wxString GetAlternateName() const { return ftsColumn[InventoryEntity::alternateName]; }
	wxString GetWarna() const { return ftsColumn[InventoryEntity::colour]; }
	wxString GetPerincian() const { return ftsColumn[InventoryEntity::detail]; }
	wxString GetBarCode() const { return ftsColumn[InventoryEntity::barCode]; }
	wxString GetBrand() const { return ftsColumn[InventoryEntity::brand]; }
	wxString GetGenericName() const { return ftsColumn[InventoryEntity::genericName]; }
	wxString GetStrength() const { return ftsColumn[InventoryEntity::strength]; }
	wxString GetWhsPacking() const { return ftsColumn[InventoryEntity::additionalPacking]; }
	wxString GetDetail1() const { return GetWarna(); }
	wxString GetDetail2() const { return GetPerincian(); }
	wxString GetDispensingInstruction(const wxString &dosageQty, const wxString &dosageFreq) const;
	wxString GetDrugWarning() const { return drugWarning; }
	virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	void Clear() { EntityRecord::Clear(); type.Clear(); packRecords.clear(); taxAndCharges.clear(); shelfList.clear(); salesReps.clear(); }
    wxString GetDescription() const;
    virtual bool IsEmpty();
    bool IsSalesEmpty();
    bool IsNewProduct() const;

    bool CheckPacking(const wxString &packing);
    bool CheckPrice(const wxString &price, const wxString &packing, const std::unordered_set<wxString> *attributes);
	bool CheckBelowCost(const wxString &price, const wxString &packing) const;
	bool CheckBelowCost(double price, const wxString &packing) const;
	double GetStockAverageCost(const wxString &packing) const;// { return wxAtof(weightedAvgCost); }

	void SetPrice(const wxString &price, const wxString &packType);
	void SetPackSize(const wxString &size, const wxString &packType);
	wxString GetPackSize(const wxString &packType) const;
	wxString GetPrice(const wxString &packType) const;
	wxString GetPriceWithGST(const wxString &packType, const wxString &gstTaxTypeID) const;
	wxString GetPackDescription(const wxString &packType) const;
	const PackingRecord &GetPack(const wxString &packType) const;
	wxString GetPackID(const wxString &packType) const;
	wxLongLong GetPriceInCents(const wxString &packType) const;
    virtual wxString GetSellingPrice(const wxString &packType, const std::unordered_set<wxString> *attributes, bool *isGetFromSpecialPrice = NULL) const;
	virtual double GetSellingPrice() const { return wxAtof(GetPrice(GetDefaultPack()));}
    void SetMaxDiscount(const wxString &disc, const wxString &packType);
    const wxString GetMaxDiscount(const wxString &packType) const;

    wxString GetBiggestPack() const; // get first one
    wxString GetSmallestPack() const; // get last one
	wxString GetDefaultPack() const; // get the one with not desc;

    wxString GetPackList();
    wxString ShowPackingQuantity(long qLoose);
    wxString ShowPackingQuantity(const wxString &v) { return ShowPackingQuantity(wxAtol(v)); }

    long GetQOH() const { return wxAtol(qoh); }
    virtual double GetStockValue() const { return wxAtof(stockValue); }
    virtual double GetAverageCost() const;
	double GetCostRef(const wxString &key);
	void SortPackRecordDescendingSize() { std::sort(packRecords.begin(), packRecords.end(), PackRecordCompareDescending); }
	void SortPackRecordAscendingSize() { std::sort(packRecords.begin(), packRecords.end(), PackRecordCompareAscending);}
};

struct StockTransactionRecord : public StockRecord {
    wxString quantity, pack;
    wxString bonusQty, bonusPack;
    wxString transactionPrice, listPrice, discountAmount, srp;
	wxString costValue, transactionValue, taxValue;
    wxDateTime expiryDate;
    wxString batchNo;
	wxString receiveDetailID;
    wxDateTime tarikhFitting, tarikhDeliver, tarikhReturn;
	wxString promoterID;
	bool isSellingPriceManuallyChanged; // used only for grid
public:
    StockTransactionRecord();
    StockTransactionRecord(const StockTransactionRecord &);
	double GetSalesAmount() { return wxAtof(transactionValue); } //transactionPrice)*wxAtof(quantity) - wxAtof(discountAmount); }
    double GetPurchasedAmount() const { return wxAtof(transactionValue); }
	double GetAmount() { return wxAtof(transactionValue); }
    wxString GetAmountString();
    wxString GetSalesAmountString() { return GetAmountString(); }
	long GetLooseQuantity() const {return wxAtof(quantity)*wxAtof(GetPackSize(pack)) + wxAtof(bonusQty)*wxAtof(GetPackSize(bonusPack));}
    virtual const wxString GetDocumentName() const { return "StockTransactionRecord"; }
    double GetAverageCost() const;
    double GetStockValue() const { return wxAtof(costValue); }
	double GetNetPrice(const StockTransactionRecord &rec) const;
	double GetNetPrice() const;
	virtual wxString GetSellingPrice(const wxString &packType, const std::unordered_set<wxString> *_attributes, bool *isGetFromSpecialPrice = NULL) const { return StockRecord::GetSellingPrice(packType, _attributes, isGetFromSpecialPrice); }
	virtual double GetSellingPrice() const;
	wxString GetSellingPriceString() const;
	void ReassignPrice(const std::unordered_set<wxString> *attr, bool appyGST=true, double discountPercentage=0);
	void CopyTransOnly(const StockTransactionRecord &);
	void CopyStockOnly(const StockRecord &);
};

struct HistoricalStockRecord : public StockTransactionRecord {
	wxString transactionID;
    wxDateTime dateOfTransaction;
    wxString refNo;
	wxString qoh;
	wxString location, who, reason;
	wxString salesRep;
public:
    HistoricalStockRecord();
    HistoricalStockRecord(const HistoricalStockRecord &);
    virtual const wxString GetDocumentName() const { return "HistoricalStock"; }
};

struct InventoryStatusRecord : public StockRecord {
    wxString month;
    wxString quantityIn;
    wxString quantityOut;
    wxString quantityAdj;
    long runningTotal;
public:
    InventoryStatusRecord();
    InventoryStatusRecord(const InventoryStatusRecord &);
    virtual const wxString GetDocumentName() const { return "InventoryStatus"; }
};

struct SalesPromotionRecord: public wpObject {
    LexiconRecord type;
    wxString discountPercentage;
    wxDateTime fromDate;
    wxDateTime toDate;
public:
    SalesPromotionRecord();
    SalesPromotionRecord(const SalesPromotionRecord &);
    virtual const wxString GetDocumentName() const { return "SalesPromotion"; }
    void Clear() { wpObject::Clear(); type.Clear(); }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
    virtual bool IsEmpty();
};

struct QuotationRecord: public StockRecord {
    wxString qty, qtyBonus, costPrice, bonusScheme, qoh;
public:
    QuotationRecord();
    QuotationRecord(const QuotationRecord &);
    virtual const wxString GetDocumentName() const { return "Quotation"; }
};

struct InventoryRecord: public wpObject {
    LexiconRecord type;
    wxString maxDiscountAllowed;
    wxDateTime fromDate;
    wxDateTime toDate;
public:
    InventoryRecord();
    InventoryRecord(const InventoryRecord &);
    virtual const wxString GetDocumentName() const { return "Inventory"; }
};

// historical sales record is used in association of stock record.
struct HistoricalSalesRecord : public MembershipRecord {
    wxDateTime dateOfTransaction;
    wxString refNo;
    wxString pack;
    wxString quantity;
    wxString price;
	wxString amount;
    wxString bonusPack;
    wxString bonusQty;
	wxString location;
	wxString who;
public:
    HistoricalSalesRecord();
    HistoricalSalesRecord(const HistoricalSalesRecord &);
    virtual const wxString GetDocumentName() const { return "HistoricalSales"; }
};

// the Historical Purchase record is used in association with with stock record.
struct HistoricalPurchaseRecord : public CompanyRecord {
    wxDateTime dateOfTransaction;
    wxString refNo;
    wxString pack;
    wxString quantity;
    wxString bonusQty;
    wxString bonusPack;
    wxString price;
	wxString amount;
    wxString discount;
	wxString batchNo;
	wxDateTime expiryDate;
	wxString location;
	wxString qoh;
	wxString who;
	wxString transactionID;
public:
    HistoricalPurchaseRecord();
    HistoricalPurchaseRecord(const HistoricalPurchaseRecord &);
    virtual const wxString GetDocumentName() const { return "HistoricalPurchase"; }
    double GetAverageCost(const StockRecord &rec) const;
	double GetNetPrice(const StockRecord &rec) const;
};


struct PaymentRecord : public wpObject {
private:
	wxString amount;
    wxString tendered;
public:
    wxString id; // paymentID
    wxString salesID; // if blank, use the SalesRecord id. By itself it should not be blank.
    wxDateTime paymentDate;
    TypeRecord paymentMethod; // type 10 - paidBy
    wxString reference;
	CompanyRecord payor;
	wxString rewardPoint;
public:
    PaymentRecord();
    PaymentRecord(const PaymentRecord &);
    PaymentRecord &operator=(const PaymentRecord &x);
    virtual const wxString GetDocumentName() const { return "Payment"; }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	void SetAmount(const wxString &s) { amount=s; }
	wxLongLong GetAmountInCents() const { return GetValueInCents(amount); }
	double GetAmount() const { return wxAtof(amount); }
    double GetTendered() const { return wxAtof(tendered); }
    void SetTendered(const wxString &t) { tendered = t; }
	const wxString GetTenderedStr() const { return tendered; }
	const wxString GetAmountStr() const { return amount; }
	void Clear() { wpObject::Clear(); paymentMethod.Clear(); paymentDate = wxDateTime::Now(); payor.Clear(); }
    virtual bool IsEmpty();
};

struct EventRecord : public wpObject {
    wxString id;
    wxString name;
    wxDateTime tarikhEvent;
	LexiconRecord eventType;
	LocationRecord location;
	MakeUpArtistRecord artist;
    std::vector<std::shared_ptr<StockRecord>> itemList;
public:
    EventRecord();
    EventRecord(const EventRecord &);
    EventRecord &operator=(const EventRecord &x);
    virtual const wxString GetDocumentName() const { return "Events"; }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);

	void Clear() { wpObject::Clear(); location.Clear(); eventType.Clear(); artist.Clear(); itemList.clear(); }
    virtual bool IsEmpty();
};

struct PengantinOrderRecord: public wpObject {
    wxString id;
    wxDateTime tarikhOrder;
    wxDateTime tarikhInput;
    PengantinRecord pengantinRecord;
    std::vector<std::shared_ptr<EventRecord> > eventList;
    std::vector<std::shared_ptr<PaymentRecord> > paymentList;
public:
    PengantinOrderRecord();
    PengantinOrderRecord(const PengantinOrderRecord &);
    PengantinOrderRecord &operator=(const PengantinOrderRecord &x);

    virtual const wxString GetDocumentName() const { return "PengantinOrder"; }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	wxDateTime GetFirstEvent();
	void Clear() { wpObject::Clear(); pengantinRecord.Clear(); eventList.clear(); paymentList.clear(); tarikhOrder = tarikhInput = wxDateTime::Now(); }
    virtual bool IsEmpty();
};

struct PengantinOrderData : public StockRecord {
    wxString group, rectype;
public:
    PengantinOrderData();
    PengantinOrderData(const PengantinOrderData &);
    virtual const wxString GetDocumentName() const { return "PengantinOrderData"; }
};

struct SearchResultData {
    wxString id;
    wxString code;
    wxString name;
    wxString warna;
    wxString perincian;
    wxString harga;
public:
    void Clear() { id.Clear(); code.Clear(); name.Clear(); warna.Clear(); perincian.Clear(); harga.Clear();}
    wxString GetDescription() const;
};

struct SearchParam : public wpObject {
	wxString search, orderBy, isConsumption, isLowStock, key, value;
	wxString type, id, method2, option2, column;
public:
	SearchParam();
	SearchParam(const SearchParam &x);
	virtual const wxString GetDocumentName() const { return "SearchParam"; }
};

struct StockImageRecord: public wpObject {
    wxString stockID;
    wxString imageNo;
    wxString width;
    wxString height;
public:
    StockImageRecord();
    StockImageRecord(const StockImageRecord &);
    virtual const wxString GetDocumentName() const { return "StockImage"; }
};

struct SalesRecord : public wpObject {
    wxString id;
	int seqNo;
    wxString note;
    wxString totalAmount;
	wxString rounding;
	wxString taxValue;
    wxString discountAtTotal;
	wxString term;
	wxString refNo;
	wxString remark; // reason for deleting, etc;

    MembershipRecord customer;
	CompanyRecord promoter;
	UserRecord user;
	TerminalRecord terminal;

    wxDateTime transactionDate;
	TypeRecord salesType;
    std::vector<std::shared_ptr<StockTransactionRecord> > list;
    std::vector<std::shared_ptr<PaymentRecord> > paymentList;
public:
    SalesRecord();
    SalesRecord(const SalesRecord &);
    SalesRecord &operator=(const SalesRecord &x);

    virtual const wxString GetDocumentName() const { return "Sales"; }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	void Clear() { wpObject::Clear(); list.clear(); customer.Clear(); user.Clear(); terminal.Clear(); paymentList.clear(); promoter.Clear(); }
    virtual bool IsEmpty();
    wxString GetAmountString();
    wxString GetPaidBy();
	double GetDiscountAtTotal();
};

struct SalesRecordList : public wpObject {
    wxDateTime fromDate, toDate;
    wxString limit, offset;
	wxString salesTypeID;
    std::vector<std::shared_ptr<SalesRecord>> list;
public:
    SalesRecordList();
    SalesRecordList(const SalesRecordList &);
    SalesRecordList &operator=(const SalesRecordList &x);
    virtual const wxString GetDocumentName() const { return "SalesRecordList"; }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	void Clear() { wpObject::Clear(); list.clear(); }
    virtual bool IsEmpty();
};

struct PurchaseRecord : public wpObject {
    wxString id;
    wxString note;
    wxString refNo;
    CompanyRecord supplier;
    wxString term;
	wxString remark;
	TypeRecord purchaseType;
    wxDateTime transactionDate;
    std::vector<std::shared_ptr<StockTransactionRecord>> list;
    std::vector<std::shared_ptr<PaymentRecord>> paymentList;
public:
    PurchaseRecord();
    PurchaseRecord(const PurchaseRecord &);
    PurchaseRecord &operator=(const PurchaseRecord &x);
    virtual const wxString GetDocumentName() const { return "Purchases"; }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	virtual wxJSONValue GetJSON();
	virtual bool LoadJSON(const wxJSONValue &v);
	void Clear();
    virtual bool IsEmpty();
    wxString GetAmountString() const;
};

struct PurchaseRecordList : public wpObject {
    wxDateTime fromDate, toDate;
    wxString limit, offset;
    std::vector<std::shared_ptr<PurchaseRecord>> list;
public:
    PurchaseRecordList();
    PurchaseRecordList(const PurchaseRecordList &);
    PurchaseRecordList &operator=(const PurchaseRecordList &x);
    virtual const wxString GetDocumentName() const { return "PurchaseRecordList"; }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	void Clear() { wpObject::Clear(); list.clear(); }
    virtual bool IsEmpty();
};

struct AccountJournalRecord : public EntityRecord {
	wxString invoice;
	wxDateTime date;
	wxString invAmount;
	wxString payAmount;
	wxString remark;
public:
	AccountJournalRecord();
	AccountJournalRecord(const AccountJournalRecord &rec);
	virtual const wxString GetDocumentName() const { return "AccountingJournalRecord"; }
};

struct AccountTransactionRecord : public wpObject {
	wxString id;
	int seqNo;
	wxString note;
	wxString totalAmount;
	wxString refNo;
	wxString remark;
	wxDateTime transactionDate;
	EntityRecord entityFrom;
	EntityRecord entityTo;
	wxString type; // out or in

	std::vector<std::shared_ptr<AccountJournalRecord>> list;
public:
	AccountTransactionRecord();
	AccountTransactionRecord(const AccountTransactionRecord &);
	AccountTransactionRecord &operator=(const AccountTransactionRecord &x);

	void Import(PurchaseRecord &rec, std::function<wxString(wxString)> fnConvertPayBy);
	void Import(SalesRecord &rec, std::function<wxString(wxString)> fnConvertPayBy);
	virtual const wxString GetDocumentName() const { return "AccountTransactionRecord"; }
	virtual bool LoadXMLNode(const wxXmlNode *);
	virtual bool GetXMLNode(wxXmlNode *tg);
	void Clear() { wpObject::Clear(); list.clear(); entityFrom.Clear(); entityTo.Clear(); }
	virtual bool IsEmpty();
};

struct AccountTransactionRecordList : public wpObject {
	wxDateTime fromDate, toDate;
	wxString limit, offset;
	std::vector<std::shared_ptr<AccountTransactionRecord>> list;
public:
	AccountTransactionRecordList();
	AccountTransactionRecordList(const AccountTransactionRecordList &);
	AccountTransactionRecordList &operator=(const AccountTransactionRecordList &x);
	virtual const wxString GetDocumentName() const { return "AccountTransactionRecordList"; }
	virtual bool LoadXMLNode(const wxXmlNode *);
	virtual bool GetXMLNode(wxXmlNode *tg);
	void Clear() { wpObject::Clear(); list.clear(); }
	virtual bool IsEmpty();
};

template <class T> struct SyncRecord : public wpObject {
	wxDateTime syncDate;
	wxString id;
	RecordList<T> list;
public:
	SyncRecord() { Init(); Insert("id", &id); Insert("sync-date", &syncDate); }
	SyncRecord(const SyncRecord& r) : wpObject(r) { Init(); Insert("id", &id); ; Insert("sync-date", &syncDate); list = r.list; }
	SyncRecord& operator=(const SyncRecord& x) { wpObject::operator=(x); list = x.list; return *this; }
	virtual const wxString GetDocumentName() const { return "SyncList"; }
	void Clear() { wpObject::Clear(); list.Clear(); }
	bool LoadXMLNode(const wxXmlNode* node) {
		if (!wpObject::LoadXMLNode(node)) return false;
		for (const wxXmlNode* childNodes = node->GetChildren(); childNodes; childNodes = childNodes->GetNext()) {
			if (childNodes->GetName().IsSameAs("List", false)) {
				list.LoadXMLNode(childNodes);
			}
		}
		return true;
	}
	bool GetXMLNode(wxXmlNode* node) {
		if (!wpObject::GetXMLNode(node)) return false;
		if (!list.IsEmpty()) {
			wxXmlNode* child = new wxXmlNode(node, nodeType, "List");
			list.GetXMLNode(child);
		}
		return true;
	}
	bool IsEmpty() {
		if (!wpObject::IsEmpty()) return false;
		if (!list.IsEmpty()) return false;
		return true;
	}
	virtual wxJSONValue GetJSON() {
		wxJSONValue val = wpObject::GetJSON();
		if (!list.IsEmpty()) {
			wxJSONValue v;
			v.Append(list.GetJSON());
			val["List"] = v;
		}
		return val;
	}
	virtual bool LoadJSON(const wxJSONValue& v) {
		bool res = wpObject::LoadJSON(v);
		wxJSONValue a = v.ItemAt("List");
		if (a.GetType() == wxJSONTYPE_INVALID) return res;
		return list.LoadJSON(a);
	}
};

typedef RecordList<HistoricalPurchaseRecord> HistoricalPurchaseRecordList;
typedef RecordList<HistoricalSalesRecord> HistoricalSalesRecordList;
typedef RecordList<InventoryRecord> InventoryRecordList;
typedef RecordList<QuotationRecord> SupplierQuotationList;
typedef RecordList<HistoricalStockRecord> HistoricalStockRecordList;
typedef RecordList<StockTransactionRecord> StockRecordList;
typedef RecordList<SalesPromotionRecord> SalesPromotionRecordList;

typedef SyncRecord<StockRecord> StockRecordSync;
typedef SyncRecord<PackingRecord> StockPackRecordSync;
typedef SyncRecord<EntityRecord> EntityRecordSync;
typedef SyncRecord<TypeRecord> TypeRecordSync;
typedef SyncRecord<CompanyRecord> CompanyRecordSync;
typedef SyncRecord<MembershipRecord> MembershipRecordSync;
typedef SyncRecord<PersonRecord> PersonRecordSync;


//WX_DECLARE_HASH_MAP(wxString, StockRecordList, wxStringHash, wxStringEqual, MapOfStockRecordList);
//WX_DECLARE_HASH_MAP(long, StockRecord, wxIntegerHash, wxIntegerEqual, wxIntegerToStockRecordHashMap);

struct FastLookupKeyCodes : public wpObject {
    std::unordered_map<long, std::shared_ptr<StockRecord>> list;
public:
    FastLookupKeyCodes() {}
    FastLookupKeyCodes(const FastLookupKeyCodes &x): wpObject(x) { operator=(x);}
    FastLookupKeyCodes &operator=(const FastLookupKeyCodes &x) {wpObject::operator=(x); list=x.list; return *this;}
    virtual const wxString GetDocumentName() const { return "FastLookupKey"; }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
    virtual void Clear() {wpObject::Clear(); list.clear();}
};

struct MessageLayer {
	int layer;
	std::vector<wxString> messages;
};

struct MessageRecord : public wpObject {
    wxString id;
    wxDateTime effectiveDate; // for searching
    LexiconRecord messageType;
    wxString priority;
    wxDateTime fromDate;
    wxDateTime toDate;
    std::vector<std::shared_ptr<MessageLayer>> messages;
public:
    MessageRecord();
    MessageRecord(const MessageRecord &old);
    MessageRecord &operator=(const MessageRecord &x);
    virtual const wxString GetDocumentName() const { return "MessageRecord"; }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	void Clear();
	virtual bool IsEmpty() { if (wpObject::IsEmpty()) return messages.empty(); else return false; }
	wxString GetStat();
};

struct ServerEventMessage: public wpObject {
	struct MessageRecord { wxString key, value; };
	wxString code;
	wxString path;
	bool isBroadcast;
	bool isMobileNotification;
	std::vector<wxString> tokenList;
	std::vector<std::shared_ptr<MessageRecord>> messages;
public:
	ServerEventMessage();
	ServerEventMessage(const ServerEventMessage &);
    ServerEventMessage &operator=(const ServerEventMessage &x);
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	void AddMessage(const wxString &key, const wxString &value);
    virtual const wxString GetDocumentName() const { return "ServerEventMessage"; }
	void Clear() { wpObject::Clear(); messages.clear(); }
	virtual bool IsEmpty() { if (wpObject::IsEmpty()) return messages.empty(); else return false; }
};


struct CustHistRecord : public wpObject {
    wxString customerID;
    wxString stockID;
    wxDateTime dateOfTransaction;
    wxString qty;
    wxString packing;
    wxString price;
    wxString retprice;
public:
    CustHistRecord();
    CustHistRecord(const CustHistRecord &);
    virtual const wxString GetDocumentName() const { return "CustHist"; }
};

struct ItmHistRecord : public wpObject {
    wxString supplierID;
    wxString stockID;
    wxDateTime dateOfTransaction;
    wxDateTime dateEntered;
    wxString batchNo;
    wxDateTime expiryDate;
    wxString qty;
    wxString packing;
    wxString price;
    wxString bonus;
    wxString bonusPacking;
    wxString refNo;
public:
    ItmHistRecord();
    ItmHistRecord(const ItmHistRecord &);
    virtual const wxString GetDocumentName() const { return "ItmHist"; }
};

extern int g_appType;

struct ServerKeyRecord: public wpObject {
	std::unordered_map<wxString, int*> map;
	std::vector<StringPair> pointValueWeightList;
	bool showMALonInvoice;
	double pointsToOneRM;//=200 points to one RM
	double RMtoOnePoint;
	bool useRMOnlyForPoints; // no cents
	bool roundToNearest5cents;
	bool roundToNearest5cents_wholesales;
	bool printReceiptByDefault;
	bool isAutoComputeOtherPackPrices;
	bool isAutoComputeDefaultPrices;
	bool useLocalPrice;
	bool showCategoryInSalesSummaryPDF;
	bool showCategoryInSalesSummaryReceiptPrinter;
	wxString referenceCostForSellingPrice; // max, latest, average
	wxString defaultColumnForZeroCostInSales; // latestCost, weightedAvgCost, maxCost
	bool saveStockPackPrices; // to save price by category in StockPackPrices - meaning that this will detach the price for edited item from stockpricecategory factorAboveCost

	bool invoiceFormatPortrait;

	wxString gstMarker; // S
	wxString gstZeroMarker; // Z
	wxString gstRegNo;
	bool hasGST;
	bool whsUseCost;// use cost price in wholesales
	bool enableCreditLimitCheck;
	wxString poisonOrderNote;
	wxString quotation_Disclaimer; // print on quotation
	wxString invoice_Disclaimer; // print on invoice

	bool showDiscountOnWholesalesInvoice;
	int ageActiveInMonths;
	int idleTimeToStartDemo;
	int idleTimeToLockScreen;
	int allowEditInvoiceRecord;
	wxString applicationName;
	wxString outletRegNo;
	wxString outletCode;
	wxString outletName;
	wxString outletAddress;
	wxString applicationType;
	bool adminCanModifyQOH;
	bool qohIncludeHoldIn;
	bool qohIncludeHoldOut;
	bool movementIncludeHold;
	bool runLocalSearch;
	bool invoiceShowCustomerAllPages; // show customer name in all invoice pages;
	bool runScriptInDbLocation; // run all script root folder in the same as current DB folder
	wxString categoryListCrossTab; // list of category names separated by ';' for crosstab, those not in this list will be grouped into OTHERS

	UniversalUniqueID entityServerID;
	wxString registrationKey; // this server ID;
	UniversalUniqueID serverGroupID;

	bool IsEntityServer() { return entityServerID.IsEmpty() || entityServerID().IsSameAs(registrationKey); }
	bool doRunHereIfServerNotFound; // run command here if not found
	UniversalUniqueID serverUUID;

	int receiptPrinterPointSize;
	int receiptPrinterSpacing;
	bool showRewardPoints;
	bool isTwoLineDescOnReceipt;
	bool showTotalDiscountOnReceipt;
	bool showDiscountPerItemOnReceipt;
	bool showGSTSummary;
	int noOfLinesEject;
	bool isPriceIncludeTax;
	int sumToNearest;
	bool truncateSum;
	int percentMarginAboveCost;

	bool autoGenerateMembershipCode;
	bool autoGenerateSupplierCode;
	bool autoGenerateItemCode;
	int autoItemCodeLength;
	int autoSupplierCodeLength;
	int autoMembershipCodeLength;
	bool allowDuplicateCode;
	bool autoCodeCheckPrefix;
	bool enableCheckPoisonSales;
	bool enableCheckPseudoPoisonSales;
	wxString poisonCategoryMustControl;

	bool retainUserIDonRelogin; //1 == yes;
	int defaultSalesSmallestPack; // 1==yes;
	bool  qrBeforeGoodBye;
	wxString qrString;
	double qrImageFactor; // 1=full page, 0.5, half

	wxString
		priceCategoryGroupID, entityTypeGroupID, poisonCategoryTypeGroupID,
		caraBayarGroupID, stockTypeGroupID, stockTransactionTypeGroupID, payByRewardPoint, adjustStockTypeID,
		payByCredit, payByCheque, payByVoucher, payByMasterCard, payByVisa, payByAmex, payByCash,
		creditSalesTypeID, cashSalesTypeID, IBTTypeID,
		taxGSTTypeID, taxPercentage, defaultServiceChargePercentage,
		firstDayOfWeek, versionString, osVersion,
		accCashTypeGroupID, accBankTypeGroupID, entityTypeMember, entityTypeSupplier, entityTypePromoter, entityTypeCustomer,
		accJournalTypePurchases, accJournalTypeSales, accJournalTypeOthers, accJournalTypeExpenses;

	int enableAccounting;
	int enableHR;
    int enableTicketing;
	int enablePrintLabel, enableMyKad, enableBackup, enableDispensingLabel, enablePatientRecord;
	int enableCatering, enableOnlineStore, enableRental;
	wxDateTime softwareExpiryDate;
	wxDateTime softwareRegisterDate;

	MessageRecord printer;
	MessageRecord customerDisplay;

	wxDateTime::WeekFlags weekflag;
public:
    ServerKeyRecord();
    ServerKeyRecord(const ServerKeyRecord &);
	ServerKeyRecord &operator=(const ServerKeyRecord &);
    virtual const wxString GetDocumentName() const { return "ServerKeyRecord"; }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	void Clear();
	int GetAutoCodeLength(const wxString &entityType);
};

struct LabelInputParam : public wpObject {
	wxString label, type, param, value, startFlag, lookupOps, list, defaultValue;
	int useTimeAsItIs;
public:
	LabelInputParam();
    LabelInputParam(const LabelInputParam &p);
	virtual const wxString GetDocumentName() const { return "LabelInputParam"; }
};


struct MenuDialogParam : public wpObject {
	wxString title;
	std::vector<std::shared_ptr<LabelInputParam>> paramList;
public:
	MenuDialogParam();
    MenuDialogParam(const MenuDialogParam &p);
	MenuDialogParam &operator=(const MenuDialogParam &);
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	virtual wxJSONValue GetJSON();
	virtual bool LoadJSON(const wxJSONValue& v);
	void Clear();
	virtual bool IsEmpty() { if (wpObject::IsEmpty()) return paramList.empty(); else return false; }
	virtual const wxString GetDocumentName() const { return "MenuDialogParam"; }
};

struct MenuActionParam : public wpObject {
	wxString name, scriptName, outputType, orientation, cmdBefore, cmdAfter, userParam, sqlLink;
	wxString sheetType;
	MenuDialogParam dialog;
public:
	MenuActionParam();
    MenuActionParam(const MenuActionParam &p);
	MenuActionParam &operator=(const MenuActionParam &);
    virtual const wxString GetDocumentName() const { return "MenuAction"; }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	virtual wxJSONValue GetJSON();
	virtual bool LoadJSON(const wxJSONValue& v);
	void Clear();
	virtual bool IsEmpty() { if (wpObject::IsEmpty()) return dialog.IsEmpty(); else return false; }
	bool GetInputDateParameters(wxDateTime &fromDate, wxDateTime &toDate);
	void AddInputDateParameters(const wxDateTime &fromDate, const wxDateTime &toDate, int useTimeAsItIs);
	wxString GetParameter(const wxString &param);
	void SetParameter(const wxString &param, const wxString &val);
	void ReplaceStatement(wxString &sqlCmd);
	void SetParameters(wpSQLStatement *stt);
	void SetDefault(); // set all values to default value if the value is empty;
};

struct QueueData {
	uintptr_t conn;
	wxMessageQueue<bool> connectionReady;
	std::shared_ptr<wxMemoryBuffer> buf;
	std::shared_ptr<wxMemoryBuffer> httpMessage;
	UniversalUniqueID messageID; // 16 bytes;
	UniversalUniqueID targetID;
	char targetType;
	wxString command;
	wxString dbIdentification;
	wxString replyPrefix; // to tell the receiving command to reply using this prefix.
	bool isHTTP, isRequestHTTP, toBroadcast;
	void Copy(std::shared_ptr<wxMemoryBuffer> oBuf, std::shared_ptr<wxMemoryBuffer> &destBuf) {
		destBuf.reset(new wxMemoryBuffer);
		destBuf->AppendData(oBuf->GetData(), oBuf->GetDataLen());
	}
public:
	QueueData(uintptr_t c) : conn(c), isHTTP(false), isRequestHTTP(true), toBroadcast(false) { messageID.Build(); }
	QueueData(const QueueData& o) : conn(o.conn), buf(o.buf), httpMessage(o.httpMessage), messageID(o.messageID), targetID(o.targetID), command(o.command), dbIdentification(o.dbIdentification), replyPrefix(o.replyPrefix), isHTTP(o.isHTTP), isRequestHTTP(o.isRequestHTTP), toBroadcast(o.toBroadcast) {}
	void CopyBuffer(std::shared_ptr<wxMemoryBuffer> oBuf) { Copy(oBuf, buf); }
	void CopyHttpMessage(std::shared_ptr<wxMemoryBuffer> oBuf) { Copy(oBuf, httpMessage); }
};

struct ServerInfo : public wpObject {
	wxString outletName;
	wxString outletCode;
	wxString registrationKey;
	wxString groupID;
public:
	ServerInfo();
	ServerInfo(const ServerInfo& rec);
	virtual const wxString GetDocumentName() const { return "ServerInfo"; }
};

struct LoginSessionRecord: public wpObject {

	wxMessageQueue<std::shared_ptr<wxMemoryBuffer>> continueData;
	wxMessageQueue<std::shared_ptr<QueueData>> incoming;
	wxMessageQueue<std::shared_ptr<QueueData>> outgoing;
	std::vector<std::shared_ptr<QueueData>> bufferedIncoming;
	wxMutex bufferedIncomingMutex;

	//UniversalUniqueID waitingForID;
	int portNo;
	int isCallBack;
	int listeningPortNo;
	bool isHTTP;
	enum ConnectionFlag { Disconnected, Failed, Connected, Handshaken } status;
	uintptr_t connectionCtx;
	wxMutex isProcessing;

	wxLongLong id; // sessionID
	wxDateTime timeIn;
	wxDateTime timeOut;
	wxString ipAddress;
	wxString applicationName;
	wxString outletName;
	wxString outletCode;
	wxString sessionKey;
	wxString serverRegistrationKey;
	UniversalUniqueID serverUUID, groupID;
	wxString certificate;
	wxString systemUser;
	wxString serverName;
	wxString peerID;
	wxString serverOsVersion, serverAppVersion, clientOsVersion, clientAppVersion;
	wxDateTime expiryDate;
	UserRecord user;
	TerminalRecord terminal;
	wxString remark;
	wxString connectFromAddress;
	DatabaseRecord databaseSelected;
	std::vector<std::shared_ptr<DatabaseRecord>> databaseList;
	std::vector<std::shared_ptr<ServerInfo>> otherServers;
	int serviceNo;
	int needUpdate;
public:
	LoginSessionRecord(uintptr_t context, const wxString &key=wxEmptyString);
	LoginSessionRecord(const LoginSessionRecord &rec);
	~LoginSessionRecord();
    virtual const wxString GetDocumentName() const { return "LoginSessionRecord"; }
	LoginSessionRecord &operator=(const LoginSessionRecord &);
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	void Clear();
    virtual bool IsEmpty();
	void CreateCertificate();

	static int GetBuildNo(const wxString &s);
	static wxString GetVersion(const wxString &s);
	static double GetVersionNo(const wxString &s);
	int GetClientBuildNo() { return GetBuildNo(clientAppVersion); }
	int GetServerBuildNo() { return GetBuildNo(serverAppVersion); }
	wxString GetClientVersion() { return GetVersion(clientAppVersion); }
	wxString GetServerVersion() { return GetVersion(serverAppVersion); }
	double GetClientVersionNo() { return GetVersionNo(clientAppVersion); }
	double GetServerVersionNo() { return GetVersionNo(serverAppVersion); }
	virtual wxJSONValue GetJSON();
	virtual bool LoadJSON(const wxJSONValue& v);
};


//WX_DECLARE_HASH_MAP(long, std::shared_ptr<LoginSessionRecord>, wxIntegerHash, wxIntegerEqual, wxIntegerToLoginSessionRecordHashMap);
//WX_DECLARE_HASH_MAP(mg_connection*, std::shared_ptr<LoginSessionRecord>, wxPointerHash, wxPointerEqual, wxConnectionToLoginSessionRecordHashMap);
//WX_DECLARE_HASH_MAP(wxString, std::shared_ptr<LoginSessionRecord>, wxStringHash, wxStringEqual, wxStringToLoginSessionRecordHashMap);


struct WorkShiftRecord: public wpObject {
	wxString id;
	wxDateTime timeStart;
	wxDateTime timeEnd;
	LoginSessionRecord createdBy;
	LoginSessionRecord closeBy;
	wxString openBalance;
	wxString closingBalance;
public:
	WorkShiftRecord();
	WorkShiftRecord(const WorkShiftRecord &rec);
	WorkShiftRecord &operator=(const WorkShiftRecord &);
    virtual const wxString GetDocumentName() const { return "WorkShiftRecord"; }
    virtual bool LoadXMLNode(const wxXmlNode *);
    virtual bool GetXMLNode(wxXmlNode *tg);
	void Clear();
    virtual bool IsEmpty();
};

struct ActivateSoftwareRecord : public wpObject {
	int accounting, ticketing, catering, onlineStore, rental, stockTracking, mykad, backup, softwareExpiry, dispensing;
public:
	ActivateSoftwareRecord();
	ActivateSoftwareRecord(const ActivateSoftwareRecord &);
	virtual const wxString GetDocumentName() const { return "ActivateSoftwareRecord"; }
};

struct AuthenticationRecord: public wpObject {
    wxLongLong id;
    wxString userid, password, email, telephone, fbId, googleId;
    wxString token;
    wxString message;
public:
    AuthenticationRecord();
    AuthenticationRecord(const AuthenticationRecord &);
	virtual const wxString GetDocumentName() const { return "AuthenticationRecord"; }
};

struct RegisterSoftwareRecord : public wpObject {
	wxLongLong id;
	UniversalUniqueID uuid,groupID;
	wxString userName, password, email, telephone, outletName, address, registrationKey, cidHash, dbFolder, dbTransFolder, version;
	struct {
		wxString accounting, ticketing, catering, onlineStore, rental, stockTracking, mykad, backup, softwareExpiry, dispensing, patientRecord, hr;
		void Clear() { accounting = ticketing = catering = onlineStore = rental = stockTracking = mykad = backup = softwareExpiry = dispensing = patientRecord = hr = ""; }
	} activationKey;
	wxDateTime registrationDate, expiryDate, lastLoginDate;
	wxString ulKeys;
	wxString ulLocalKeys;
	wxString messages, messageDetail;
	wxString outletCode;
public:
	RegisterSoftwareRecord();
	RegisterSoftwareRecord(const RegisterSoftwareRecord &);
	virtual const wxString GetDocumentName() const { return "RegisterSoftwareRecord"; }
};

struct GSTCompany: public wpObject {
	//<Company>
	//	<BusinessName>ABC SDN BHD</BusinessName>
	//	<BusinessRN>654321-V</BusinessRN>
	//	<GSTNumber>IDGST:10001/2015</GSTNumber>
	//	<PeriodStart>2015-12-01</PeriodStart>
	//	<PeriodEnd>2015-12-31</PeriodEnd>
	//	<GAFCreationDate>2015-12-31</GAFCreationDate>
	//	<ProductVersion>2015-12-31</ProductVersion>
	//	<GAFVersion>2015-12-31</GAFVersion>
	//</Company>
	wxString businessName;
	wxString businessRN;
	wxString gstNo;
	wxDateTime periodStart;
	wxDateTime periodEnd;
	wxString productVersion;
	wxString GAFVersion;
public:
    virtual const wxString GetDocumentName() const { return "Company"; }
};

struct GSTPurchase: public wpObject {
    virtual const wxString GetDocumentName() const { return "Purchase"; }
	//<Purchase>
	//	<SupplierName>MEI MEI SDN BHD</SupplierName>
	//	<SupplierBRN>123456-G</SupplierBRN>
	//	<InvoiceDate>2015-12-19</InvoiceDate>
	//	<InvoiceNumber>STV/012324/8</InvoiceNumber>
	//	<ImportDeclarationNo />
	//	<LineNumber>1</LineNumber>
	//	<ProductDescription>Purchase of shark fins</ProductDescription>
	//	<PurchaseValueMYR>300</PurchaseValueMYR>
	//	<GSTValueMYR>18</GSTValueMYR>
	//	<TaxCode>TX</TaxCode>
	//	<FCYCode>XXX</FCYCode>
	//	<PurchaseFCY>0</PurchaseFCY>
	//	<GSTFCY>0</GSTFCY>
	//</Purchase>
};

class GSTSupply: public wpObject {
    virtual const wxString GetDocumentName() const { return "Supply"; }
	//<Supply>
	//	<CustomerName>PQR SDN BHD</CustomerName>
	//	<CustomerBRN>867890-B</CustomerBRN>
	//	<InvoiceDate>2015-12-21</InvoiceDate>
	//	<InvoiceNumber>2353</InvoiceNumber>
	//	<LineNumber>1</LineNumber>
	//	<ProductDescription>Rental of residential House</ProductDescription>
	//	<SupplyValueMYR>1000</SupplyValueMYR>
	//	<GSTValueMYR>0</GSTValueMYR>
	//	<TaxCode>ES</TaxCode>
	//	<Country />
	//	<FCYCode>XXX</FCYCode>
	//	<SupplyFCY>0</SupplyFCY>
	//	<GSTFCY>0</GSTFCY>
	//</Supply>
};

class GSTLedgerEntry: public wpObject {
    virtual const wxString GetDocumentName() const { return "LedgerEntry"; }
	//<LedgerEntry>
	//	<TransactionDate>2015-12-18</TransactionDate>
	//	<AccountID>10000</AccountID>
	//	<AccountName>BANK</AccountName>
	//	<TransactionDescription>Payment for fish crackers</TransactionDescription>
	//	<Name>THAI FISH CRACKERS</Name>
	//	<TransactionID />
	//	<SourceDocumentID>TTref784316</SourceDocumentID>
	//	<SourceType>AP</SourceType>
	//	<Debit>0</Debit>
	//	<Credit>1802</Credit>
	//	<Balance>8198</Balance>
	//</LedgerEntry>
};

class GSTFooter: public wpObject {
    virtual const wxString GetDocumentName() const { return "Footer"; }
	//<Footer>
	//	<TotalPurchaseCount>4</TotalPurchaseCount>
	//	<TotalPurchaseAmount>3960</TotalPurchaseAmount>
	//	<TotalPurchaseAmountGST>123.6</TotalPurchaseAmountGST>
	//	<TotalSupplyCount>5</TotalSupplyCount>
	//	<TotalSupplyAmount>8000</TotalSupplyAmount>
	//	<TotalSupplyAmountGST>120</TotalSupplyAmountGST>
	//	<TotalLedgerCount>39</TotalLedgerCount>
	//	<TotalLedgerDebit>24343.60</TotalLedgerDebit>
	//	<TotalLedgerCredit>24343.60</TotalLedgerCredit>
	//	<TotalLedgerBalance>0.00</TotalLedgerBalance>
	//</Footer>
};

class GSTAuditFile: public wpObject {
    virtual const wxString GetDocumentName() const { return "GSTAuditFile"; }
	std::vector<GSTCompany> companies; // <Companies></Companies>
	std::vector<GSTPurchase> purchases; //<Purchases></Purchases>
	std::vector<GSTSupply> supplies; //<Supplies></Supplies>
	std::vector<GSTLedgerEntry> ledgers; //<Ledger></Ledger>
};

//WX_DECLARE_HASH_MAP(long, MenuActionParam, wxIntegerHash, wxIntegerEqual, wxIntegerToMenuActionParamHashMap);

/*specifier	Replaced by	Example
%a	Abbreviated weekday name *	Thu
%A	Full weekday name *	Thursday
%b	Abbreviated month name *	Aug
%B	Full month name *	August
%c	Date and time representation *	Thu Aug 23 14:55:02 2001
%C	Year divided by 100 and truncated to integer (00-99)	20
%d	Day of the month, zero-padded (01-31)	23
%D	Short MM/DD/YY date, equivalent to %m/%d/%y	08/23/01
%e	Day of the month, space-padded ( 1-31)	23
%F	Short YYYY-MM-DD date, equivalent to %Y-%m-%d	2001-08-23
%g	Week-based year, last two digits (00-99)	01
%G	Week-based year	2001
%h	Abbreviated month name * (same as %b)	Aug
%H	Hour in 24h format (00-23)	14
%I	Hour in 12h format (01-12)	02
%j	Day of the year (001-366)	235
%m	Month as a decimal number (01-12)	08
%M	Minute (00-59)	55
%n	New-line character ('\n')
%p	AM or PM designation	PM
%r	12-hour clock time *	02:55:02 pm
%R	24-hour HH:MM time, equivalent to %H:%M	14:55
%S	Second (00-61)	02
%t	Horizontal-tab character ('\t')
%T	ISO 8601 time format (HH:MM:SS), equivalent to %H:%M:%S	14:55:02
%u	ISO 8601 weekday as number with Monday as 1 (1-7)	4
%U	Week number with the first Sunday as the first day of week one (00-53)	33
%V	ISO 8601 week number (00-53)	34
%w	Weekday as a decimal number with Sunday as 0 (0-6)	4
%W	Week number with the first Monday as the first day of week one (00-53)	34
%x	Date representation *	08/23/01
%X	Time representation *	14:55:02
%y	Year, last two digits (00-99)	01
%Y	Year	2001
%z	ISO 8601 offset from UTC in timezone (1 minute=1, 1 hour=100)
If timezone cannot be termined, no characters	+100
%Z	Timezone name or abbreviation *
*/

#define wpDATEFORMAT  "%d-%m-%Y"
#define wpDATEFORMATLONG "%a %d-%b-%Y"
#define wpDATETIMEFORMAT "%d-%m-%Y %H:%M:%S"


/*
C string that contains the text to be written to stdout.
It can optionally contain embedded format specifiers that are replaced by the values specified in subsequent additional arguments and formatted as requested.

A format specifier follows this prototype: [see compatibility note below]
%[flags][width][.precision][length]specifier

Where the specifier character at the end is the most significant component, since it defines the type and the interpretation of its corresponding argument:
specifier	Output	Example
d or i	Signed decimal integer	392
u		Unsigned decimal integer	7235
o		Unsigned octal	610
x		Unsigned hexadecimal integer	7fa
X		Unsigned hexadecimal integer (uppercase)	7FA
f		Decimal floating point, lowercase	392.65
F		Decimal floating point, uppercase	392.65
e		Scientific notation (mantissa/exponent), lowercase	3.9265e+2
E		Scientific notation (mantissa/exponent), uppercase	3.9265E+2
g		Use the shortest representation: %e or %f	392.65
G		Use the shortest representation: %E or %F	392.65
a		Hexadecimal floating point, lowercase	-0xc.90fep-2
A		Hexadecimal floating point, uppercase	-0XC.90FEP-2
c		Character	a
s		String of characters	sample
p		Pointer address	b8000000
n		Nothing printed.

The corresponding argument must be a pointer to a signed int.
The number of characters written so far is stored in the pointed location.
%		A % followed by another % character will write a single % to the stream.	%

The format specifier can also contain sub-specifiers: flags, width, .precision and modifiers (in that order), which are optional and follow these specifications:

flags	description
-		Left-justify within the given field width; Right justification is the default (see width sub-specifier).
+		Forces to preceed the result with a plus or minus sign (+ or -) even for positive numbers. By default, only negative numbers are preceded with a - sign.
(space)	If no sign is going to be written, a blank space is inserted before the value.
#		Used with o, x or X specifiers the value is preceeded with 0, 0x or 0X respectively for values different than zero.
		Used with a, A, e, E, f, F, g or G it forces the written output to contain a decimal point even if no more digits follow. By default, if no digits follow, no decimal point is written.
0		Left-pads the number with zeroes (0) instead of spaces when padding is specified (see width sub-specifier).


width		description
(number)	Minimum number of characters to be printed. If the value to be printed is shorter than this number, the result is padded with blank spaces. The value is not truncated even if the result is larger.
*			The width is not specified in the format string, but as an additional integer value argument preceding the argument that has to be formatted.

.precision	description
.number		For integer specifiers (d, i, o, u, x, X): precision specifies the minimum number of digits to be written. If the value to be written is shorter than this number, the result is padded with leading zeros. The value is not truncated even if the result is longer. A precision of 0 means that no character is written for the value 0.
			For a, A, e, E, f and F specifiers: this is the number of digits to be printed after the decimal point (by default, this is 6).
			For g and G specifiers: This is the maximum number of significant digits to be printed.
			For s: this is the maximum number of characters to be printed. By default all characters are printed until the ending null character is encountered.
			If the period is specified without an explicit value for precision, 0 is assumed.
.*			The precision is not specified in the format string, but as an additional integer value argument preceding the argument that has to be formatted.

The length sub-specifier modifies the length of the data type. This is a chart showing the types used to interpret the corresponding arguments with and without length specifier (if a different type is used, the proper type promotion or conversion is performed, if allowed):
specifiers
length	d i	u o x X	f F e E g G a A	c	s	p	n
(none)	int	unsigned int	double	int	char*	void*	int*
hh		signed char	unsigned char					signed char*
h		short int	unsigned short int					short int*
l		long int	unsigned long int		wint_t	wchar_t*		long int*
ll		long long int	unsigned long long int					long long int*
j		intmax_t	uintmax_t					intmax_t*
z		size_t	size_t					size_t*
t		ptrdiff_t	ptrdiff_t					ptrdiff_t*
L		long double

Note regarding the c specifier: it takes an int (or wint_t) as argument, but performs the proper conversion to a char value (or a wchar_t) before formatting it for output.

Note: Yellow rows indicate specifiers and sub-specifiers introduced by C99. See <cinttypes> for the specifiers for extended types.
... (additional arguments)
Depending on the format string, the function may expect a sequence of additional arguments, each containing a value to be used to replace a format specifier in the format string (or a pointer to a storage location, for n).
There should be at least as many of these arguments as the number of values specified in the format specifiers. Additional arguments are ignored by the function.

*/
