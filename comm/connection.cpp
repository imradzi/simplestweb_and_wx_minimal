#ifdef _WIN32
#include "winsock2.h"
#endif

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif
#include "wx/socket.h"
#include "wx/mstream.h"
#include "wx/cmdline.h"
#include "wx/image.h"
#include "wx/aui/aui.h"
#include "wx/html/htmlwin.h"
#include "wx/mstream.h"
#include "wx/sstream.h"
#include "wx/wfstream.h"
#include "wx/zipstrm.h"
#include "wx/zstream.h"
#include "wx/tokenzr.h"
#include "wx/utils.h"
#include "wx/aui/aui.h"
#include "wx/grid.h"

#include "wx/metafile.h"
#include "wx/print.h"
#include "wx/printdlg.h"
#include "wx/sstream.h"
#include "wx/protocol/http.h"

#include <iostream>
#include "http.h"
#include "xmlParser.h"
#include "http.h"
#include "userDB.h"
#include "net.h"
#include "wpObject.h"
#include "Connection.h"
#include "GenericKedaiRuncitIDs.h"
#include "Logger.h"
#include "GenericMain.h"

extern long gSocketTimeOut;

inline void Connection::DisableSocketShutdownByPeer() { webSocketClient.allowShutdownByPeer = false; }

//extern void ShowLog(const wxString &);

wxDEFINE_EVENT(wpEVT_DB_SERVER_MESSAGE, wxCommandEvent);
wxDEFINE_EVENT(wpEVT_DB_SERVER_LOGOUT, wxCommandEvent);

bool Connection::IsValid() { return webSocketClient.IsConnected(); }

int gTimeOutServer = 1000; // 5 secs

std::shared_ptr<wxMemoryBuffer> Connection::SendAndGet(const wxString &serverID, bool isGroup, const wxString& cmd) { return webSocketClient.GetData(serverID, isGroup, cmd, webSocketClient.ctx); }
size_t Connection::SendAndGetFile(const wxString& serverID, bool isGroup, const wxString& cmd, const wxString& fileName) { return webSocketClient.GetFileData(serverID, isGroup, cmd, fileName, webSocketClient.ctx); }
std::shared_ptr<wxMemoryBuffer> Connection::SendAndGetCompressed(const wxString& serverID, bool isGroup, const wxString& cmd) { return webSocketClient.GetDataUnzipped(serverID, isGroup, cmd, webSocketClient.ctx); }
wxString Connection::SendAndGetCompressedAsString(const wxString& serverID, bool isGroup, const wxString& cmd) { return webSocketClient.GetDataUnzippedAsString(serverID, isGroup, cmd, webSocketClient.ctx); }
std::shared_ptr<wxMemoryBuffer> Connection::GetData(const wxString &targetID, bool isGroup, const wxString& command, http_message* hm) { return webSocketClient.GetData(targetID, isGroup, command, webSocketClient.ctx, hm); }
void Connection::SendCommand(const wxString& serverID, bool isGroup, const wxString& command, http_message* hm) { webSocketClient.GetDataAsync(serverID, isGroup, command, webSocketClient.ctx, hm); }
std::shared_ptr<wxMemoryBuffer> Connection::SendBuffer(std::shared_ptr<wxMemoryBuffer> buf, bool waitForResult) { return webSocketClient.SendBuffer(buf, webSocketClient.ctx, waitForResult); }
void Connection::BroadcastGroup(const wxString& serverID, bool isGroup, const wxString& command, http_message* hm) { webSocketClient.GetDataAsync(serverID, isGroup, command, webSocketClient.ctx, hm); }

wxString Connection::SendAndGetAsString(const wxString& serverID, bool isGroup, const wxString& cmd) {
	std::shared_ptr<wxMemoryBuffer> buf = SendAndGet(serverID, isGroup, cmd);
	if (buf) return wxString((char*)buf->GetData(), buf->GetDataLen());
	return "";
}

std::shared_ptr<wxMemoryBuffer> Connection::SendDataCompressed(const wxString& serverID, bool isGroup, const wxString &cmd, wxMemoryBuffer buf) {
	std::shared_ptr<wxMemoryBuffer> toSend(new wxMemoryBuffer);

	UniversalUniqueID msgID = webSocketClient.SendMultiple(serverID, isGroup, cmd, webSocketClient.ctx);
	std::shared_ptr<wxMemoryBuffer> zipped = String::ZipIt(buf.GetData(), buf.GetDataLen());
	if (!msgID.IsEmpty())
		return webSocketClient.ContinueSending(serverID, isGroup, msgID, zipped, webSocketClient.ctx);
	return nullptr;
}

wxString Connection::SendDataCompressedAsString(const wxString& serverID, bool isGroup, const wxString& cmd, wxMemoryBuffer data) {
	std::shared_ptr<wxMemoryBuffer> buf = SendDataCompressed(serverID, isGroup, cmd, data);
	if (buf) return wxString((char*)buf->GetData(), buf->GetDataLen());
	return "";
}

Connection::Connection() : sessionRecord(new LoginSessionRecord(NULL)) {
}

Connection::Connection(const wxString &sName, int pNo, bool enableSSL) : isAuthenticated(false), sessionRecord(new LoginSessionRecord(NULL)) {
	Init(sName, pNo, enableSSL);
}

//void Connection::Init(const wxString& sName, int pNo, bool enableSSL) {
//	if (!webSocketClient) return;
//	wxIPV4address addr;
//	addr.Hostname(sName);
//	wxString ipAddr = addr.IPAddress();
//	connectString = wxString::Format("%s:%d", ipAddr, pNo);
//	isAuthenticated = false;
//	webSocketClient.Init(this, connectString, enableSSL, false);
//	if (webSocketClient.IsConnected()) {
//		sessionRecord = GetLoginSession(webSocketClient.ctx, true);
//		ResetSessionRecord(sName, pNo);
//		std::thread runner(&WebClient::Poll, webSocketClient);
//		runner.detach();
//	}
//	else {
//		delete webSocketClient;
//		webSocketClient = nullptr;
//	}
//}


void Connection::Init(const wxString& sName, int pNo, bool enableSSL) {
	connectString = wxString::Format("%s:%d", sName, pNo);
	isAuthenticated = false;
	webSocketClient.Init(this, connectString, enableSSL, false);
	if (webSocketClient.IsConnected()) {
		sessionRecord = GetLoginSession(webSocketClient.ctx, true);
		ResetSessionRecord(sName, pNo);
		webSocketClient.StartThread();
	}
}

void Connection::ResetSessionRecord(wxString sName, int pNo) {
	sessionRecord->portNo = pNo;
	sessionRecord->serverName = sName;
	sessionRecord->CreateCertificate();
	sessionRecord->systemUser = wxGetUserId();
	SetComputer(GetComputerIdentity(), GetIPAddress());
}

Connection::~Connection() { Disconnect(); }

void Connection::Disconnect() {
	if (webSocketClient.IsConnected()) {
		webSocketClient.StopIt();
		webSocketClient.WaitCompletion();
	}
}

bool Connection::IsAllowed(const std::string &c) {
	return (GetUserRecord().allowedModules.find(c) != GetUserRecord().allowedModules.end());
}

bool Connection::Connect(bool splash) {
	errMessage = wxEmptyString;
	return InitializeSession(splash);
}

bool Connection::ConnectForDownload() {
	return true;
}

bool Connection::DoLogin(UserRecord &rec) {
	LoggerTracker _g("Connection::DoLogin", false);
	errMessage = wxEmptyString;
	try {
		isAuthenticated = false;
		XMLTag tg("opslogin");
		wxXmlNode *t = tg.AddChild("parameter");
		rec.ops="login";
		rec.GetXML(t);
		if (Execute(tg)) {
			rec.LoadXML(tg);
			return true;
		}
	} catch (std::exception &e) {
		errMessage = e.what();
		return false;
	}
	return false;
}

void Connection::Logout() {
	errMessage = wxEmptyString;
	try {
		XMLTag tg("logout");
		Execute(tg);
		sessionRecord->Clear();
	} catch (std::exception &e) {
		errMessage = e.what();
	}
}

bool Connection::ValidateUser(UserRecord &rec) {
	errMessage = wxEmptyString;
	try {
		isAuthenticated = false;
		XMLTag tg("opslogin");
		wxXmlNode *t = tg.AddChild("parameter");
		rec.ops = "validate";
		rec.GetXML(t);
		if (Execute(tg)) {
			rec.LoadXML(tg);
			return true;
		}
	} catch (std::exception &e) {
		errMessage = e.what();
		return false;
	}
	return false;
}


bool Connection::InitializeSession(bool toSplash) {
	errMessage = wxEmptyString;
	int timeout=0;
#ifndef _DEBUG
    timeout=2;
#endif
	if (toSplash) {
		std::shared_ptr<ScreenSplasher> splash(new ScreenSplasher(Translate("Connecting to the server"), timeout));
	}
	try {
		XMLTag tg("createsession");
		wxXmlNode *t = tg.AddChild("parameter");
		sessionRecord->ops = "init";
		sessionRecord->clientAppVersion = GetSoftwareVersion();
		sessionRecord->clientOsVersion = wxGetOsDescription();
		sessionRecord->GetXML(t);
		if (Execute(tg)) {
			sessionRecord->LoadXML(tg);
			if (!sessionRecord->errorMessage.IsEmpty()) {
				errMessage = sessionRecord->errorMessage;
				return false;
			}
			return true;
		} else {
			errMessage = tg.GetAttribute("wpErrorMessage");
		}
	} catch (std::exception &e) {
		errMessage = "InitializeSession: "+wxString(e.what());
	} catch (...) {
		errMessage = "InitializeSession error";
	}
	return false;
}

bool Connection::ChangeDatabase() {
	errMessage = wxEmptyString;
	//ScreenSplasher *splash = NULL;
	//splash = new ScreenSplasher(Translate("Changing database"), 0);
	try {
		XMLTag tg("createsession");
		wxXmlNode *t = tg.AddChild("parameter");
		sessionRecord->ops = "init";
		sessionRecord->clientAppVersion = GetSoftwareVersion();
		sessionRecord->GetXML(t);
		if (Execute(tg)) {
			sessionRecord->LoadXML(tg);
			//if (splash) delete splash;
			//splash = NULL;
			if (!sessionRecord->errorMessage.IsEmpty()) {
				errMessage = sessionRecord->errorMessage;
				return false;
			}
			return true;
		} else {
			errMessage = tg.GetAttribute("wpErrorMessage");
		}
	} catch (std::exception &e) {
		errMessage = "InitializeSession: "+wxString(e.what());
	} catch (...) {
		errMessage = "InitializeSession error";
	}
	//if (splash) delete splash;
	//splash = NULL;
	return false;
}

bool Connection::GetCurrentSession() {
	try {
		XMLTag tg("createsession");
		wxXmlNode *t = tg.AddChild("parameter");
		sessionRecord->ops = "get";
		sessionRecord->GetXML(t);
		if (Execute(tg)) {
			sessionRecord->LoadXML(tg);
			return true;
		}
	} catch (std::exception &e) {
		errMessage = "InitializeSession: "+wxString(e.what());
	}
	return false;
}


// returns plain string
wxString Connection::Request(XMLTag &tg) {
	try {
		return SendAndGetCompressedAsString(sessionRecord->peerID, false, tg.GetString());
	} catch (...) { return ""; }
}

wxString Connection::Request(XMLTag &/*tg*/, const wxString &param) {
	try {
		return SendAndGetAsString(sessionRecord->peerID, false, param);
	} catch (...) { return ""; }
}

size_t Connection::GetFile(const XMLTag &tg, const wxString &fileName) {
	errMessage = wxEmptyString;
	try {
		return SendAndGetCompressedFile(sessionRecord->peerID, false, tg.GetString(), fileName);
	} catch (std::exception &e) {
		errMessage = e.what();
	}
	return 0;
}

size_t Connection::GetFile(const wxString &api, const wxString &fileName) {
	wxHTTP get;
	get.SetHeader("Content-type", "application/zip;");
	get.SetTimeout(10); // 10 seconds of timeout instead of 10 minutes ...

	for (int i = 0; !get.Connect(sessionRecord->serverName, sessionRecord->portNo); i++) {
		if (i > 3) {
			get.Close();
			return 0;
		}
		wxSleep(5);
	}

	wxApp::IsMainLoopRunning();

	wxInputStream *httpStream = get.GetInputStream(api);

	size_t sz = 0;
	if (get.GetError() == wxPROTO_NOERR) {
		wxFileOutputStream out_stream(fileName);
		httpStream->Read(out_stream);
		sz = out_stream.GetLength();
	} else {
		wxMessageBox(Translate("Unable to connect!"));
	}
	wxDELETE(httpStream);
	get.Close();
	return sz;
}

wxString Connection::GetString(const XMLTag &tg) {
	errMessage = wxEmptyString;
	try {
		return SendAndGetCompressedAsString(sessionRecord->peerID, false, tg.GetString());
	} catch (std::exception &e) {
		errMessage = e.what();
	}
	return "";
}

XMLTag &Connection::RequestXML(XMLTag &tg) {
	try {
		tg.Parse(SendAndGetAsString(sessionRecord->peerID, false, tg.GetString()));
		return tg;
	} catch (...) { return tg; }
}

size_t Connection::SendAndGetCompressedFile(const wxString &serverID, bool isGroup, const wxString &cmd, const wxString &filename) {
	//wxBusyCursor waitCursor;
	try {
		//TimeTrackerStream trek(cmd + "SendAndGetCompressedFile", gUpdateLog);
		wxString zipFileName = filename+".z";
		size_t sz = SendAndGetFile(serverID, isGroup, cmd, zipFileName);
		//trek.Write(wxString::Format("SendAndGetFile return with %d bytes", sz));
		if (sz > 0) {
			UnCompressFile(zipFileName, filename);
			//trek.Write("uncompress to " + filename);
		}
		return sz;
	} catch (...) {
		//gUpdateLog << "\nException in SendAndGetCompressedFile " << std::endl;
		return 0;
	}
}

extern bool toDebug;

bool Connection::SetLoginSession(LoginSessionRecord &rec) {
	XMLTag tg("setloginsession");
	wxXmlNode *t = tg.AddChild("parameter");
	rec.GetXML(t);
	return Execute(tg.GetString());
}

bool Connection::Execute(XMLTag &tg) {
	try {
		wxString res = SendAndGetAsString(sessionRecord->peerID, false, tg.GetString());
		tg.Clear();
		if (tg.Parse(res))
			return tg.GetAttribute("wpReturnCode") == "ok";
	} catch (...) {
		return false;
	}
	return false;
}

wxString Connection::ExecuteNoParse(XMLTag& tg) {
	wxString res;
	try {
		res = SendAndGetAsString(sessionRecord->peerID, false, tg.GetString());
	}
	catch (...) {
	}
	return "";
}

bool Connection::Execute(const wxString &s) {
	try {
		XMLTag tg;
		tg.Parse(SendAndGetAsString(sessionRecord->peerID, false, s));
		return tg.GetAttribute("wpReturnCode") == "ok";
	} catch (...) {
		return false;
	}
}

bool Connection::SetServerApplicationName(const wxString &name) {
	XMLTag tg("setkey");
	wxXmlNode *t = tg.AddChild("parameter");
	t->AddAttribute("ApplicationName", name);
	if (Execute(tg)) {
		sessionRecord->applicationName = name;
		return true;
	}
	return false;
}

wxSplashScreen* ScreenSplasher::CreateSplashScreen(const wxString &text, int timeOut, wxWindow *parent) {
	wxMemoryDC memDC;
	int w=800, h=200;
	wxBitmap bitmap(w, h);
	memDC.SelectObject(bitmap);
	memDC.SetBackground(*wxLIGHT_GREY_BRUSH);
	memDC.Clear();
	memDC.SetPen(*wxBLACK_PEN);
	memDC.SetBrush(*wxTRANSPARENT_BRUSH);
	memDC.SetFont(wxFont(20, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_LIGHT));
	wxSize sz = memDC.GetTextExtent(text);
	memDC.DrawText(text, (w-sz.GetWidth())/2, (h-sz.GetHeight())/2);
	memDC.SelectObject(wxNullBitmap);
	wxSplashScreen *window =  new wxSplashScreen(bitmap, wxSPLASH_CENTRE_ON_SCREEN|( timeOut>0 ? wxSPLASH_TIMEOUT : wxSPLASH_NO_TIMEOUT), timeOut, parent, wxID_ANY);
	return window;
}

extern wxString gCentralServer;
extern long gCentralServerPort;

long gNumOfSecondsToRetryConnecting = 5;


bool LiveServerConnectionBase::Reconnect() {
	if (connection.IsValid()) return true;

	ShowLog("Connecting to " + GetServerName() + " [" + GetServerAddress() + ":" + String::IntToString(GetServerPort()) + "]");
	connection.Init(GetServerAddress(), GetServerPort(), GetUseSSL());
	if (connection.IsValid()) {
		ShowLog(GetServerName() + " connected [" + GetServerAddress() + ":" + String::IntToString(GetServerPort()) + "]");
	}
	return true;
}

void LiveServerConnectionBase::MainLoop(WSSocket::Flag &isTerminating) {
	std::shared_ptr<UpstreamQueueData> dat;

	wxDateTime ts = wxDateTime::UNow();
	bool x;
	while (keepMoving) {
		if (isTerminating())
			keepMoving = false;
		if (messageQueue.ReceiveTimeout(WAITINGTIME, dat) == wxMSGQUEUE_NO_ERROR) {
			ExecuteCommand(dat);
			if (!keepMoving) {
				ShowLog(GetServerName() + " function returns FALSE, dropping line");
				break;
			}
		}
		if (!connection.IsValid()) {
			if (gNumOfSecondsToRetryConnecting < 0) break; // no more checking...
			if (wxDateTime::UNow().Subtract(ts).GetSeconds() >= gNumOfSecondsToRetryConnecting) { // redo connect in 5 seconds
				keepMoving = Reconnect();
				ts = wxDateTime::UNow();
			}
			continue;
		}

	}

	if (syncherThread) syncherThread->Delete();
	syncherThread = nullptr;
	Logout();
	connection.Disconnect();
	SendFeedbackToMain(101, this); // erase centralserver connection from list;
	ShowLog(GetServerName() + " connection closed [" + GetServerAddress() + ":" + String::IntToString(GetServerPort()) + "]");
}

void LiveServerConnectionBase::Logout() {
	ShowLog("logging out from central server.");
	if (connection.IsValid()) connection.Logout();
	ShowLog("logging out from " + GetServerName() + " - DONE.");
}

bool LiveServerConnectionBase::UpdateRemote(const wxString& serverID, wpObject& r, const wxString& command) {
	XMLTag tg(command);
	wxXmlNode* t = tg.AddChild("parameter");
	r.ops = "update";
	r.serverIDtoRun = serverID;
	r.updateFromRemote = true;
	r.GetXML(t);
	std::shared_ptr<wxMemoryBuffer> result;
	bool res = ExecuteRemote(serverID, false, result, tg.GetString(), NULL);
	if (res && result) {
		wxString x((const char*)result->GetData(), result->GetDataLen());
		if (tg.Parse(x)) {
			r.LoadXML(tg);
			// r is new data after loadxml.
			r.updateFromRemote = false;
			r.toUpdateRemote = true;
			r.isUpdatedRemote = true;
			return true;
		}
	}
	r.toUpdateRemote = true;
	return false;
}

void LiveServerConnectionBase::ExecuteCommand(std::shared_ptr<UpstreamQueueData> dat) {
	if (!dat) return;
	dat->status = (connection.IsValid()) ? 0 : -1;
	try {
		auto f = functionMap[dat->jsonData["command"].AsString()];
		if (f) {
			keepMoving = f(dat);
		}
	}
	catch (wpSQLException& e) {
		ShowLog(e.GetMessage());
	}
	catch (...) {}
};

bool LiveServerConnectionBase::Init(const wxString& m, const wxString& t) {
	try {
		masterDB = m; transDB = t;
		ReadConnectionConfiguration();
		if (!IsServerPortValid()) {
			SendFeedbackToMain(101, this); // erase centralserver connection from list;
			ShowLog("Server connection closed [" + GetServerAddress() + ":" + String::IntToString(GetServerPort()) + "]");
			keepMoving = false;
			return false; // no run against server.
		}

		std::shared_ptr<UserDB> db(new UserDB(m, t));
		db->Open();
		databaseIdentity = db->GetDatabaseIdentity();
		db->GetRegistrationData(&registrationData);
		entityServerID = gRunningData.syncToServer.IsEmpty() ? db->entityServerID : gRunningData.syncToServer;
		groupID = db->groupID;
		wxString regGroupID = registrationData.groupID();
		InitializeFunction();
		syncherThread = nullptr;


		// to define lambda to catch events from server here...
		ShowLog(GetServerName() + " connection started for " + GetServerAddress() + ":" + String::IntToString(GetServerPort()));
		connection.Init(GetServerAddress(), GetServerPort(), GetUseSSL());
		if (connection.IsValid()) {
			ShowLog(wxString::Format("Registration-Server Connecting using %s to %s:%d", (GetUseSSL() ? "HTTPS" : "HTTP"), GetServerAddress(), int(GetServerPort())));
		}
		keepMoving = true;
		return true;
	}
	catch (StringException& s) {
		ShowLog(s.GetMessage());
	}
	catch (...) {
	}
	SendFeedbackToMain(101, this); // erase centralserver connection from list;
	ShowLog(GetServerName() + " connection closed [" + GetServerAddress() + ":" + String::IntToString(GetServerPort()) + "]");
	keepMoving = false;
	return false;
}

extern const char* ERROR_SERVER_NOT_FOUND;
extern int Length_ERROR_SERVER_NOT_FOUND;

wxString LiveServerConnectionThread::RunSyncData(UserDB& db, wpObject& r, Synch::SyncTables ty) {
	XMLTag tg("opssyncrecord");
	wxXmlNode* t = tg.AddChild("parameter");
	r.ops = Synch::GetTableName(ty);
	r.serverIDtoRun = entityServerID;
	r.originServer = r.senderID = db.registrationKey;
	r.GetXML(t);
	std::shared_ptr<wxMemoryBuffer> result;
	ShowLog("RunSyncData to " + entityServerID);
	bool res = ExecuteRemote(entityServerID, false, result, tg.GetString(), NULL);
	if (res) {
		if (result && result->GetDataLen() > 0) {
			if (wxStrncmp((const char*)result->GetData(), ERROR_SERVER_NOT_FOUND, Length_ERROR_SERVER_NOT_FOUND) == 0) return "";
			std::shared_ptr<wxMemoryBuffer> unzippedRes = String::UnzipIt(result);
			if (unzippedRes && unzippedRes->GetDataLen() > 0)
				return wxString((const char*)unzippedRes->GetData(), unzippedRes->GetDataLen());
		}
	}
	return "";
}

bool LiveServerConnectionThread::SyncData(UserDB& reg, StockRecordSync& r) {
	wxString resString = RunSyncData(reg, r, Synch::stock);
	if (!resString.IsEmpty()) {
		XMLTag tg;
		tg.Parse(resString);
		r.LoadXML(tg);
		return true;
	}
	return false;
}
bool LiveServerConnectionThread::SyncData(UserDB& reg, StockPackRecordSync& r) {
	wxString resString = RunSyncData(reg, r, Synch::stockPack);
	if (!resString.IsEmpty()) {
		XMLTag tg;
		tg.Parse(resString);
		r.LoadXML(tg);
		return true;
	}
	return false;
}
bool LiveServerConnectionThread::SyncData(UserDB& reg, EntityRecordSync& r) {
	wxString resString = RunSyncData(reg, r, Synch::entity);
	if (!resString.IsEmpty()) {
		XMLTag tg;
		tg.Parse(resString);
		r.LoadXML(tg);
		return true;
	}
	return false;
}
bool LiveServerConnectionThread::SyncData(UserDB& reg, TypeRecordSync& r, Synch::SyncTables ty) {
	wxString resString = RunSyncData(reg, r, ty); // syncTypes or syncUL_Keys;
	if (!resString.IsEmpty()) {
		XMLTag tg;
		tg.Parse(resString);
		r.LoadXML(tg);
		return true;
	}
	return false;
}
bool LiveServerConnectionThread::SyncData(UserDB& reg, CompanyRecordSync& r) {
	wxString resString = RunSyncData(reg, r, Synch::company);
	if (!resString.IsEmpty()) {
		XMLTag tg;
		tg.Parse(resString);
		r.LoadXML(tg);
		return true;
	}
	return false;
}
bool LiveServerConnectionThread::SyncData(UserDB& reg, PersonRecordSync& r, Synch::SyncTables ty) {
	wxString resString = RunSyncData(reg, r, ty);
	if (!resString.IsEmpty()) {
		XMLTag tg;
		tg.Parse(resString);
		r.LoadXML(tg);
		return true;
	}
	return false;
}

bool LiveServerConnectionThread::SyncData(UserDB& reg, MembershipRecordSync& r) {
	wxString resString = RunSyncData(reg, r, Synch::membership);
	if (!resString.IsEmpty()) {
		XMLTag tg;
		tg.Parse(resString);
		r.LoadXML(tg);
		return true;
	}
	return false;
}


bool LiveServerConnectionBase::ExecuteRemoteGroup(const wxString& grpID, wpObject& r, const wxString& command, bool toWaitForResult) {
	XMLTag tg(command);
	wxXmlNode* t = tg.AddChild("parameter");
	r.ops = "execute";
	r.GetXML(t);
	std::shared_ptr<wxMemoryBuffer> result;
	bool res = ExecuteRemote(grpID, true, result, tg.GetString(), NULL, toWaitForResult);
	if (res) {
		if (toWaitForResult) {
			wxString x((const char*)result->GetData(), result->GetDataLen());
			if (tg.Parse(x)) {
				if (tg.GetAttribute("wpOperation").IsSameAs("noupdate", false)) // rec.ops
					return false;
				r.LoadXML(tg);
				return true;
			}
		}
		return true;
	}
	return false;
}

bool LiveServerConnectionBase::WaitForResult(std::shared_ptr<wxMemoryBuffer>& result) {
	wxDateTime ts = wxDateTime::UNow();
	std::shared_ptr<UpstreamQueueData> dat;
	while (true) {
		if (!keepMoving) {
			ShowLog("Waiting, but receive keepMoving=false");
			return false;
		}
		if (receiveQueue.ReceiveTimeout(WAITINGTIME, dat) == wxMSGQUEUE_NO_ERROR) {
			result = dat->result;
			return dat->status == 0;
		}
		if (wxDateTime::UNow().Subtract(ts).GetSeconds() > gSocketTimeOut) {
			result.reset();
			return false;
		}
	}
	result.reset();
	return true;
}

bool LiveServerConnectionBase::SendSocketFrame(std::shared_ptr<wxMemoryBuffer>& result, websocket_message* wm, bool waitForResult) {
	std::shared_ptr<UpstreamQueueData> dat(new UpstreamQueueData);
	dat->jsonData["command"] = wxString("upstreamsocket");
	dat->data.reset(new wxMemoryBuffer);
	dat->data->AppendData(wm->data, wm->size);
	messageQueue.Post(dat);

	if (waitForResult) return WaitForResult(result);

	return true;
}

bool LiveServerConnectionBase::ExecuteRemote(const wxString& targetID, bool isGroup, std::shared_ptr<wxMemoryBuffer>& result, const wxString& cmd, http_message* hm, bool toWaitForResult) {
	std::shared_ptr<UpstreamQueueData> dat(new UpstreamQueueData);
	dat->jsonData["command"] = wxString("upstream");
	dat->stringCommand = cmd;
	dat->hm = hm;
	dat->targetID = targetID;
	dat->targetType = isGroup ? wpSocketFrameHeader::targetGroup : wpSocketFrameHeader::targetServer;
	dat->waitForResult = toWaitForResult;
	messageQueue.Post(dat);
	if (toWaitForResult) return WaitForResult(result);
	return true;
}

void LiveServerConnectionBase::ReadConnectionConfiguration() {
	wxJSONReader jr;
	wxJSONValue jv;
	if (jr.Parse(ReadStringFromFile("server.json"), &jv) == 0) {
		if (jv.HasMember("central-server")) {
			gCentralServer = jv["central-server"]["host"].AsString();
			if (gCentralServer.IsSameAs("localhost", false)) gCentralServer = "127.0.0.1";
			long p;
			if (jv["central-server"]["port"].AsLong(p))
				gCentralServerPort = p;
		}
		if (jv.HasMember("registration-server")) {
			gRegistrationServer = jv["registration-server"]["host"].AsString();
			if (gRegistrationServer.IsSameAs("localhost", false)) gRegistrationServer = "127.0.0.1";
			long p;
			if (jv["registration-server"]["port"].AsLong(p))
				gRegistrationServerPort = p;
		}
	}
}

extern EntitySynchronizer* CreateSynchronizerThread(LiveServerConnectionThread* live, const wxString& masterDB, const wxString& transDB);
extern void StartSynchronizerThreadRun(EntitySynchronizer* t, Synch::SyncTables ty);
void LiveServerConnectionThread::Synch(Synch::SyncTables ty) {
	StartSynchronizerThreadRun(syncherThread, ty);
}

extern void CopyOnlineServer(std::vector<std::shared_ptr<ServerInfo>> l);
extern void NotifyEntityServerOnline();

void LiveServerConnectionThread::InitializeFunction() {
	functionMap["upstream"] = [&](std::shared_ptr<UpstreamQueueData> param)->bool {
		std::shared_ptr<UpstreamQueueData> res(new UpstreamQueueData);

		if (param->status != 0) {
			res->status = -1;
			receiveQueue.Post(res); // stop waiting because connection not established.
			return true;
		}
		bool isGroup = param->targetType == wpSocketFrameHeader::targetGroup;
		if (param->waitForResult) {
			res->result = connection.GetData(param->targetID, isGroup, param->stringCommand, param->hm); // send command up to live-server.
			receiveQueue.Post(res);
		}
		else connection.SendCommand(param->targetID, isGroup, param->stringCommand, param->hm);
		return true; // return is set in keepMoving. - false=stop thread;
	};

	functionMap["upstreamsocket"] = [&](std::shared_ptr<UpstreamQueueData> param)->bool {
		std::shared_ptr<UpstreamQueueData> res(new UpstreamQueueData);
		if (param->status != 0) {
			res->status = -1;
			receiveQueue.Post(res); // stop waiting because connection not established.
			return true;
		}
		res->result = connection.SendBuffer(param->data, param->waitForResult); // send command up to live-server.
		if (param->waitForResult)
			receiveQueue.Post(res);
		return true; // return is set in keepMoving. - false=stop thread;
	};
}


//LiveServerConnectionThread::LiveServerConnectionThread(UserDB* db) : keepMoving(false), masterDB(db->GetDBName()), transDB(db->GetTransactionDBName()), databaseIdentity(db->GetDatabaseIdentity()), isCheckRegistration(false) {
//	db->GetRegistrationData(registrationData);
//	entityServerID = gRunningData.syncToServer.IsEmpty() ? db->entityServerID : gRunningData.syncToServer;
//	groupID = db->groupID;
//	syncherThread = nullptr;
//}

//LiveServerConnectionThread::LiveServerConnectionThread(const wxString& m, const wxString& t) : keepMoving(false), masterDB(m), transDB(t), isCheckRegistration(false) {
//	std::shared_ptr<UserDB> db(new UserDB(m, t));
//	db->Open();
//	databaseIdentity = db->GetDatabaseIdentity();
//	db->GetRegistrationData(registrationData);
//	entityServerID = gRunningData.syncToServer.IsEmpty() ? db->entityServerID : gRunningData.syncToServer;
//	groupID = db->groupID;
//	wxString regGroupID = registrationData.groupID();
//	syncherThread = nullptr;
//}

bool LiveServerConnectionThread::Reconnect() {
	if (!LiveServerConnectionBase::Reconnect()) return false;
	//if (!connection)
	//	connection = new Connection(gCentralServer, gCentralServerPort, gRunningData.useSSLforLiveConnection);
	//if (syncherThread) {
	//	syncherThread->Delete();
	//	syncherThread = nullptr;
	//}
	//if (connection && connection->IsValid()) {
	//	connection->sessionRecord->databaseSelected.dbName = gRunningData.databaseName;
	//	connection->sessionRecord->databaseSelected.transactionName = gRunningData.transName;
	//	connection->sessionRecord->terminal.code = databaseIdentity;
	//	UserRecord rec;
	//	rec.GetCode() = UserRecord::serverAppUserCode; // terminal.code is computer identity;
	//	rec.encryptedPassword = GetEncryptedString(UserRecord::serverAppUserCode + connection->sessionRecord->terminal.code);
	//	connection->sessionRecord->user = rec;
	//	connection->sessionRecord->groupID = registrationData.groupID;
	//	wxString groupIDS = registrationData.groupID();
	//	connection->sessionRecord->outletCode = registrationData.outletCode;
	//	connection->sessionRecord->outletName = registrationData.outletName;
	//	connection->sessionRecord->serverRegistrationKey = registrationData.registrationKey;
	//	connection->sessionRecord->serverUUID = registrationData.uuid;
	//	ShowLog(wxString::Format("Live-Server Connecting using %s to %s:%d", (gRunningData.useSSLforLiveConnection ? "HTTPS" : "HTTP"), gCentralServer, int(gCentralServerPort)));
	//	ShowLog(connection->sessionRecord->GetJSONString());
	//	if (connection->Connect(false)) { // must specify which database to connect to.
	//		if (connection->sessionRecord->needUpdate) {
	//			// TODO: auto update server
	//		}
	//		if (connection->Login(rec)) {
	//			gNumOfSecondsToRetryConnecting = 5; // make it 5 secs if disconnected by the server
	//			ShowLog("Login on central server - OK. - sessionKey = " + connection->sessionRecord->sessionKey + " peerID = "+connection->sessionRecord->peerID + " registrationKey="+connection->sessionRecord->serverRegistrationKey);
	//			if (mainApp) {
	//				GenericBaseApp* p = static_cast<GenericBaseApp*>(mainApp);
	//				if (p) {
	//					for (auto const &it: connection->sessionRecord->otherServers) {
	//						ShowLog(">>>Logged in [" + it->outletCode + "] " + it->outletName + ">>>" + it->registrationKey);
	//						if (it->registrationKey.IsSameAs(entityServerID)) {
	//							NotifyEntityServerOnline();
	//						}
	//					}
	//					CopyOnlineServer(connection->sessionRecord->otherServers);
	//				}
	//			}
	//			syncherThread = CreateSynchronizerThread(this, masterDB, transDB);
	//		}
	//		else {
	//			ShowLog("Login on central server failed. Error: " + connection->errMessage);
	//			connection.reset();
	//		}
	//	}
	//	else {
	//		ShowLog("cannot connect to central server. Error: " + connection->errMessage);
	//		connection.reset();
	//	}
	//}
	return true;
}

wxMutex mtxCheckReg;

void RegistrationServerConnectionThread::InitializeFunction() {
	functionMap["chkreg"] = [&](std::shared_ptr<UpstreamQueueData> dat)->bool {
		try {
			if (!dat) return true;
			// do registration...
			if (dat->status != 0) return false;
			if (mtxCheckReg.TryLock() != wxMutexError::wxMUTEX_NO_ERROR) return false; // some thread already in process of registration; just exist.
			http_message hm;
			std::shared_ptr<RegisterSoftwareRecord> rec = GetRegistrationData(masterDB, transDB);
			wxString json = rec->GetJSONString();

			// uses \n only in Format() because the format will convert to \r\n
			wxString v = wxString::Format(
				"GET /global/chkreg?v=%s&id=%s HTTP/1.1\nTransfer-Encoding:chunked\nConnection:keep-alive\nContent-Type:application/json\nKeep-Alive:timeout=300, max=1000\nContent-Length:%d\n\n",
				wpVersionNo, rec->cidHash, int(json.Length()));
			std::shared_ptr<char> str(new char[v.Length() + json.Length() + 2]);
			strcpy(str.get(), v.mbc_str());
			strcat(str.get(), json.mbc_str());
			mg_parse_http(str.get(), strlen(str.get()), &hm, 1); // cannot use wxString direct because .mbc_str() or the like create temporary string that may not have lifespan that is required by the usage of hm.
			std::shared_ptr<wxMemoryBuffer> buf = connection.GetData(nullUUID(), false, "/global/chkreg", &hm);
			wxString res;
			if (buf) res = wxString((const char*)buf->GetData(), buf->GetDataLen()); // connection->GetData("/global/chkreg", &hm);
			if (res.IsEmpty()) {
				mtxCheckReg.Unlock();
				return true; // try again
			}

			SetRegistrationData(masterDB, transDB, res);
			mtxCheckReg.Unlock();
		}
		catch (StringException& s) {
			ShowLog(s.GetMessage());
		}
		catch (...) {
		}
		return false;
	};
}