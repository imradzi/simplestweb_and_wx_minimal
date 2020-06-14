#pragma once
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include "mongoose.h"


#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include ""
#ifndef MAX_PATH_SIZE
#define MAX_PATH_SIZE 512
#endif

//waiting time to 1ms;
#define WAITINGTIME 1

extern long gToCleanupSessionFile;
//extern bool gIsAppShuttingDown;

extern wxMessageQueue<wxJSONValue> webLogQueue;

extern wxString gRegistrationServer;
extern long gCentralServerPort;
extern wxString gCentralServer;
extern long gRegistrationServerPort;


extern const char broadcastChar;
extern const char continueChar;
extern const char commandChar;
extern const char replyChar;

extern std::shared_ptr<RegisterSoftwareRecord> GetRegistrationData(const wxString& masterDB, const wxString& transDB);
extern bool SetRegistrationData(const wxString& masterDB, const wxString& transDB, const wxString& res);

constexpr const char *socketFrameVersion = "Ver1";
constexpr int socketFrameVersionLength= 4;

struct wpSocketFrameHeader {
	enum TargetType { targetServer = 'c', targetGroup = 'g' };
	unsigned char sig[socketFrameVersionLength]; // "Ver1"; just to identify new version and ignore any frame with old version.
	unsigned char prefix;
	unsigned char messageID[UUID_SIZE];
	unsigned char originServer[UUID_SIZE];
	unsigned char targetType; // s=server; g=group
	unsigned char targetID[UUID_SIZE];
	uint32_t httpMessageLength;
public:
	wpSocketFrameHeader(const UniversalUniqueID& targetID, char typeOfTarget, const UniversalUniqueID& msgID) { Init(targetID, typeOfTarget, msgID); }
	static void Init(wpSocketFrameHeader* x, const UniversalUniqueID& targetID, char typeOfTarget, const UniversalUniqueID& msgID) {
		memset(x, 0, sizeof(wpSocketFrameHeader));
		memcpy(x->sig, socketFrameVersion, socketFrameVersionLength);
		memcpy(x->originServer, thisServerID.GetData(), thisServerID.Size());
		memcpy(x->targetID, targetID.GetData(), targetID.Size());
		memcpy(x->messageID, msgID.GetData(), msgID.Size());
		x->targetType = typeOfTarget; // default is server.
		x->prefix = ' ';
	}
	void Init(const UniversalUniqueID& target, char typeOfTarget, const UniversalUniqueID& msgID) { Init(this, target, typeOfTarget, msgID); }
};

class SessionPool {
	std::unordered_map<wxString, uintptr_t> pool;
	std::unordered_map<wxString, uintptr_t> byServerKey; //wxStringToLoginSessionRecordHashMap byServerKey;
	std::unordered_map<uintptr_t, std::shared_ptr<LoginSessionRecord>> poolByPtr;// wxConnectionToLoginSessionRecordHashMap poolByPtr;
	std::unordered_map<wxString, std::unordered_set<uintptr_t>> byGroupID; // should be set of string instead login session.
	wxString CreateKey() { return UniversalUniqueID::Create().GetAbbreviated(); }
	wxString GetNewKey();
	void DestroyLoginSessionRecord(const wxString &k);
	wxMutex lock;
public:
	SessionPool() {}
	virtual ~SessionPool() {}
	void Init() {}

	std::shared_ptr<LoginSessionRecord> GetLoginSessionRecord(uintptr_t p, bool toCreate); // get existing one or create new one.
	std::shared_ptr<LoginSessionRecord> GetLoginSessionRecord(uintptr_t x); // get existing one or create new one.
	std::shared_ptr<LoginSessionRecord> FindLoginSessionRecord(uintptr_t p) { return GetLoginSessionRecord(p, false); }
	std::shared_ptr<LoginSessionRecord> GetLoginSessionRecord(uintptr_t p, const wxString &key, bool toCreate); // get existing one or create new one.
	std::shared_ptr<LoginSessionRecord> CreateLoginSessionRecord(uintptr_t p, const wxString& key=wxEmptyString);
	bool IsLoginSessionExists(uintptr_t p);
	bool IsLoginSessionExists(const wxString& x);

	uintptr_t GetConnectionByServerKey(const wxString& key);
	std::unordered_set<uintptr_t> GetConnectionByGroupID(const wxString& groupID) { wxMutexLocker __l(lock);  return byGroupID[groupID]; }
	void AddLoginSessionByServerKey(std::shared_ptr<LoginSessionRecord> sess);


	void DestroyLoginSessionRecord(uintptr_t ctx);

	std::shared_ptr<LoginSessionRecord> GetLoginSessionRecord(const wxString& x);
	int GetPoolSize() { return poolByPtr.size(); }
	std::vector<wxString> GetListOfServers();
	std::vector<wxString> GetListOfGroups();
};

extern SessionPool gSessionPool;

const std::shared_ptr<LoginSessionRecord> GetLoginSessionConstPtr(uintptr_t c);
std::shared_ptr<LoginSessionRecord> FindLoginSession(uintptr_t c);
std::shared_ptr<LoginSessionRecord> GetLoginSession(uintptr_t c, bool toCreate);
std::shared_ptr<LoginSessionRecord> GetLoginSession(uintptr_t c, const wxString& key, bool toCreate);
std::shared_ptr<LoginSessionRecord> CreateLoginSession(uintptr_t c, const wxString& skey);

std::shared_ptr<LoginSessionRecord> GetLoginSessionByServerKey(const wxString& key);
void AddLoginSessionByServerKey(std::shared_ptr<LoginSessionRecord> sess);

void DestroySessionRecord(uintptr_t c);
bool IsSessionExists(uintptr_t c);
bool IsSessionExists(const wxString& x);

enum CommandLoginListOptions { ServerOnly, All, ActiveUsers };

class Connection;


class WSSocket {
public:
	class Flag {
		std::mutex mtx;
		bool v;
	public:
		Flag() { Reset(); }
		void Set() {
			std::lock_guard<std::mutex> _lock(mtx);
			v = true;
		}
		void Reset() {
			std::lock_guard<std::mutex> _lock(mtx);
			v = false;
		}
		bool operator()() { return v; }
	};
protected:
	static void ev_handler(struct mg_connection* nc, int ev, void* ev_data);
public:
	Flag isTerminating;
public:
	void ClearMemory() {
		memset(&opt, 0, sizeof(mg_serve_http_opts));
		memset(&bind_opts, 0, sizeof(mg_bind_opts));
		memset(&connect_opts, 0, sizeof(mg_connect_opts));
	}
	static bool FindAndRunAtServer(const wxString& key, std::function<void(mg_connection*)> fn); // must be run within evHandler or evPoll;
	static bool FindAndRunAtGroup(const wxString& groupID, std::function<void(mg_connection*)> fn); // must be run within evHandler or evPoll;
	static bool SendToGroup(websocket_message* wm);
	static bool SendToServer(websocket_message* wm);
	static bool SendToGroup(std::shared_ptr<QueueData> b);
public:
	void WaitCompletion() {
		isTerminating.Set();
		if (runningThread.joinable())
			runningThread.join(); 
	}
	virtual void MainLoop(WSSocket::Flag &terminateFlag);
	virtual void FinalBroadcast() {}
	void StartThread() {
		runningThread = std::thread([&]() {
			MainLoop(isTerminating);
		});
	}
public:
	virtual wxThread::ExitCode Entry() { return 0; }
	virtual bool IsConnected() const { return isConnected; }
	virtual void evConnected(mg_connection*, int) { isConnected = true; }
	virtual bool evClosed(mg_connection*) { isConnected = false; return true; }
	virtual void evHandshaken(mg_connection*) { isConnected = true; }
	virtual void evHTTPrequest(mg_connection*, http_message*) {}
	virtual void evHTTPreply(mg_connection*, http_message*) {}
	virtual void evSocketFrame(mg_connection*, const char*, int) {}
	bool WaitForReady();
	
public:
	WSSocket(mg_connection* conn);
	WSSocket(Connection* ow, const wxString& uri);
	WSSocket();
	virtual ~WSSocket();

	virtual void OnPoll(struct mg_connection *nc);
	void StopIt() { 
		messageQ.Post(true);
	}
	std::shared_ptr<QueueData> WaitForData(const UniversalUniqueID &id, uintptr_t conn);
	bool Send(std::shared_ptr<QueueData> data);
	std::shared_ptr<wxMemoryBuffer> GetData(const wxString& targetID, bool isGroup,  const wxString& command, uintptr_t conn, http_message* hm = NULL);
	//std::shared_ptr<wxMemoryBuffer> GetData(const wxString& targetID, bool isGroup, const wxString& command) { return GetData(targetID, isGroup, command, uintptr_t(client->user_data)); }
	wxString GetDataAsString(const wxString& targetID, bool isGroup, const wxString& command, uintptr_t conn, http_message *hm=NULL);
	UniversalUniqueID GetDataAsync(const wxString& targetID, bool isGroup, const wxString& command, uintptr_t conn, http_message* hm = NULL);
	void Broadcast(const wxString& targetID, const wxString& command, http_message* hm = NULL);
	std::shared_ptr<wxMemoryBuffer> SendBuffer(std::shared_ptr<wxMemoryBuffer> buf, uintptr_t conn, bool waitForResult);
	void ExecuteRemote(const wxString& targetID, bool isGroup, const wxString& command, uintptr_t conn, http_message* hm = NULL) { GetDataAsync(targetID, isGroup, command, conn, hm); }
	std::shared_ptr<wxMemoryBuffer> RetrieveAsyncData(uintptr_t conn, const UniversalUniqueID& messageID) {
		std::shared_ptr<QueueData> dat = WaitForData(messageID, conn);
		return dat ? dat->buf : nullptr;
	}
	wxString RetrieveAsyncDataAsString(uintptr_t conn, const UniversalUniqueID& id);

	std::shared_ptr<wxMemoryBuffer> GetDataUnzipped(const wxString& targetID, bool isGroup, const wxString& command, uintptr_t conn);
	//std::shared_ptr<wxMemoryBuffer> GetDataUnzipped(const wxString& targetID, bool isGroup, const wxString& command) { return GetDataUnzipped(targetID, isGroup, command, client); }
	wxString GetDataUnzippedAsString(const wxString& targetID, bool isGroup, const wxString& command, uintptr_t conn);
	size_t GetFileData(const wxString& targetID, bool isGroup, const wxString& command, const wxString& filename, uintptr_t conn);
	UniversalUniqueID SendMultiple(const wxString& targetID, bool isGroup, const wxString& command, uintptr_t conn);
	std::shared_ptr<wxMemoryBuffer> ContinueSending(const wxString& targetID, bool isGroup, const UniversalUniqueID& messageID, std::shared_ptr<wxMemoryBuffer> buf, uintptr_t conn);
//	std::shared_ptr<wxMemoryBuffer> ContinueSending(const wxString& targetID, bool isGroup, const UniversalUniqueID& messageID, std::shared_ptr<wxMemoryBuffer> buf) { return ContinueSending(targetID, isGroup, messageID, buf, client); }

	//size_t GetFileData(const wxString& targetID, bool isGroup, const wxString& command, const wxString& filename) { return GetFileData(targetID, isGroup, command, filename, client); }
	//UniversalUniqueID SendMultiple(const wxString& targetID, bool isGroup, const wxString& command) { return SendMultiple(targetID, isGroup, command, client); }

	void Broadcast(const wxString& targetID, bool isGroup, const wxString& msg, const wxString& dbPath);
	void ServeHTTP(uintptr_t conn, std::shared_ptr<wxMemoryBuffer> httpHeader);
	void ReplyHTTP(uintptr_t conn, std::shared_ptr<wxMemoryBuffer> buf);
	void Reply(uintptr_t conn, const UniversalUniqueID& id, std::shared_ptr<wxMemoryBuffer> buf);
	virtual void ExecuteCommand(wpSocketFrameHeader *frmHeader, mg_connection* nc, const wxString& cmd, std::shared_ptr<wxMemoryBuffer> httpMessage);
	virtual void CloseConnection(std::shared_ptr<LoginSessionRecord> sess);
	//virtual void ExecuteIncomingData(std::shared_ptr<LoginSessionRecord> sess, std::shared_ptr<QueueData> data) { sess->incoming.Post(data); }
	static wxString GetNumberOfConnections();
	std::vector<std::shared_ptr<LoginSessionRecord>> GetLoginSessionList(CommandLoginListOptions param);
public:
	wxString uri;
	wxString socketUrl;
	wxString portNo;
	wxString rootFolder;
	wxString extraHeader;
	wxString postData;
	mg_mgr mgr;
	mg_serve_http_opts opt;
	mg_bind_opts bind_opts;
	mg_connect_opts connect_opts;
	wxString errMessage;
	bool isShuttingDown;
	bool isCompleted;
	bool allowShutdownByPeer;
	bool useSSL;

	uintptr_t ctx;
	Connection* owner;

	wxMessageQueue<bool> isReady;

	wxMessageQueue<bool> messageQ;
	wxMessageQueue<std::shared_ptr<QueueData>> bcastOutgoing;
	bool isConnected;
	wxMessageQueue<CommandLoginListOptions> qGetLoginList; // 0 = only servers, 1=all
	wxMessageQueue<std::vector<std::shared_ptr<LoginSessionRecord>>> qReadLoginList;
protected:
	//bool stopping;
	//std::mutex mtxRun;
	//wxMutex x_condMtx;
	//wxCondition x_completed;
	std::thread runningThread;
};

class WSClient : public WSSocket {
public:
	bool isHTTPconnection;
	virtual void evConnected(mg_connection*, int);
	virtual bool evClosed(mg_connection* /*nc*/);
	virtual void evHandshaken(mg_connection* /*nc*/);
public:
	WSClient() : WSSocket(), isHTTPconnection(false) {}
	WSClient(Connection* ow, const wxString& ur, bool isHttp = false) : WSSocket(ow, ur), isHTTPconnection(isHttp) {}
	~WSClient();
	virtual wxThread::ExitCode Entry() override;
};

class WebClient : public WSClient {
//	static void ev_handler(struct mg_connection* nc, int ev, void* ev_data);
public:
	WebClient() : WSClient() {}
	~WebClient() {}
	void Init(Connection* ow, const wxString& ur, bool enableSSL, bool isHttp = false);
};


class WSServer : public WSSocket {
public:
	static wxMessageQueue<mg_connection*> loggingOut;
	virtual bool evClosed(mg_connection* nc);
public:
	WSServer() : WSSocket() {}
	WSServer(int pNo, const char* docRoot);
	//virtual void ExecuteIncomingData(std::shared_ptr<LoginSessionRecord> sess, std::shared_ptr<QueueData> data);
	static void NotifyLogout(mg_connection* c) { loggingOut.Post(c); }
	virtual wxThread::ExitCode Entry() override;
	int GetNumberOfClients();
	wxJSONValue GetOnlineUsers();
	wxJSONValue GetLiveServerConnections();
	std::vector<std::shared_ptr<ServerInfo>> GetLoggedInServers(const wxString& groupID);
	wxJSONValue ListOnlineServer();
	//void BroadcastGroup(const XMLTag& tg);
	wxJSONValue ConsolidateData(const wxString& group, const wxString& cmd, http_message* hm);
	//wxJSONValue ExecuteRemote(const wxString &serverID, const wxString &cmd, http_message* hm); // slow version, because it's querying each connection for identity
	bool ExecuteRemote(std::shared_ptr<wxMemoryBuffer> &result, const wxString& serverID, const wxString &groupID, const wxString &senderID, const wxString& cmd, http_message* hm);
	bool IsConnected() const { return isConnected; }
	void FinalBroadcast() override {
		Broadcast(thisServerID(), true, "the server is shutting down", "*");
	}
};

class WebServer : public WSServer {
//	static void ev_handler(struct mg_connection* nc, int ev, void* ev_data);
public:
	WebServer() : WSServer() {}
	~WebServer() {}
	void Init(int pNo, const char* docRoot, bool useSSL);
};


namespace WorkerThreadQueue {
	struct Queue {
		WSSocket* owner;
		uintptr_t conn; // context id for mg_connection->user_data
		std::shared_ptr<wxMemoryBuffer> buf;
		std::shared_ptr<wxMemoryBuffer> httpMessage;
		wxString command;
		wxString dbName;
		wxString replyPrefix;
		UniversalUniqueID messageID;
		UniversalUniqueID targetID;
		UniversalUniqueID originID;
		char targetType;
		bool isHTTP;
		bool isClosing;
		std::shared_ptr<LoginSessionRecord> sessionRecord;
	public:
		Queue() :owner(NULL), conn(0), command(""), isHTTP(false), isClosing(false) {}
		Queue(const Queue& cp) : owner(cp.owner), conn(cp.conn), buf(cp.buf), httpMessage(cp.httpMessage), command(cp.command.Clone()), dbName(cp.dbName.Clone()), replyPrefix(cp.replyPrefix), messageID(cp.messageID), isHTTP(cp.isHTTP), isClosing(cp.isClosing), sessionRecord(cp.sessionRecord) {}
		Queue(WSSocket* _owner, uintptr_t c, const wxString& cmd, bool isHttp = false) :owner(_owner), conn(c), command(cmd.Clone()), isHTTP(isHttp), isClosing(false) {}
		Queue(WSSocket* _owner, uintptr_t c, void* b, size_t len) :owner(_owner), conn(c), isHTTP(false), isClosing(false) { buf.reset(new wxMemoryBuffer); buf->AppendData(b, len); }
		Queue(WSSocket* _owner, uintptr_t c, http_message* hm) :owner(_owner), conn(c), command(""), isHTTP(true), isClosing(false) {
			httpMessage.reset(new wxMemoryBuffer);
			httpMessage->AppendData(hm->message.p, hm->message.len);
		}
	};
	extern wxMessageQueue <std::shared_ptr<Queue>> queue;
	inline void Execute(std::shared_ptr<Queue> b) { queue.Post(b); }
}
