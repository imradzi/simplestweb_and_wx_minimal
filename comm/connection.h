#pragma once
#include "wx/hashset.h"
#include "wx/splash.h"
#include "wx/msgqueue.h"
#include "wx/hashmap.h"

#include <list>
#include "comm.h"

typedef std::unordered_map<wxString, std::function<wxString(const wxString&, const wxString&, bool)>> wpStringToApiCallHash;

class WebClient;

class Connection {
	friend class WebClient;
	wxMessageQueue<wxMemoryBuffer> incoming;
	WebClient webSocketClient;
	void SetComputer(wxString id, wxString ip) { sessionRecord->terminal.code=id; sessionRecord->ipAddress=ip; }
	wxString connectString;
public:
	wxString errMessage;
	bool isAuthenticated;
	std::shared_ptr<LoginSessionRecord> sessionRecord;
public:
	Connection();
	Connection(const wxString &sName, int pNo, bool enableSSL=false);
	inline void DisableSocketShutdownByPeer();
	void Init(const wxString &sName, int pNo, bool enableSSL=false);
	virtual ~Connection();

	wxString GetSelectedDBName() { return sessionRecord ? sessionRecord->databaseSelected.name : wxString(); }
	bool IsValid();
	void ResetSessionRecord(wxString serverName, int portNo);
	wxString GetTerminal() const { return sessionRecord->terminal.code; }
	wxString GetServerName() const { return sessionRecord->serverName; }
	int GetPortNo() const { return sessionRecord->portNo; }
	UserRecord &GetUserRecord() { return sessionRecord->user; }
	bool IsAdminUser() { return GetUserRecord().IsAdminUser(); }
	const wxString GetErrorMessage() const { return errMessage; }
	bool IsError() const { return !errMessage.IsEmpty(); }
	bool IsAllowed(const std::string &);
	bool InitializeSession(bool splash=true);
	bool InitializeCallBack();
	bool GetCurrentSession();
	bool ChangeDatabase();
	bool DoLogin(UserRecord &rec);
	bool ReLogin() {
		CheckAndReAstablishConnection();
		return DoLogin(GetUserRecord());
	}
	void Logout();
	bool Login(UserRecord &rec) { return DoLogin(rec); }
	//bool ChangeUser(UserRecord &rec);
	bool ValidateUser(UserRecord &rec); //const wxString &uid, const wxString &pwd);

	bool Connect(bool splash=true);
	bool ConnectForDownload();
	bool ConnectForSynchronize() { return ConnectForDownload(); }
	void CheckAndReAstablishConnection();
	void Disconnect();
	bool SetServerApplicationName(const wxString &e);
	wxImage GetImage(const wxString &imageName, wxString mimeType = "image/jpeg");
	wxIcon GetIcon(const wxString &imageName, wxString mimeType = "image/x-icon");
	size_t GetFile(const XMLTag &tg, const wxString &fileName);
	wxString GetString(const XMLTag &tg);
	wxString GetPage(const wxString &pageName);

	size_t GetFile(const wxString &api, const wxString &fileName);


	wxString SendAndGetAsString(const wxString& serverID, bool isGroup, const wxString& cmd);
	std::shared_ptr<wxMemoryBuffer> SendAndGet(const wxString& serverID, bool isGroup, const wxString& cmd);
	size_t SendAndGetFile(const wxString& serverID, bool isGroup, const wxString& cmd, const wxString& fileName);
	std::shared_ptr<wxMemoryBuffer> SendAndGetCompressed(const wxString& serverID, bool isGroup, const wxString& cmd);
	wxString SendAndGetCompressedAsString(const wxString& serverID, bool isGroup, const wxString& cmd);
	std::shared_ptr<wxMemoryBuffer> GetData(const wxString& serverID, bool isGroup, const wxString& command, http_message* hm);
	void SendCommand(const wxString& serverID, bool isGroup, const wxString& command, http_message* hm);
	std::shared_ptr<wxMemoryBuffer> SendBuffer(std::shared_ptr<wxMemoryBuffer> buf, bool waitForResult=false);
	void BroadcastGroup(const wxString& serverID, bool isGroup, const wxString& command, http_message* hm);

	std::shared_ptr<wxMemoryBuffer> SendDataCompressed(const wxString& serverID, bool isGroup, const wxString& cmd, wxMemoryBuffer data);
	wxString SendDataCompressedAsString(const wxString& serverID, bool isGroup, const wxString& cmd, wxMemoryBuffer data);


	size_t SendAndGetCompressedFile(const wxString& serverID, bool isGroup, const wxString &cmd, const wxString &filename);
	wxString Request(XMLTag &tg);
	wxString Request(XMLTag &tg, const wxString &param);
	wxString GetData(XMLTag &tg) {return Request(tg); }
	XMLTag &RequestXML(XMLTag &tg);
	bool SetLoginSession(LoginSessionRecord &r);
	bool Execute(const wxString &);
	bool Execute(XMLTag &tg);
	wxString ExecuteNoParse(XMLTag& tg);
};

class ScreenSplasher {
	wxSplashScreen *p;
public:
	wxSplashScreen* CreateSplashScreen(const wxString &text, int timeOut, wxWindow *parent=nullptr);
public:
	ScreenSplasher(const wxString &text, int timeout, wxWindow *parent=nullptr) {
		p = CreateSplashScreen(text, timeout, parent);
	}
	~ScreenSplasher() { delete p; }
};




class UpstreamQueueData {
public:
	UpstreamQueueData() : hm(nullptr), status(0), waitForResult(false) {}
public:
	wxJSONValue jsonData;
	wxString stringCommand;
	std::shared_ptr<wxMemoryBuffer> result;
	std::shared_ptr<wxMemoryBuffer> data;
	wxString targetID;
	char targetType;
	http_message* hm;
	int status;
	bool waitForResult;
};

class EntitySynchronizer;

class LiveServerConnectionBase {
public:
	virtual void InitializeFunction() = 0;
	virtual wxString GetServerName() const = 0;
	void ReadConnectionConfiguration();
	void Send(std::shared_ptr<UpstreamQueueData> v) { messageQueue.Post(v); }
public:
	LiveServerConnectionBase() = default;
	virtual wxString GetServerAddress() = 0;
	virtual long GetServerPort() = 0;
	virtual bool GetUseSSL() = 0;

	virtual bool Init(const wxString& m, const wxString& t);
	virtual void MainLoop(WSSocket::Flag &isTerminating);

	void ExecuteCommand(std::shared_ptr<UpstreamQueueData> dat);
	void StopIt() { /*commandQueue.Post(true);*/ } //TODO: - to look at this again
	std::unique_ptr<std::lock_guard<std::mutex>> WaitCompletion() { return std::make_unique<std::lock_guard<std::mutex>>(mtxRun); }
	void StartThread() {
		//std::thread runner(&LiveServerConnectionBase::MainLoop, this, commandQueue);
		//runner.detach();
	}
	bool IsServerPortValid() { return !GetServerAddress().IsEmpty() && GetServerPort() > 0; }
	virtual bool Reconnect();
	virtual void Logout();

	bool UpdateRemote(const wxString& serverID, wpObject& r, const wxString& command);
	bool ExecuteRemoteGroup(const wxString& groupID, wpObject& r, const wxString& command, bool waitForResult = true);
	bool ExecuteRemote(const wxString& targetID, bool isGroup, std::shared_ptr<wxMemoryBuffer>& result, const wxString& cmd, http_message* hm, bool waitForResult = true);
	bool SendSocketFrame(std::shared_ptr<wxMemoryBuffer>& result, websocket_message* wm, bool waitForResult);
	bool WaitForResult(std::shared_ptr<wxMemoryBuffer>& result);
public:
	EntitySynchronizer* syncherThread;
	WSSocket::Flag isTerminating;
protected:
	bool keepMoving;
	wxString entityServerID, groupID, masterDB, transDB;
	std::mutex mtxRun;
	RegisterSoftwareRecord registrationData;
	wxMessageQueue<std::shared_ptr<UpstreamQueueData>> receiveQueue;
	wxMessageQueue<std::shared_ptr<UpstreamQueueData>> messageQueue;
	std::unordered_map <wxString, std::function<bool(std::shared_ptr<UpstreamQueueData>)>> functionMap;
	wxString databaseIdentity;
	Connection connection;
};

class LiveServerConnectionThread : public LiveServerConnectionBase {
	void InitializeFunction() override;
	virtual wxString GetServerName() const { return "Live Connection Server"; }
public:
	LiveServerConnectionThread() {}
	bool Reconnect() override;
	wxString GetServerAddress() override { return gCentralServer; }
	long GetServerPort() override { return gCentralServerPort; }
	bool GetUseSSL() override { return gRunningData.useSSLforRegistration; }
	void StartSync() { Synch(Synch::all); }
	void Synch(Synch::SyncTables ty);
	wxString RunSyncData(UserDB& db, wpObject& r, Synch::SyncTables ty);
	bool SyncData(UserDB& db, StockRecordSync& r);
	bool SyncData(UserDB& db, StockPackRecordSync& r);
	bool SyncData(UserDB& db, EntityRecordSync& r);
	bool SyncData(UserDB& db, TypeRecordSync& r, Synch::SyncTables ty);
	bool SyncData(UserDB& db, CompanyRecordSync& r);
	bool SyncData(UserDB& db, PersonRecordSync& r, Synch::SyncTables ty);
	bool SyncData(UserDB& db, MembershipRecordSync& r);
};

class RegistrationServerConnectionThread : public LiveServerConnectionBase {
	void InitializeFunction() override;
	virtual wxString GetServerName() const { return "Registration Server"; }
public:
	RegistrationServerConnectionThread() : LiveServerConnectionBase() {}
	wxString GetServerAddress() override { return gRegistrationServer; }
	long GetServerPort() override { return gRegistrationServerPort; }
	bool GetUseSSL() override { return gRunningData.useSSLforRegistration; }
	void CheckRegistration() {
		std::shared_ptr<UpstreamQueueData> p(new UpstreamQueueData);
		p->jsonData["command"] = wxString("chkreg");
		Send(p);
	}
};

wxDECLARE_EVENT(wpEVT_DB_SERVER_MESSAGE, wxCommandEvent);
wxDECLARE_EVENT(wpEVT_DB_SERVER_LOGOUT, wxCommandEvent);
