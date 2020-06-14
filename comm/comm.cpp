#ifdef _WIN32
#include "winsock2.h"
#endif
#include <signal.h>

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "wx/file.h"
#include "wx/tokenzr.h"
#include "wx/socket.h"
#include "wx/mstream.h"
#include "wx/cmdline.h"
#include "wx/pdfdoc.h"
#include "wx/pdffontmanager.h"
#include "wx/stdpaths.h"
#include "wx/wfstream.h"
#include "wx/numformatter.h"
#include "wx/intl.h"
#include "wx/cmdline.h"
#include "wx/cmdargs.h"
#include "wx/msgqueue.h"
#include "wx/stdpaths.h"
#include "wx/dir.h"

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <chrono>

// #include "http.h"
// #include "xmlParser.h"
// #include "http.h"
// #include "GenericMain.h"
// #include "GenericKedaiRuncitIDs.h"
// #include "GenericKedaiRuncitRunner.h"
// #include "Logger.h"
// #include "comm.h"
// #include "Connection.h"
// #include "runner.h"
// #include "net.h"
// #include "MACAddressUtility.h"
// #include "sqlite3.h"

// #include "http.h"
// #include "xmlParser.h"
// #include "http.h"
// #include "GenericMain.h"
// #include "GenericKedaiRuncitIDs.h"
// #include "GenericKedaiRuncitRunner.h"
// #include "Logger.h"
#include "comm.h"
#include "connection.h"
// #include "runner.h"
// #include "net.h"
// #include "MACAddressUtility.h"
// #include "sqlite3.h"

#define _SHOWLOG_ 1

extern const char* ERROR_SERVER_NOT_FOUND;
extern int Length_ERROR_SERVER_NOT_FOUND;

const char* Synch::syncTableNames[] = {
	"syncAll",
	"syncTypes",
	"syncUL_Keys",
	"syncEntity",
	"syncAccount",
	"syncPerson",
	"syncMembership",
	"syncCompany",
	"syncStock",
	"syncStockPack",
	"syncSalesRep",
	"syncPromoter",
	0
};

namespace WorkerThreadQueue {
	wxMessageQueue<std::shared_ptr<Queue>> queue;
}

constexpr int WAITING_TIME_DEFAULT = 10;
constexpr long DEFAULT_PORTNO = 32256;
long gToCleanupSessionFile = WAITING_TIME_DEFAULT;  // 10 minutes for idle to delete all session files;

//static DataStreaming gStreaming;
//wxMutex DataStreaming::_lock;

wxString gRegistrationServer = "pharmapos.com";
long gCentralServerPort = DEFAULT_PORTNO;

wxString gCentralServer = "pharmapos.com"; // https is prepended...
long gRegistrationServerPort = DEFAULT_PORTNO;
//bool gIsAppShuttingDown = false;

inline int is_websocket(const struct mg_connection* nc) {
	return nc->flags & MG_F_IS_WEBSOCKET;
}

static wxMutex mtxUserDataCounter;
static uintptr_t userDataValue = 0;

static uintptr_t GetNextUserDataSeqNo() {
	wxMutexLocker __lock(mtxUserDataCounter);
	while ((++userDataValue == 0)) { // in case of overflow.
		;
	}
	return userDataValue;
}

SessionPool gSessionPool;

std::shared_ptr<LoginSessionRecord> GetLoginSessionConstPtr(uintptr_t c) { return gSessionPool.FindLoginSessionRecord(c); }
std::shared_ptr<LoginSessionRecord> FindLoginSession(uintptr_t c) { return gSessionPool.FindLoginSessionRecord(c); }
std::shared_ptr<LoginSessionRecord> GetLoginSession(uintptr_t c, bool toCreate) { return gSessionPool.GetLoginSessionRecord(c, toCreate); }
std::shared_ptr<LoginSessionRecord> GetLoginSession(uintptr_t c, const wxString& key, bool toCreate) { return gSessionPool.GetLoginSessionRecord(c, key, toCreate); }
std::shared_ptr<LoginSessionRecord> CreateLoginSession(uintptr_t c, const wxString& skey) { return gSessionPool.CreateLoginSessionRecord(c, skey); }
uintptr_t GetConnectionByServerKey(const wxString& key) { return gSessionPool.GetConnectionByServerKey(key); }
void AddLoginSessionByServerKey(std::shared_ptr<LoginSessionRecord> sess) { gSessionPool.AddLoginSessionByServerKey(sess); }

void DestroySessionRecord(uintptr_t c) { return gSessionPool.DestroyLoginSessionRecord(c); }
bool IsSessionExists(uintptr_t c) { return gSessionPool.IsLoginSessionExists(c); }
bool IsSessionExists(const wxString& x) { return gSessionPool.IsLoginSessionExists(x); }

std::shared_ptr<LoginSessionRecord> SessionPool::GetLoginSessionRecord(const wxString& x) {
	wxMutexLocker _lock(lock);
	std::unordered_map<wxString, uintptr_t>::iterator it = pool.find(x);
	if (it == pool.end() || it->second == 0) throw StringException("login session record not found.");
	return FindLoginSessionRecord(it->second);
}

std::shared_ptr<LoginSessionRecord> SessionPool::GetLoginSessionRecord(uintptr_t x) {
	wxMutexLocker _lock(lock);
	std::unordered_map<uintptr_t, std::shared_ptr<LoginSessionRecord>>::iterator it = poolByPtr.find(x);
	if (it == poolByPtr.end()) return nullptr;
	return it->second;
}


std::shared_ptr<LoginSessionRecord> SessionPool::GetLoginSessionRecord(uintptr_t x, bool toCreate) {
	wxMutexLocker _lock(lock);
	if (x == 0) return toCreate ? CreateLoginSessionRecord(x, "") : nullptr;

	std::unordered_map<uintptr_t, std::shared_ptr<LoginSessionRecord>>::iterator it = poolByPtr.find(x);
	if (it == poolByPtr.end())
		return toCreate ? CreateLoginSessionRecord(x, ""): nullptr;
	return it->second;
}

uintptr_t SessionPool::GetConnectionByServerKey(const wxString &key) {
	wxMutexLocker _lock(lock);
	std::unordered_map<wxString, uintptr_t>::iterator it = byServerKey.find(key);
	if (it == byServerKey.end()) {
		return 0;
	}
	return it->second;
}

void SessionPool::AddLoginSessionByServerKey(std::shared_ptr<LoginSessionRecord> sess) {
	wxMutexLocker _l(lock);
	byServerKey[sess->serverRegistrationKey] = sess->connectionCtx;

	wxString gid = sess->groupID();
	byGroupID[gid].insert(sess->connectionCtx); //should insert just the stringkey of server. Instead of the pointer to sess. The pointer can change it's address.
}

std::vector<wxString> SessionPool::GetListOfServers() {
	wxMutexLocker _l(lock);
	std::vector<wxString> res;
	for (auto const& it : byServerKey) {
		res.push_back(it.first);
	}
	return res;
}

std::vector<wxString> SessionPool::GetListOfGroups() {
	wxMutexLocker _l(lock);
	std::vector<wxString> res;
	for (auto const& it : byGroupID) {
		res.push_back(it.first);
	}
	return res;
}

// get existing session and replace session key with new key;
std::shared_ptr<LoginSessionRecord> SessionPool::GetLoginSessionRecord(uintptr_t x, const wxString &key, bool toCreate) {
	wxMutexLocker _lock(lock);
	std::shared_ptr<LoginSessionRecord> sess;
	if (x > 0) {
		std::unordered_map<uintptr_t, std::shared_ptr<LoginSessionRecord>>::iterator it = poolByPtr.find(x);
		if (it == poolByPtr.end()) {
			return toCreate ? CreateLoginSessionRecord(x, key) : nullptr;
		}
		sess = it->second;
	}
	else if (toCreate) {
		sess = CreateLoginSessionRecord(x, key);
	}
	if (!key.IsEmpty()) {
		std::unordered_map<wxString, uintptr_t>::iterator its = pool.find(key);
		if (its != pool.end()) {
			pool.erase(its);
		}
		pool[key] = x;
	}
	return sess;
}

bool SessionPool::IsLoginSessionExists(const wxString& x) {
	wxMutexLocker _lock(lock);
	std::unordered_map<wxString, uintptr_t>::iterator it = pool.find(x);
	return (it != pool.end());
}

bool SessionPool::IsLoginSessionExists(uintptr_t x) {
	wxMutexLocker _lock(lock);
	std::unordered_map<uintptr_t, std::shared_ptr<LoginSessionRecord>>::iterator it = poolByPtr.find(x);
	return (it != poolByPtr.end());
}

std::shared_ptr<LoginSessionRecord> SessionPool::CreateLoginSessionRecord(uintptr_t x, const wxString &key) {

	DestroyLoginSessionRecord(key);
	DestroyLoginSessionRecord(x);
	wxMutexLocker _lock(lock);

	wxString k = key;
	if (k.IsEmpty()) {k = GetNewKey();}
	if (x ==0) {x = GetNextUserDataSeqNo();}
	std::pair<std::unordered_map<uintptr_t, std::shared_ptr<LoginSessionRecord>>::iterator, bool> res = poolByPtr.insert(std::unordered_map<uintptr_t, std::shared_ptr<LoginSessionRecord>>::value_type(x, std::shared_ptr<LoginSessionRecord>(new LoginSessionRecord(x, k))));
	// res.first = value, res.second = bool result of the insert.
	std::shared_ptr<LoginSessionRecord> &newRec = res.first->second;
	newRec->connectionCtx = x;
	pool[newRec->sessionKey] = x;
	return newRec;
}

void SessionPool::DestroyLoginSessionRecord(const wxString& x) {
	wxMutexLocker _lock(lock);
	std::unordered_map<wxString, uintptr_t>::iterator it = pool.find(x);
	if (it != pool.end()) {
		DestroyLoginSessionRecord(it->second);
	}
}

//extern void SendServerLogoutEvent(const wxString& groupID, const wxString& data);
void SessionPool::DestroyLoginSessionRecord(uintptr_t idx) {
	wxMutexLocker _lock(lock);
	std::unordered_map<uintptr_t, std::shared_ptr<LoginSessionRecord>>::iterator it = poolByPtr.find(idx);
	if (it != poolByPtr.end()) {
		wxString serverID = it->second->serverRegistrationKey;
		wxString groupID = it->second->groupID();
		wxString sessionKey = it->second->sessionKey;

		auto its = pool.find(sessionKey);
		if (its != pool.end()) pool.erase(its);

		its = byServerKey.find(serverID);
		if (its != byServerKey.end()) {
			byServerKey.erase(its);
		}

		auto its2 = byGroupID.find(groupID);

		if (its2 != byGroupID.end()) {
			std::unordered_set<uintptr_t>& s = its2->second;
			for (std::unordered_set<uintptr_t>::iterator xit = s.begin(); xit != s.end(); xit++) { // need erase it:
				if (*xit == idx) {
					s.erase(xit);
					break;
				}
			}
			if (s.empty()) byGroupID.erase(its2);
		}
		poolByPtr.erase(it);
	}
}

wxString SessionPool::GetNewKey() {
	wxString res;
	for (;;) {
		res = CreateKey();
		if (!IsLoginSessionExists(res)) {
			return res;
		}
	}
}

wxMessageQueue<mg_connection*> WSServer::loggingOut;

//void CheckConnectionPointer(mg_connection* nc) {
//	static std::unordered_set<struct mg_connection*> connectionSet;
//	static wxMutex connectionSetMutex;
//	static int nconn = 0;
//
//	connectionSetMutex.Lock();
//	if (connectionSet.find(nc) == connectionSet.end()) {
//		ShowLog(wxString::Format("*****new nc = %p*******", nc));
//		connectionSet.insert(nc);
//	} else {
//		if (nconn != connectionSet.size()) {
//			nconn = connectionSet.size();
//			ShowLog(wxString::Format("No of connection = %d", (int)connectionSet.size()));
//		}
//	}
//	connectionSetMutex.Unlock();
//}

static std::shared_ptr<LoginSessionRecord> GetLS(mg_connection* nc) {
	wxASSERT(nc);
	std::shared_ptr<LoginSessionRecord> session = FindLoginSession(uintptr_t(nc->user_data));
	return session;
}

/*
message:
	1 type 'b', '-', 'c', 'r'
	2 msgID
	3 buffer
*/

const char broadcastChar = 'b';
const char continueChar  = '-';
const char commandChar   = 'c';
const char replyChar     = 'r';


wxString WSSocket::GetNumberOfConnections() {
	int res = GetLoginSessionList(CommandLoginListOptions::All).size();
	return wxString::Format("No of connections = %d, No of sessions=%d", res, gSessionPool.GetPoolSize());
}

extern UniversalUniqueID thisServerID;
extern UniversalUniqueID thisGroupID;

// data structure:
//		wpSocketFrameHeader
//		httpMessage (0 to httpMessageLength)
//		data (actual data length)

extern bool IsInList(const unsigned char*, size_t len);

bool IsValidMessage(wpSocketFrameHeader* sfh) {
	if (sfh->prefix == replyChar || sfh->prefix == continueChar) {return true;}
	if (memcmp(sfh->originServer, thisServerID.GetData(), thisServerID.Size()) == 0) {return false;}
	if (memcmp(sfh->originServer, sfh->targetID, UUID_SIZE) == 0) {return false;}
	return true;
}

bool gAllowAllConnection = false;

bool MessageRunHere(wpSocketFrameHeader* sfh) {
	if (memcmp(sfh->originServer, UniversalUniqueID::terminalGuid, UniversalUniqueID::Size()) == 0) return true; // terminal App calling.
	if (memcmp(sfh->targetID, thisGroupID.GetData(), thisGroupID.Size()) == 0) return true; // same group - still need to send to others with same group
	if (memcmp(sfh->targetID, thisServerID.GetData(), thisServerID.Size()) == 0) return true;
	if (gAllowAllConnection) {
		if (memcmp(sfh->targetID, nullUUID.GetData(), nullUUID.Size()) == 0) return true;
		if (memcmp(sfh->targetID, sfh->originServer, UniversalUniqueID::Size()) == 0) return true; // confusion - sender send to itself
	}
	return IsInList(sfh->originServer, UniversalUniqueID::Size());
}

bool MessageRunHere(wpSocketFrameHeader* sfh, std::shared_ptr<LoginSessionRecord> sess) {
	if (sfh->targetType == wpSocketFrameHeader::targetGroup)
		return memcmp(sfh->targetID, sess->groupID.GetData(), sess->groupID.Size()) == 0;
	return UniversalUniqueID(sfh->targetID)().IsSameAs(sess->serverRegistrationKey);
}

extern void SendSocketFrameUpstream(websocket_message* wm);

bool WSSocket::FindAndRunAtServer(const wxString& key, std::function<void(mg_connection*)> fn) {
	for (mg_connection* c = mg_next(&mgr, NULL); c != NULL; c = mg_next(&mgr, c)) {
		std::shared_ptr<LoginSessionRecord> sess = FindLoginSession(uintptr_t(c->user_data));
		if (sess && sess->serverRegistrationKey.IsSameAs(key)) {
			fn(c);
			return true;
		}
	}
	return false;
}


bool WSSocket::FindAndRunAtGroup(const wxString& groupID, std::function<void(mg_connection*)> fn) {
	bool res = false;
	wxString thisID = thisServerID();
	for (mg_connection* c = mg_next(&mgr, NULL); c != NULL; c = mg_next(&mgr, c)) {
		std::shared_ptr<LoginSessionRecord> sess = FindLoginSession(uintptr_t(c->user_data));
		if (!sess) continue;
		if (sess->serverRegistrationKey.IsSameAs(thisID)) continue; // skip this server;
		if (sess->groupID().IsSameAs(groupID)) {
			fn(c);
			res = true;
		}
	}
	return res;
}

bool WSSocket::SendToServer(websocket_message* wm) {
	wpSocketFrameHeader* sfh = (wpSocketFrameHeader*)wm->data;
	wxString tgt = UniversalUniqueID(sfh->targetID)();
	return FindAndRunAtServer(tgt, [&](mg_connection *c) {mg_send_websocket_frame(c, WEBSOCKET_OP_BINARY, wm->data, wm->size); });
}

bool WSSocket::SendToGroup(websocket_message *wm) {
	wpSocketFrameHeader* sfh = (wpSocketFrameHeader*)wm->data;
	UniversalUniqueID tgtUUID(sfh->targetID);
#ifdef _SHOWLOG_
	ShowLog("Sending to group (websocket) " + tgtUUID());
#endif
	if (tgtUUID == nullUUID) return false;
	wxString tgt = tgtUUID();
	FindAndRunAtGroup(tgt, [&](mg_connection* c) {mg_send_websocket_frame(c, WEBSOCKET_OP_BINARY, wm->data, wm->size); });
	SendSocketFrameUpstream(wm);
	return true;
}

// should be called within eventHandler or evPoll;

bool WSSocket::SendToGroup(std::shared_ptr<QueueData> b) {
#ifdef _SHOWLOG_
	ShowLog("Sending to group (messageQ) " + b->targetID());
#endif
	if (b->targetID == nullUUID) return false;
	std::shared_ptr<wpSocketFrameHeader> hdr(new wpSocketFrameHeader(b->targetID, b->targetType, b->messageID));
	if (b->replyPrefix.IsEmpty() || b->replyPrefix.IsSameAs(commandChar))
		hdr->prefix = commandChar;
	else
		hdr->prefix = replyChar;
	hdr->httpMessageLength = b->httpMessage ? b->httpMessage->GetDataLen() : 0;

	wxString thisID = thisServerID();
	for (mg_connection* c = mg_next(&mgr, NULL); c != NULL; c = mg_next(&mgr, c)) {
		std::shared_ptr<LoginSessionRecord> sess = FindLoginSession(uintptr_t(c->user_data));
		if (!sess) continue;
		if (sess->serverRegistrationKey.IsSameAs(thisID)) continue; // skip this server;
		if (sess->groupID == b->targetID) {
			mg_send_websocket_frame(c, WEBSOCKET_OP_BINARY | WEBSOCKET_DONT_FIN, hdr.get(), sizeof(wpSocketFrameHeader));
			if (hdr->httpMessageLength > 0)
				mg_send_websocket_frame(c, WEBSOCKET_OP_CONTINUE | WEBSOCKET_DONT_FIN, b->httpMessage->GetData(), hdr->httpMessageLength);
			if (b->buf) mg_send_websocket_frame(c, WEBSOCKET_OP_CONTINUE, b->buf->GetData(), b->buf->GetDataLen());
			else mg_send_websocket_frame(c, WEBSOCKET_OP_CONTINUE, NULL, 0);
		}
	}
	return true;
	//SendSocketFrameUpstream(wm); --- can cause endless loop, right????
}

void WSSocket::ev_handler(struct mg_connection* nc, int ev, void* ev_data) {
	static wxCriticalSection cs;
	
	if (nc->mgr->user_data == NULL) return;

	wxCriticalSectionLocker __lockSection(cs);

	auto fnReturnError = [nc](wpSocketFrameHeader *sfh) {
		std::shared_ptr<wpSocketFrameHeader> hdr(new wpSocketFrameHeader(sfh->originServer, sfh->targetType, sfh->messageID));
		hdr->prefix = replyChar;
		hdr->httpMessageLength = 0;
		mg_send_websocket_frame(nc, WEBSOCKET_OP_BINARY | WEBSOCKET_DONT_FIN, hdr.get(), sizeof(wpSocketFrameHeader));
		mg_send_websocket_frame(nc, WEBSOCKET_OP_CONTINUE, ERROR_SERVER_NOT_FOUND, Length_ERROR_SERVER_NOT_FOUND);
	};

	switch (ev) {
		case MG_EV_ACCEPT: {
			//ShowLog("MG_EV_ACCEPT");
			break;
		}
		case MG_EV_CONNECT: {
#ifdef _SHOWLOG_
			LoggerTracker __trackker("MG_EV_CONNECT", false);
#endif
			WSSocket* ptr = static_cast<WSSocket*>(nc->mgr->user_data); if (ptr == nullptr) return;
			int flag = *(int*)ev_data;
			ptr->evConnected(nc, flag);
			std::shared_ptr<LoginSessionRecord> session = GetLoginSession(uintptr_t(nc->user_data), true); 
			nc->user_data = (void *)session->connectionCtx;
			//ShowLog(thisComputerID+" MG_EV_CONNECT:" + session->terminal.code);
			if (flag == 0) {
				session->status = LoginSessionRecord::Connected;
				mg_send_websocket_handshake2(nc, ptr->socketUrl.mbc_str(), NULL, "ppos_chat", NULL);
			}
			else {
				session->status = LoginSessionRecord::Failed;
				ptr->errMessage = strerror(flag);
			}
			break;
		}
		case MG_EV_CLOSE: {
#ifdef _SHOWLOG_
			LoggerTracker __trackker("MG_EV_CLOSE", false);
#endif
			WSSocket* ptr = static_cast<WSSocket*>(nc->mgr->user_data); if (ptr == nullptr) return;
			ptr->isShuttingDown = ptr->allowShutdownByPeer;
			if (!ptr->evClosed(nc)) {
				std::shared_ptr<LoginSessionRecord> session = GetLS(nc);
				if (session && !session->sessionKey.IsEmpty()) {
					session->status = LoginSessionRecord::Disconnected; // set to zero;
					//ptr->CloseConnection(session);
				}
				DestroySessionRecord(uintptr_t(nc->user_data)); //-- this will be done by <forcelogout>
			}
			break;
		}
		case MG_EV_POLL: {
			WSSocket* ptr = static_cast<WSSocket*>(nc->mgr->user_data); if (!ptr) return;
			ptr->OnPoll(nc);
			break;
		}
		case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST: {
			break;
		}

		case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
#ifdef _SHOWLOG_
			LoggerTracker __trackker("MG_EV_WEBSOCKET_HANDSHAKE_DONE", false);
#endif
			WSSocket* ptr = static_cast<WSSocket*>(nc->mgr->user_data); if (!ptr) return;
			ptr->evHandshaken(nc);
			std::shared_ptr<LoginSessionRecord> session = GetLoginSession(uintptr_t(nc->user_data), true); // create if not there yet
			if (nc->user_data == NULL) nc->user_data = (void*)session->connectionCtx;
			//ShowLog(thisComputerID + " MG_EV_WEBSOCKET_HANDSHAKE_DONE:" + session->terminal.code);
			session->status = LoginSessionRecord::Handshaken;
			break;
		}
		case MG_EV_HTTP_REQUEST: {
#ifdef _SHOWLOG_
			LoggerTracker __trackker("MG_EV_HTTP_REQUEST", false);
#endif
			WSSocket* ptr = static_cast<WSSocket*>(nc->mgr->user_data); if (!ptr) return;
			struct http_message* hm = (struct http_message*)ev_data;
			ptr->evHTTPrequest(nc, hm);
			std::shared_ptr<LoginSessionRecord> session = GetLoginSession(uintptr_t(nc->user_data), true); // create if not there yet
			if (nc->user_data == NULL) nc->user_data = (void*)session->connectionCtx;
			//ShowLog(thisComputerID + " MG_EV_HTTP_REQUEST:" + session->terminal.code);
			if (session->status <= LoginSessionRecord::Failed) session->status = LoginSessionRecord::Connected;
			session->isHTTP = true;
			std::shared_ptr<WorkerThreadQueue::Queue> q(new WorkerThreadQueue::Queue(ptr, uintptr_t(nc->user_data), hm)); // TODO: parse http_message header to get the targetID, originID etc.
			WorkerThreadQueue::Execute(q);
			break;
		}
		case MG_EV_HTTP_REPLY: {
#ifdef _SHOWLOG_
			LoggerTracker __trackker("MG_EV_HTTP_REPLY", false);
#endif
			WSSocket* ptr = static_cast<WSSocket*>(nc->mgr->user_data); if (!ptr) return;
			struct http_message* hm = (struct http_message*)ev_data;
			ptr->evHTTPreply(nc, hm);
			std::shared_ptr<LoginSessionRecord> session = GetLoginSession(uintptr_t(nc->user_data), false); // must have been there
			if (session) {
				//ShowLog(thisComputerID + " MG_EV_HTTP_REPLY: from " + session->terminal.code);
				session->isHTTP = true;
				if (session->status <= LoginSessionRecord::Failed) session->status = LoginSessionRecord::Connected;
				//nc->flags |= MG_F_CLOSE_IMMEDIATELY; // if HTTP to close every call.
				std::shared_ptr<QueueData> data(new QueueData(session->connectionCtx)); // TODO: parse hm to get targetID, originID, etc
				data->httpMessage.reset(new wxMemoryBuffer);
				data->httpMessage->AppendData(hm->message.p, hm->message.len);
				data->isHTTP = true;
				session->incoming.Post(data);
			}// else ShowLog(thisComputerID + " MG_EV_HTTP_REPLY: **** non existing session ****");
			ptr->isShuttingDown = ptr->allowShutdownByPeer;
			break;
		}
		case MG_EV_WEBSOCKET_FRAME: { // prefix, msgID, buffer
#ifdef _SHOWLOG_
			LoggerTracker __trackker("MG_EV_WEBSOCKET_FRAME", false);
#endif
			struct websocket_message* wm = (struct websocket_message*)ev_data;
			wpSocketFrameHeader* sfh = (wpSocketFrameHeader*)wm->data;
			if (strncmp((const char*)sfh->sig, socketFrameVersion, socketFrameVersionLength) != 0) { // wrong version
				nc->flags |= MG_F_CLOSE_IMMEDIATELY;
				break; // don't process other socket frame;
			}
			if (!IsValidMessage(sfh)) {
#ifdef _SHOWLOG_
				ShowLog("Received message-invalid, ignored. " + UniversalUniqueID(sfh->originServer)() + " -> " + UniversalUniqueID(sfh->targetID)());
#endif
				//fnReturnError(sfh);
				break; // don't process other socket frame;
			}
			if (!MessageRunHere(sfh)) {
#ifdef _SHOWLOG_
				ShowLog(UniversalUniqueID(sfh->originServer)() + " -> " + UniversalUniqueID(sfh->targetID)());
#endif
				if (sfh->targetType == wpSocketFrameHeader::targetServer) {
					WSSocket* ptr = static_cast<WSSocket*>(nc->mgr->user_data); if (!ptr) return;
					if (!ptr->SendToServer(wm)) { // cannot find server.
						fnReturnError(sfh);
					}
				}
				else if (sfh->targetType == wpSocketFrameHeader::targetGroup) {
					WSSocket* ptr = static_cast<WSSocket*>(nc->mgr->user_data);
					ptr->SendToGroup(wm);
				}
				break;
			}
#ifdef _SHOWLOG_
			ShowLog(UniversalUniqueID(sfh->originServer)() + " oo " + UniversalUniqueID(sfh->targetID)());
#endif
			// this one line causes the whole thing hanged after one broadcast.
			//if (sfh->targetType == wpSocketFrameHeader::targetGroup) {
			//	ptr->SendToGroup(wm);
			//}

			std::shared_ptr<LoginSessionRecord> session = GetLoginSession(uintptr_t(nc->user_data), true);
			if (nc->user_data == NULL) nc->user_data = (void*)session->connectionCtx;
			session->isHTTP = false;
			if (session->status <= LoginSessionRecord::Failed) session->status = LoginSessionRecord::Connected;

			std::shared_ptr<wxMemoryBuffer> httpMessage(new wxMemoryBuffer);
			const char* p = (const char*)wm->data + sizeof(wpSocketFrameHeader);
			int len = wm->size - sizeof(wpSocketFrameHeader);
			if (sfh->httpMessageLength > 0) {
				httpMessage->AppendData(p, sfh->httpMessageLength);
				p += sfh->httpMessageLength;
				len -= sfh->httpMessageLength;
			}
			switch (sfh->prefix) {
				case broadcastChar: {
					wxString mesg(p, len);
					wxCommandEvent e(wpEVT_DB_SERVER_MESSAGE);
#ifdef _SHOWLOG_
					ShowLog("Received broadcast data "+mesg);
#endif
					e.SetString(mesg);
					e.SetEventObject(NULL);
					wxQueueEvent(wxTheApp, e.Clone());
					break;
				}
				case commandChar: {
					WSSocket* ptr = static_cast<WSSocket*>(nc->mgr->user_data);
					if (ptr) {
						ptr->ExecuteCommand(sfh, nc, wxString(p, len), httpMessage);
					}
					break;
				}
				default: {
					std::shared_ptr<QueueData> data(new QueueData(session->connectionCtx));
					data->messageID.Copy(sfh->messageID);
					data->buf.reset(new wxMemoryBuffer);
					data->httpMessage = httpMessage;
					data->buf->AppendData(p, len);
					data->targetID.Copy(sfh->targetID);
					data->targetType = sfh->targetType;
					session->incoming.Post(data);
					break;
				}
			}
		}
	}
}

void WSSocket::OnPoll(mg_connection *nc) {
	auto ProcessHTTP = [&](std::shared_ptr<QueueData> b) -> void{
		if (b->buf && b->buf->GetDataLen() > 0) {
			mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection:keep-alive\r\nContent-Type:application/json\r\nKeep-Alive:timeout=300,max=1000\r\n\r\n");
			mg_send_http_chunk(nc, (const char*)b->buf->GetData(), b->buf->GetDataLen());
			mg_send_http_chunk(nc, "", 0);
		} else if (nc->mgr->user_data) {
			WSSocket* p = (WSSocket*)nc->mgr->user_data;
			http_message hm;
			if (b->httpMessage && (mg_parse_http((const char*)b->httpMessage->GetData(), b->httpMessage->GetDataLen(), &hm, b->isRequestHTTP) > 0)) { // is_req 0 = reply, 1=request
				//ShowLog(wxString(hm.method.p, hm.method.len) + " " + wxString(hm.uri.p, hm.uri.len) + " extra headers = " + (p->opt->extra_headers ? p->opt->extra_headers : "<no extra>"));
				mg_serve_http(nc, &hm, p->opt);
			}
		}
	};

	CommandLoginListOptions commandParam = CommandLoginListOptions::ServerOnly;
	if (qGetLoginList.ReceiveTimeout(WAITINGTIME, commandParam) == wxMSGQUEUE_NO_ERROR) {
		std::vector<std::shared_ptr<LoginSessionRecord>> res;
		switch (commandParam) {
		case CommandLoginListOptions::ServerOnly: // get list of servers
			for (mg_connection* c = mg_next(&mgr, NULL); c != NULL; c = mg_next(&mgr, c)) {
				std::shared_ptr<LoginSessionRecord> s = FindLoginSession(uintptr_t(c->user_data));
				if (s && s->user.IsServerAppUser())
					res.push_back(s);
			}
			break;
		case CommandLoginListOptions::All: // get all list;
			for (mg_connection* c = mg_next(&mgr, NULL); c != NULL; c = mg_next(&mgr, c)) {
				std::shared_ptr<LoginSessionRecord> s = FindLoginSession(uintptr_t(c->user_data));
				if (s) res.push_back(s);
			}
			break;
		case CommandLoginListOptions::ActiveUsers: // get number of clients:
			for (mg_connection* c = mg_next(&mgr, NULL); c != NULL; c = mg_next(&mgr, c)) {
				std::shared_ptr<LoginSessionRecord> s = FindLoginSession(uintptr_t(c->user_data));
				if (s && !s->user.id.IsEmpty() && !s->user.IsServerAppUser())
					res.push_back(s);
			}
			break;
		}
		qReadLoginList.Post(res);
	}
	auto fnBroadCast = [&](std::shared_ptr<QueueData> qData) {
		if (qData->toBroadcast) { // broadcast data;
			SendToGroup(qData); // send to other servers withing same group;
			// still continue with all other terminals connected.
			for (mg_connection* c = mg_next(&mgr, NULL); c != NULL; c = mg_next(&mgr, c)) {
				std::shared_ptr<LoginSessionRecord> session = FindLoginSession(uintptr_t(c->user_data));
				if (!session) continue;
				if (session->user.IsServerAppUser()) continue; // don't send broadcast to live-server/server-to-server comm;
				if (qData->dbIdentification.IsSameAs("*") || (session->databaseSelected.dbName.IsSameAs(qData->dbIdentification, false))) {// sending to those of the same database selected - unless the path is *
					wpSocketFrameHeader hdr(qData->targetID, qData->targetType, qData->messageID);
					hdr.prefix = broadcastChar;
					hdr.httpMessageLength = qData->httpMessage ? qData->httpMessage->GetDataLen() : 0;

					mg_send_websocket_frame(c, WEBSOCKET_OP_BINARY | WEBSOCKET_DONT_FIN, &hdr, sizeof(wpSocketFrameHeader));
					if (hdr.httpMessageLength > 0)
						mg_send_websocket_frame(c, WEBSOCKET_OP_CONTINUE | WEBSOCKET_DONT_FIN, qData->httpMessage->GetData(), hdr.httpMessageLength);

					mg_send_websocket_frame(c, WEBSOCKET_OP_CONTINUE, qData->buf->GetData(), qData->buf->GetDataLen());
				}
			}
		}
	};

	std::shared_ptr<QueueData> b;
	if (bcastOutgoing.ReceiveTimeout(WAITINGTIME, b) == wxMSGQUEUE_NO_ERROR) {
		fnBroadCast(b);
	}
	std::shared_ptr<LoginSessionRecord> session = GetLS(nc);
	if (!session) return;
	if (session->status == LoginSessionRecord::Handshaken) {
		wxMessageQueueError rc = session->outgoing.ReceiveTimeout(WAITINGTIME, b);
		if (rc == wxMSGQUEUE_NO_ERROR) {
#ifdef _SHOWLOG_
			ShowLog("Sending data to " + b->targetID());
#endif
			if (b->toBroadcast) { // broadcast data;
				fnBroadCast(b);
			} else if (b->isHTTP) { // replyHTTP
				//ShowLog(thisComputerID + " replyhttp to " + sess->terminal.code);
				ProcessHTTP(b);
			} else {
#ifdef _SHOWLOG_
				LoggerTracker(wxString::Format("mg_send_websocket_frame to %s (%ld)", b->targetID(), (long)(b->buf ? b->buf->GetDataLen() : 0)), false);
#endif
				std::shared_ptr<wpSocketFrameHeader> hdr(new wpSocketFrameHeader(b->targetID, b->targetType, b->messageID));
				if (b->replyPrefix.IsEmpty() || b->replyPrefix.IsSameAs(commandChar))
					hdr->prefix = commandChar;
				else
					hdr->prefix = replyChar;
				hdr->httpMessageLength = b->httpMessage ? b->httpMessage->GetDataLen() : 0;

#ifdef _SHOWLOG_
				ShowLog(wxString::Format("Sending header (%ld)", (long)sizeof(wpSocketFrameHeader)));
#endif

				mg_send_websocket_frame(nc, WEBSOCKET_OP_BINARY | WEBSOCKET_DONT_FIN, hdr.get(), sizeof(wpSocketFrameHeader));
				if (hdr->httpMessageLength > 0) {
#ifdef _SHOWLOG_
					ShowLog(wxString::Format("Sending http message (%ld)", (long)hdr->httpMessageLength));
#endif
					mg_send_websocket_frame(nc, WEBSOCKET_OP_CONTINUE | WEBSOCKET_DONT_FIN, b->httpMessage->GetData(), hdr->httpMessageLength);
				}
				if (b->buf) {
#ifdef _SHOWLOG_
					ShowLog(wxString::Format("Sending buf (%ld)", (long)b->buf->GetDataLen()));
#endif
					mg_send_websocket_frame(nc, WEBSOCKET_OP_CONTINUE, b->buf->GetData(), b->buf->GetDataLen());
				}
				else {
#ifdef _SHOWLOG_
					ShowLog("Sending ending buf (0)");
#endif
					mg_send_websocket_frame(nc, WEBSOCKET_OP_CONTINUE, NULL, 0);
				}
			}
		}
	}
	else if (session->isHTTP && session->status == LoginSessionRecord::Connected) { // isHTTP
		std::shared_ptr<QueueData> queData;
		wxMessageQueueError rc = session->outgoing.ReceiveTimeout(WAITINGTIME, queData);
		if (rc == wxMSGQUEUE_NO_ERROR) {
			std::shared_ptr<LoginSessionRecord> sess = GetLS(nc);
			if (!sess) return;
			ProcessHTTP(queData);
		}
	}
}

std::vector<std::shared_ptr<LoginSessionRecord>> WSSocket::GetLoginSessionList(CommandLoginListOptions param) {
	qGetLoginList.Post(param);
	std::vector<std::shared_ptr<LoginSessionRecord>> data;
	wxDateTime tStart = wxDateTime::UNow();
	while (true) {
		int rc = qReadLoginList.ReceiveTimeout(WAITINGTIME, data);
		if (rc == wxMSGQUEUE_NO_ERROR) return data;
		if (rc == wxMSGQUEUE_TIMEOUT) {
			if (wxDateTime::UNow().Subtract(tStart).GetMilliseconds() > 1000) {
				data.clear();
				break;
			}
		} else {
			data.clear();
			break;
		}
	}
	return data;
}


void WSSocket::Broadcast(const wxString &targetID, bool isGroup, const wxString& msg, const wxString& dbPath) {
	std::shared_ptr<QueueData> dat(new QueueData(NULL));
	dat->dbIdentification = dbPath;
	dat->buf = String::ToBuffer(msg);
	dat->toBroadcast = true;
	dat->targetID.Import(targetID);
	dat->targetType = isGroup ? wpSocketFrameHeader::targetGroup : wpSocketFrameHeader::targetServer;
	dat->messageID.Clear();
	Send(dat);
}

void WSSocket::ServeHTTP(uintptr_t conn, std::shared_ptr<wxMemoryBuffer> httpMessage) {
	std::shared_ptr<QueueData> dat(new QueueData(conn));
	dat->isHTTP = true;
	dat->CopyHttpMessage(httpMessage);
	dat->messageID.Clear();
	dat->isRequestHTTP = true;
	Send(dat);
}

void WSSocket::ReplyHTTP(uintptr_t conn, std::shared_ptr<wxMemoryBuffer> buf) {
	std::shared_ptr<QueueData> dat(new QueueData(conn));
	dat->CopyBuffer(buf);
	dat->isHTTP = true;
	dat->messageID.Clear();
	dat->isRequestHTTP = false;
	Send(dat);
}

WSSocket::WSSocket(mg_connection* conn) : ctx(uintptr_t(conn->user_data)), owner(nullptr), isConnected(false), isShuttingDown(false), isCompleted(false) { ClearMemory(); }

WSSocket::WSSocket(Connection* ow, const wxString& ur) : isConnected(false),
ctx(0),owner(ow),isShuttingDown(false), isCompleted(false), allowShutdownByPeer(false), uri("/xmlcommand"), socketUrl(ur), useSSL(false), extraHeader("Access-Control-Allow-Origin: *\r\nConnection:keep-alive\r\nContent-Type:application/json\r\nKeep-Alive:timeout=300,max=1000")
{
	ClearMemory();
}

WSSocket::WSSocket() : isConnected(false), ctx(0), owner(NULL), isShuttingDown(false), isCompleted(false), allowShutdownByPeer(false), useSSL(false), socketUrl("/xmlcommand"), extraHeader("Access-Control-Allow-Origin: *\r\nConnection:keep-alive\r\nContent-Type:application/json\r\nKeep-Alive:timeout=300,max=1000")
{
	ClearMemory();
}

WSSocket::~WSSocket() {}

long gSocketTimeOut = 60 * 5; // 5 minutes;

bool WSSocket::Send(std::shared_ptr<QueueData> data) {
	if (data->conn == 0) {
		bcastOutgoing.Post(data);
		return true;
	} 

	std::shared_ptr<LoginSessionRecord> sess = FindLoginSession(data->conn);
	if (!sess) return false;

	if (!data->toBroadcast) {
		if (sess->status < LoginSessionRecord::Connected) return false;
	}
	sess->outgoing.Post(data);
	return true;
}

std::shared_ptr<QueueData> WSSocket::WaitForData(const UniversalUniqueID& id, uintptr_t conn) {
	std::shared_ptr<LoginSessionRecord> sess = FindLoginSession(conn);
	std::shared_ptr<QueueData> res;
	if (!sess) return res;

	wxDateTime tStart = wxDateTime::UNow();
	while (true) {
		{
			wxMutexLocker __lock(sess->bufferedIncomingMutex);
			for (std::vector<std::shared_ptr<QueueData>>::iterator it = sess->bufferedIncoming.begin(); it != sess->bufferedIncoming.end(); it++) {
				if ((*it)->messageID == id) {
					res = *it;
					sess->bufferedIncoming.erase(it);
					//ShowLog("WaitForData: got from buffered incoming: "+id());
					return res;
				}
			}
		}

		sess = FindLoginSession(conn);
		if (!sess) break;
		wxMessageQueueError rc = sess->incoming.ReceiveTimeout(WAITINGTIME, res);
		if (isShuttingDown) return nullptr;
		if (rc == wxMSGQUEUE_NO_ERROR) {
			if (res->messageID == id) return res;
			else {
				wxMutexLocker __lock(sess->bufferedIncomingMutex);
				sess->bufferedIncoming.push_back(res); // lain di tunggu lain yang sampai dulu.
			}
		}
		else if (rc != wxMSGQUEUE_TIMEOUT) {
			ShowLog(wxString::Format("Unknown Error(%d) waiting for %s", int(rc), id()));
			return nullptr;
		}
		else {
			if (wxDateTime::UNow().Subtract(tStart).GetSeconds() > gSocketTimeOut) {
				ShowLog("Timeout waiting for reply id:"+id());
				return nullptr;
			}
		}
	}
	return nullptr;
}

std::shared_ptr<wxMemoryBuffer> WSSocket::SendBuffer(std::shared_ptr<wxMemoryBuffer> buf, uintptr_t conn, bool waitForResult) {
	if (!IsConnected()) return nullptr;
	std::shared_ptr<QueueData> dat(new QueueData(conn));
	dat->buf = buf;
	UniversalUniqueID id = dat->messageID;
	Send(dat);
	if (!waitForResult) return nullptr;
	std::shared_ptr<QueueData> res = WaitForData(id, conn);
	if (res) return res->buf;
	return nullptr;
}

UniversalUniqueID WSSocket::GetDataAsync(const wxString &targetID, bool isGroup, const wxString& command, uintptr_t conn, http_message* hm) {
	if (!IsConnected()) return UniversalUniqueID();
	wxASSERT(conn);
	std::shared_ptr<QueueData> dat(new QueueData(conn));
	dat->buf = String::ToBuffer(command);
	dat->targetID.Import(targetID);
	dat->targetType = isGroup ? wpSocketFrameHeader::targetGroup : wpSocketFrameHeader::targetServer;
	if (hm && hm->message.len > 0) {
		dat->httpMessage.reset(new wxMemoryBuffer);
		dat->httpMessage->AppendData(hm->message.p, hm->message.len);
	}
	Send(dat);
	return dat->messageID;
}

void WSSocket::Broadcast(const wxString& targetID, const wxString& command, http_message* hm) {
	std::shared_ptr<QueueData> dat(new QueueData(0));
	dat->buf = String::ToBuffer(command);
	dat->targetID.Import(targetID);
	dat->targetType = wpSocketFrameHeader::targetGroup;
	dat->toBroadcast = true;
	if (hm && hm->message.len > 0) {
		dat->httpMessage.reset(new wxMemoryBuffer);
		dat->httpMessage->AppendData(hm->message.p, hm->message.len);
	}
	Send(dat);
}

wxString WSSocket::RetrieveAsyncDataAsString(uintptr_t conn, const UniversalUniqueID& messageID) {
	std::shared_ptr<wxMemoryBuffer> dat = RetrieveAsyncData(conn, messageID);
	if (dat)
		return wxString((char*)dat->GetData(), dat->GetDataLen());
	else return "";
}

std::shared_ptr<wxMemoryBuffer> WSSocket::GetData(const wxString& targetID, bool isGroup, const wxString& command, uintptr_t conn, http_message *hm) {
	if (!IsConnected()) return nullptr;
	wxASSERT(conn);
	std::shared_ptr<QueueData> dat(new QueueData(conn));
	dat->buf = String::ToBuffer(command);
	dat->targetID.Import(targetID);
	dat->targetType = isGroup ? wpSocketFrameHeader::targetGroup : wpSocketFrameHeader::targetServer;
	if (hm && hm->message.len > 0) {
		dat->httpMessage.reset(new wxMemoryBuffer);
		dat->httpMessage->AppendData(hm->message.p, hm->message.len);
	}
	UniversalUniqueID id = dat->messageID;
	Send(dat);
	std::shared_ptr<QueueData> res = WaitForData(id, conn);
	if (res) return res->buf;

	return nullptr;
}

wxString WSSocket::GetDataAsString(const wxString& targetID, bool isGroup, const wxString& command, uintptr_t conn, http_message* hm) {
	std::shared_ptr<wxMemoryBuffer> recvData = GetData(targetID, isGroup, command, conn, hm);
	if (recvData) return wxString((char*)recvData->GetData(), recvData->GetDataLen());
	return "";
}

std::shared_ptr<wxMemoryBuffer> WSSocket::GetDataUnzipped(const wxString& targetID, bool isGroup, const wxString& command, uintptr_t conn) {
	if (!IsConnected()) return nullptr;
	wxASSERT(conn);
	std::shared_ptr<QueueData> dat(new QueueData(conn));
	dat->buf = String::ToBuffer(command);
	dat->targetID.Import(targetID);
	dat->targetType = isGroup ? wpSocketFrameHeader::targetGroup : wpSocketFrameHeader::targetServer;
	UniversalUniqueID id = dat->messageID;
	Send(dat);
	std::shared_ptr<QueueData> recvDat = WaitForData(id, conn);
	if (recvDat) {
		std::shared_ptr<wxMemoryBuffer> unzipped = String::UnzipIt(recvDat->buf);
		wxLogMessage("unzipped string length=" + String::IntToString(unzipped->GetDataLen()));
		return unzipped;
	} else return nullptr;


	//	return wxString((const char*)unzipped->GetData(), unzipped->GetDataLen());
	//}
	//else return "";
}

wxString WSSocket::GetDataUnzippedAsString(const wxString& targetID, bool isGroup, const wxString& command, uintptr_t conn) {
	std::shared_ptr<wxMemoryBuffer> unzipped = GetDataUnzipped(targetID, isGroup, command, conn);
	if (unzipped) return wxString((const char*)unzipped->GetData(), unzipped->GetDataLen());
	return "";
}

size_t WSSocket::GetFileData(const wxString& targetID, bool isGroup, const wxString& command, const wxString &fileName, uintptr_t conn) {
	if (!IsConnected()) return 0;
	wxASSERT(conn);
	std::shared_ptr<QueueData> dat(new QueueData(conn));
	dat->buf = String::ToBuffer(command);
	dat->targetID.Import(targetID);
	dat->targetType = isGroup ? wpSocketFrameHeader::targetGroup : wpSocketFrameHeader::targetServer;
	UniversalUniqueID id = dat->messageID;
	Send(dat);
	std::shared_ptr<QueueData> recvDat = WaitForData(id, conn);
	if (recvDat) {
		if (recvDat->buf->GetDataLen() == 6) {
			wxString str((const char *)recvDat->buf->GetData(), recvDat->buf->GetDataLen());
			if (str.IsSameAs("NoData", true)) return 0;
		}
		wxFile f(fileName, wxFile::write);
		f.Write(recvDat->buf->GetData(), recvDat->buf->GetDataLen());
		return f.Length();
	}
	else return 0;
}

UniversalUniqueID WSSocket::SendMultiple(const wxString& targetID, bool isGroup, const wxString& command, uintptr_t conn) {
	if (!IsConnected()) return UniversalUniqueID();
	wxASSERT(conn);
	std::shared_ptr<QueueData> dat(new QueueData(conn));
	dat->buf = String::ToBuffer(command);
	dat->targetID.Import(targetID);
	dat->targetType = isGroup ? wpSocketFrameHeader::targetGroup : wpSocketFrameHeader::targetServer;
	UniversalUniqueID id = dat->messageID;
	Send(dat);
	std::shared_ptr<QueueData> recvDat = WaitForData(id, conn);
	if (recvDat) {
		wxString ret((char*)recvDat->buf->GetData(), recvDat->buf->GetDataLen());
		if (ret.IsSameAs("ok", false)) return id;
	}
	id.Clear();
	return id;
}

std::shared_ptr<wxMemoryBuffer> WSSocket::ContinueSending(const wxString& targetID, bool isGroup, const UniversalUniqueID& messageID, std::shared_ptr<wxMemoryBuffer> buf, uintptr_t conn) {
	if (!IsConnected()) return nullptr;
	wxASSERT(conn);
	std::shared_ptr<QueueData> dat(new QueueData(conn));
	dat->messageID = messageID;
	dat->CopyBuffer(buf);
	dat->targetID.Import(targetID);
	dat->targetType = isGroup ? wpSocketFrameHeader::targetGroup : wpSocketFrameHeader::targetServer;
	Send(dat);
	std::shared_ptr<QueueData> res = WaitForData(messageID, conn);
	if (res) return res->buf;

	return nullptr;
}

void WSSocket::CloseConnection(std::shared_ptr<LoginSessionRecord> sess) {
	if (sess && !sess->user.IsEmpty()) {
		std::shared_ptr<WorkerThreadQueue::Queue> q(new WorkerThreadQueue::Queue);
		q->isClosing = true;
		q->sessionRecord = sess;
		WorkerThreadQueue::Execute(q);
	}
}

void WSSocket::ExecuteCommand(wpSocketFrameHeader *sfh, mg_connection* nc, const wxString& cmd, std::shared_ptr<wxMemoryBuffer> httpMessage) {
	std::shared_ptr<WorkerThreadQueue::Queue> q(new WorkerThreadQueue::Queue(this, uintptr_t(nc->user_data), cmd));
	q->replyPrefix = replyChar;
	q->messageID.Copy(sfh->messageID);
	q->targetID.Copy(sfh->targetID);
	q->originID.Copy(sfh->originServer);
	q->targetType = sfh->targetType;
	q->httpMessage = httpMessage;
	WorkerThreadQueue::Execute(q);
}

// this is going to be put in the thread...
void WSSocket::MainLoop(WSSocket::Flag &isTerminating) {
	if (!isConnected) return;

//	std::lock_guard<std::mutex> _guard(mtxRun);
	bool stopping = false;
	for (; !stopping;) {
		try {
			if (isTerminating()) {
				stopping = true;
				FinalBroadcast(); // let final mgr_pool to run.
			}
			mg_mgr_poll(&mgr, WAITINGTIME);
		}
		catch (wpSQLException& e) {
			ShowLog("WebServer: sqlite3 exception: " + e.GetMessage());
		}
		catch (StringException& e) {
			ShowLog("WebServer: stringexception: " + e.GetMessage());
		}
		catch (...) {
			ShowLog("WebServer: unknown exception");
		}
	}
	mg_mgr_free(&mgr);
	isCompleted = true;
	ShowLog("MainLoop: completed");
}
WSClient::~WSClient() {}
wxThread::ExitCode WSClient::Entry() {
//	isConnected = true;
//	mg_connection* client = NULL;
//
//	mg_mgr t_mgr;
//	mg_serve_http_opts t_opt;
//	mg_bind_opts t_bind_opts;
//	mg_connect_opts t_connect_opts;
//
//	mgr = &t_mgr;
//	opt = &t_opt;
//	bind_opts = &t_bind_opts;
//	connect_opts = &t_connect_opts;
//
//	memset(&t_opt, 0, sizeof(mg_serve_http_opts));
//	memset(&t_bind_opts, 0, sizeof(mg_bind_opts));
//	memset(&t_connect_opts, 0, sizeof(mg_connect_opts));
//	try {
//		ctx = 0;
//		t_opt.extra_headers = extraHeader;
//		mg_mgr_init(mgr, this);
//#if MG_ENABLE_SSL
//		if (useSSL) {
//			if (!wxFileExists("ssl_cert.pem") || !wxFileExists("ssl_key.key")) {
//				throw StringException("ssl certificate ssl_cert.pem and ssl_key.key not found");
//			}
//			t_connect_opts.ssl_cert = "ssl_cert.pem";
//			t_connect_opts.ssl_key = "ssl_key.key";
//		}
//#endif
//		if (isHTTPconnection) {
//			client = mg_connect_http_opt(&t_mgr, ev_handler, t_connect_opts, uri, extraHeader, (postData.IsEmpty() ? (const char*)NULL : (const char*)postData.mbc_str()));
//		}
//		else {
//			client = mg_connect_ws_opt(&t_mgr, ev_handler, t_connect_opts, uri, "ppos_chat", extraHeader);
//		}
//		if (!client) {
//			isConnected = false;
//			isReady.Post(false); // not ready.
//		}
//		else {
//			isConnected = true;
//			isReady.Post(true);
//
//			std::shared_ptr<LoginSessionRecord> sess = CreateLoginSession(0, "");
//			ctx = sess->connectionCtx;
//			client->user_data = (void*)ctx;
//			allowShutdownByPeer = true;
//			for (; true;) {
//				WSSocket::Commands cmd;
//				if (commandQueue.ReceiveTimeout(WAITINGTIME, cmd) == wxMSGQUEUE_NO_ERROR)
//					if (cmd == WSSocket::Commands::evStop) break;
//				mg_mgr_poll(mgr, WAITINGTIME);
//			}
//		}
//	}
//	catch (wpSQLException & e) {
//		ShowLog("WSServer: sqlite3 exception: " + e.GetMessage());
//	}
//	catch (StringException & e) {
//		ShowLog("WSServer: stringexception: " + e.GetMessage());
//	}
//	catch (...) {
//		ShowLog("WSServer: unknown exception");
//	}
//
//	mg_mgr_free(&t_mgr);
//	ctx = 0;
//	client = NULL;
	return 0;
}


void WebClient::Init(Connection* ow, const wxString& ur, bool withSSL, bool isHttp) {
	owner = ow; socketUrl = ur; isHTTPconnection = isHttp;

	isConnected = false;
	mg_connection* client = NULL;
	ctx = 0;
	opt.extra_headers = extraHeader;
	mg_mgr_init(&mgr, this);
#if MG_ENABLE_SSL
	if (withSSL) {
		if (!wxFileExists("ssl_cert.pem") || !wxFileExists("ssl_key.key")) {
			throw StringException("ssl certificate ssl_cert.pem and ssl_key.key not found");
		}
		connect_opts.ssl_cert = "ssl_cert.pem";
		connect_opts.ssl_key = "ssl_key.key";
	}
#endif
	if (isHTTPconnection) {
		client = mg_connect_http_opt(&mgr, ev_handler, connect_opts, uri, extraHeader, (postData.IsEmpty() ? (const char*)NULL : (const char*)postData.mbc_str()));
	}
	else {
		client = mg_connect_ws_opt(&mgr, ev_handler, connect_opts, uri, "ppos_chat", extraHeader);
	}
	if (!client) {
		isConnected = false;
		mg_mgr_free(&mgr);
		return;
	}
	isConnected = true;

	std::shared_ptr<LoginSessionRecord> sess = CreateLoginSession(0, "");
	ctx = sess->connectionCtx;
	client->user_data = (void*)ctx;
	allowShutdownByPeer = true;
}

int gWaitForReadyTimeout = 30; //seconds

bool WSSocket::WaitForReady() {
	wxDateTime tStart = wxDateTime::UNow();
	while (true) {
		if (wxDateTime::UNow().Subtract(tStart).GetSeconds() > gWaitForReadyTimeout) return false;
		bool b;
		if (isReady.ReceiveTimeout(WAITINGTIME, b) == wxMSGQUEUE_NO_ERROR)
			return b;
	}
	return false;
}

void WSClient::evConnected(mg_connection*, int) {
	isConnected = true;
	if (isHTTPconnection) {
		isReady.Post(true);
	}
}

bool WSClient::evClosed(mg_connection* /*nc*/) { isConnected = false; isReady.Post(false); return true; }

void WSClient::evHandshaken(mg_connection* /*nc*/) {
	isConnected = true;
	isReady.Post(true);
}


WSServer::WSServer(int pNo, const char* docRoot) : WSSocket() {
	portNo.Printf("%d",pNo);
	rootFolder = wxGetCwd() + wxFileName::GetPathSeparator() + docRoot;
}

//static struct mg_serve_http_opts s_http_server_opts;
//static void ev0_handler(struct mg_connection* nc, int ev, void* p) {
//	//WSServer* svr = (WSServer*)nc->mgr->user_data;
//	if (ev == MG_EV_HTTP_REQUEST) {
//		mg_serve_http(nc, (struct http_message*)p, s_http_server_opts);
//	}
//}

extern bool keepMoving; 
wxThread::ExitCode WSServer::Entry() {
//	mg_mgr t_mgr;
//	mg_serve_http_opts t_opt;
//	mg_bind_opts t_bind_opts;
//	mg_connect_opts t_connect_opts;
//
//	mgr = &t_mgr;
//	opt = &t_opt;
//	bind_opts = &t_bind_opts;
//	connect_opts = &t_connect_opts;
//
//	memset(&t_opt, 0, sizeof(mg_serve_http_opts));
//	memset(&t_bind_opts, 0, sizeof(mg_bind_opts));
//	memset(&t_connect_opts, 0, sizeof(mg_connect_opts));
//
//	try {
//#if MG_ENABLE_SSL
//		if (useSSL) {
//			if (!wxFileExists("ssl_cert.pem") || !wxFileExists("ssl_key.key")) {
//				throw StringException("ssl certificate ssl_cert.pem and ssl_key.key not found");
//			}
//			t_bind_opts.ssl_cert = "ssl_cert.pem";
//			t_bind_opts.ssl_key = "ssl_key.key";
//		}
//#endif
//		t_opt.document_root = rootFolder;
//		t_opt.extra_headers = extraHeader;
//		t_opt.enable_directory_listing = "yes";
//
//		mg_mgr_init(&t_mgr, this);
//		if (t_mgr.user_data != this) {
//			ShowLog("something wrong");
//		}
//		struct mg_connection* nc = mg_bind_opt(&t_mgr, portNo, ev_handler, t_bind_opts);
//		if (nc) {
//			mg_set_protocol_http_websocket(nc);
//			isConnected = true;
//			isReady.Post(true);
//			for (; keepMoving;) {
//				WSSocket::Commands cmd;
//				if (commandQueue.ReceiveTimeout(WAITINGTIME, cmd) == wxMSGQUEUE_NO_ERROR) {
//					if (cmd == WSSocket::Commands::evStop) break;
//				}
//				mg_mgr_poll(&t_mgr, WAITINGTIME);
//			}
//			Broadcast(thisServerID(), true, "the server is shutting down", "*");
//		} else {
//			isReady.Post(false);
//			ShowLog(wxString("WSServer: cannot bind to port ") + portNo);
//		}
//		ShowLog("WSServer thread closing...");
//	} catch (wpSQLException& e) {
//		ShowLog("WSServer: sqlite3 exception: " + e.GetMessage());
//	} catch (StringException& e) {
//		ShowLog("WSServer: stringexception: " + e.GetMessage());
//	} catch (...) {
//		ShowLog("WSServer: unknown exception");
//	}
//	mg_mgr_free(&t_mgr);
//	SendFeedbackToMain(102, this); //102-wsserver thread; 101-liverserver thread. - to remove the thread from list.
//	isConnected = false;
	return 0;
}

bool WSServer::evClosed(mg_connection* nc) {
	WSSocket::evClosed(nc);
	std::shared_ptr<LoginSessionRecord> p = GetLS(nc);
	if (p && !p->sessionKey.IsEmpty()) {
		std::shared_ptr<WorkerThreadQueue::Queue> q(new WorkerThreadQueue::Queue(this, uintptr_t(nc->user_data), "<forcelogout/>"));
		WorkerThreadQueue::Execute(q);
		return true;
	}
	return false;
}

void WebServer::Init(int pNo, const char* docRoot, bool withSSL) {
	portNo.Printf("%d", pNo);
#if MG_ENABLE_SSL
	if (withSSL) {
		if (!wxFileExists("ssl_cert.pem") || !wxFileExists("ssl_key.key")) {
			throw StringException("ssl certificate ssl_cert.pem and ssl_key.key not found");
		}
		bind_opts.ssl_cert = "ssl_cert.pem";
		bind_opts.ssl_key = "ssl_key.key";
	}
#endif
	opt.document_root = docRoot;
	opt.extra_headers = extraHeader;
	opt.enable_directory_listing = "yes";

	mg_mgr_init(&mgr, this);
	if (mgr.user_data != this) {
		ShowLog("something wrong");
	}
	struct mg_connection* nc = mg_bind_opt(&mgr, portNo, ev_handler, bind_opts);
	if (nc) {
		mg_set_protocol_http_websocket(nc);
		isConnected = true;
	}
	else {
		isConnected = false;
		mg_mgr_free(&mgr);
	}
}

//void WSServer::ExecuteIncomingData(std::shared_ptr<LoginSessionRecord>, std::shared_ptr<QueueData> data) {
//	std::shared_ptr<WorkerThreadQueue::Queue> q(new WorkerThreadQueue::Queue(data->conn, wxString((const char *)data->buf->GetData(), data->buf->GetDataLen())));
//	q->replyPrefix = replyChar;
//	q->messageID = data->messageID;
//	q->targetID = data->targetID;
//	q->targetType = data->targetType;
//	WorkerThreadQueue::Execute(q);
//}

int WSServer::GetNumberOfClients() {
	return GetLoginSessionList(CommandLoginListOptions::ActiveUsers).size();
}

wxJSONValue WSServer::GetOnlineUsers() {
	wxJSONValue v;
	for (auto const &it: GetLoginSessionList(CommandLoginListOptions::ActiveUsers)) {
		wxJSONValue s = it->GetJSON();
		s["user"].Remove("menu");
		UserRecord rec;
		rec.SetContent(s["user"]["content"].AsString());
		s["user"].Remove("content");
		s["user"]["code"] = rec.GetCode();
		s["user"]["name"] = rec.GetName();
		v.Append(s);
	}
	return v;
}

//void WebClient::ev_handler(struct mg_connection* nc, int ev, void* ev_data) {
//	struct websocket_message* wm = (struct websocket_message*)ev_data;
//	WSSocket* ptr = static_cast<WSSocket*>(nc->mgr->user_data);
//	wxASSERT(ptr);
//
//	auto fnReturnError = [nc](wpSocketFrameHeader* sfh) {
//		std::shared_ptr<wpSocketFrameHeader> hdr(new wpSocketFrameHeader(sfh->originServer, sfh->targetType, sfh->messageID));
//		hdr->prefix = replyChar;
//		hdr->httpMessageLength = 0;
//		mg_send_websocket_frame(nc, WEBSOCKET_OP_BINARY | WEBSOCKET_DONT_FIN, hdr.get(), sizeof(wpSocketFrameHeader));
//		mg_send_websocket_frame(nc, WEBSOCKET_OP_CONTINUE, ERROR_SERVER_NOT_FOUND, Length_ERROR_SERVER_NOT_FOUND);
//	};
//
//	switch (ev) {
//	case MG_EV_ACCEPT: {
//		//ShowLog("MG_EV_ACCEPT");
//		break;
//	}
//	case MG_EV_CONNECT: {
//#ifdef _SHOWLOG_
//		LoggerTracker __trackker("MG_EV_CONNECT", false);
//#endif
//		int flag = *(int*)ev_data;
//		ptr->evConnected(nc, flag);
//		std::shared_ptr<LoginSessionRecord> session = GetLoginSession(uintptr_t(nc->user_data), true);
//		nc->user_data = (void*)session->connectionCtx;
//		//ShowLog(thisComputerID+" MG_EV_CONNECT:" + session->terminal.code);
//		if (flag == 0) {
//			session->status = LoginSessionRecord::Connected;
//			mg_send_websocket_handshake2(nc, ptr->socketUrl.mbc_str(), NULL, "ppos_chat", NULL);
//		}
//		else {
//			session->status = LoginSessionRecord::Failed;
//			ptr->errMessage = strerror(flag);
//		}
//		break;
//	}
//	case MG_EV_CLOSE: {
//#ifdef _SHOWLOG_
//		LoggerTracker __trackker("MG_EV_CLOSE", false);
//#endif
//		ptr->isShuttingDown = ptr->allowShutdownByPeer;
//		if (!ptr->evClosed(nc)) {
//			std::shared_ptr<LoginSessionRecord> session = GetLS(nc);
//			if (session && !session->sessionKey.IsEmpty()) {
//				session->status = LoginSessionRecord::Disconnected; // set to zero;
//				//ptr->CloseConnection(session);
//			}
//			DestroySessionRecord(uintptr_t(nc->user_data)); //-- this will be done by <forcelogout>
//		}
//		break;
//	}
//	case MG_EV_POLL: {
//		ptr->OnPoll(nc);
//		break;
//	}
//	case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST: {
//		break;
//	}
//
//	case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
//#ifdef _SHOWLOG_
//		LoggerTracker __trackker("MG_EV_WEBSOCKET_HANDSHAKE_DONE", false);
//#endif
//		ptr->evHandshaken(nc);
//		std::shared_ptr<LoginSessionRecord> session = GetLoginSession(uintptr_t(nc->user_data), true); // create if not there yet
//		if (nc->user_data == NULL) nc->user_data = (void*)session->connectionCtx;
//		//ShowLog(thisComputerID + " MG_EV_WEBSOCKET_HANDSHAKE_DONE:" + session->terminal.code);
//		session->status = LoginSessionRecord::Handshaken;
//		break;
//	}
//	case MG_EV_HTTP_REQUEST: {
//#ifdef _SHOWLOG_
//		LoggerTracker __trackker("MG_EV_HTTP_REQUEST", false);
//#endif
//		struct http_message* hm = (struct http_message*)ev_data;
//		ptr->evHTTPrequest(nc, hm);
//		std::shared_ptr<LoginSessionRecord> session = GetLoginSession(uintptr_t(nc->user_data), true); // create if not there yet
//		if (nc->user_data == NULL) nc->user_data = (void*)session->connectionCtx;
//		//ShowLog(thisComputerID + " MG_EV_HTTP_REQUEST:" + session->terminal.code);
//		if (session->status <= LoginSessionRecord::Failed) session->status = LoginSessionRecord::Connected;
//		session->isHTTP = true;
//		std::shared_ptr<WorkerThreadQueue::Queue> q(new WorkerThreadQueue::Queue(ptr, uintptr_t(nc->user_data), hm)); // TODO: parse http_message header to get the targetID, originID etc.
//		WorkerThreadQueue::Execute(q);
//		break;
//	}
//	case MG_EV_HTTP_REPLY: {
//#ifdef _SHOWLOG_
//		LoggerTracker __trackker("MG_EV_HTTP_REPLY", false);
//#endif
//		struct http_message* hm = (struct http_message*)ev_data;
//		ptr->evHTTPreply(nc, hm);
//		std::shared_ptr<LoginSessionRecord> session = GetLoginSession(uintptr_t(nc->user_data), false); // must have been there
//		if (session) {
//			//ShowLog(thisComputerID + " MG_EV_HTTP_REPLY: from " + session->terminal.code);
//			session->isHTTP = true;
//			if (session->status <= LoginSessionRecord::Failed) session->status = LoginSessionRecord::Connected;
//			//nc->flags |= MG_F_CLOSE_IMMEDIATELY; // if HTTP to close every call.
//			std::shared_ptr<QueueData> data(new QueueData(session->connectionCtx)); // TODO: parse hm to get targetID, originID, etc
//			data->httpMessage.reset(new wxMemoryBuffer);
//			data->httpMessage->AppendData(hm->message.p, hm->message.len);
//			data->isHTTP = true;
//			session->incoming.Post(data);
//		}// else ShowLog(thisComputerID + " MG_EV_HTTP_REPLY: **** non existing session ****");
//		ptr->isShuttingDown = ptr->allowShutdownByPeer;
//		break;
//	}
//	case MG_EV_WEBSOCKET_FRAME: { // prefix, msgID, buffer
//#ifdef _SHOWLOG_
//		LoggerTracker __trackker("MG_EV_WEBSOCKET_FRAME", false);
//#endif
//		wpSocketFrameHeader* sfh = (wpSocketFrameHeader*)wm->data;
//		if (strncmp((const char*)sfh->sig, socketFrameVersion, socketFrameVersionLength) != 0) { // wrong version
//			nc->flags |= MG_F_CLOSE_IMMEDIATELY;
//			break; // don't process other socket frame;
//		}
//		if (!IsValidMessage(sfh)) {
//#ifdef _SHOWLOG_
//			ShowLog("Received message-invalid, ignored. " + UniversalUniqueID(sfh->originServer)() + " -> " + UniversalUniqueID(sfh->targetID)());
//#endif
//			//fnReturnError(sfh);
//			break; // don't process other socket frame;
//		}
//		if (!MessageRunHere(sfh)) {
//#ifdef _SHOWLOG_
//			ShowLog(UniversalUniqueID(sfh->originServer)() + " -> " + UniversalUniqueID(sfh->targetID)());
//#endif
//			if (sfh->targetType == wpSocketFrameHeader::targetServer) {
//				if (!ptr->SendToServer(wm)) { // cannot find server.
//					fnReturnError(sfh);
//				}
//			}
//			else if (sfh->targetType == wpSocketFrameHeader::targetGroup) {
//				ptr->SendToGroup(wm);
//			}
//			break;
//		}
//#ifdef _SHOWLOG_
//		ShowLog(UniversalUniqueID(sfh->originServer)() + " oo " + UniversalUniqueID(sfh->targetID)());
//#endif
//		// this one line causes the whole thing hanged after one broadcast.
//		//if (sfh->targetType == wpSocketFrameHeader::targetGroup) {
//		//	ptr->SendToGroup(wm);
//		//}
//
//		std::shared_ptr<LoginSessionRecord> session = GetLoginSession(uintptr_t(nc->user_data), true);
//		if (nc->user_data == NULL) nc->user_data = (void*)session->connectionCtx;
//		session->isHTTP = false;
//		if (session->status <= LoginSessionRecord::Failed) session->status = LoginSessionRecord::Connected;
//
//		std::shared_ptr<wxMemoryBuffer> httpMessage(new wxMemoryBuffer);
//		const char* p = (const char*)wm->data + sizeof(wpSocketFrameHeader);
//		int len = wm->size - sizeof(wpSocketFrameHeader);
//		if (sfh->httpMessageLength > 0) {
//			httpMessage->AppendData(p, sfh->httpMessageLength);
//			p += sfh->httpMessageLength;
//			len -= sfh->httpMessageLength;
//		}
//		switch (sfh->prefix) {
//		case broadcastChar: {
//			wxString mesg(p, len);
//			wxCommandEvent e(wpEVT_DB_SERVER_MESSAGE);
//#ifdef _SHOWLOG_
//			ShowLog("Received broadcast data " + mesg);
//#endif
//			e.SetString(mesg);
//			e.SetEventObject(NULL);
//			wxQueueEvent(wxTheApp, e.Clone());
//			break;
//		}
//		case commandChar: {
//			if (ptr) {
//				ptr->ExecuteCommand(sfh, nc, wxString(p, len), httpMessage);
//			}
//			break;
//		}
//		default: {
//			std::shared_ptr<QueueData> data(new QueueData(session->connectionCtx));
//			data->messageID.Copy(sfh->messageID);
//			data->buf.reset(new wxMemoryBuffer);
//			data->httpMessage = httpMessage;
//			data->buf->AppendData(p, len);
//			data->targetID.Copy(sfh->targetID);
//			data->targetType = sfh->targetType;
//			session->incoming.Post(data);
//			break;
//		}
//		}
//	}
//	}
//}
//
//void WebServer::ev_handler(struct mg_connection* nc, int ev, void* ev_data) {
//	struct websocket_message* wm = (struct websocket_message*)ev_data;
//	WSSocket* ptr = static_cast<WSSocket*>(nc->mgr->user_data);
//	wxASSERT(ptr);
//
//	auto fnReturnError = [nc](wpSocketFrameHeader* sfh) {
//		std::shared_ptr<wpSocketFrameHeader> hdr(new wpSocketFrameHeader(sfh->originServer, sfh->targetType, sfh->messageID));
//		hdr->prefix = replyChar;
//		hdr->httpMessageLength = 0;
//		mg_send_websocket_frame(nc, WEBSOCKET_OP_BINARY | WEBSOCKET_DONT_FIN, hdr.get(), sizeof(wpSocketFrameHeader));
//		mg_send_websocket_frame(nc, WEBSOCKET_OP_CONTINUE, ERROR_SERVER_NOT_FOUND, Length_ERROR_SERVER_NOT_FOUND);
//	};
//
//	switch (ev) {
//	case MG_EV_ACCEPT: {
//		//ShowLog("MG_EV_ACCEPT");
//		break;
//	}
//	case MG_EV_CONNECT: {
//#ifdef _SHOWLOG_
//		LoggerTracker __trackker("MG_EV_CONNECT", false);
//#endif
//		int flag = *(int*)ev_data;
//		ptr->evConnected(nc, flag);
//		std::shared_ptr<LoginSessionRecord> session = GetLoginSession(uintptr_t(nc->user_data), true);
//		nc->user_data = (void*)session->connectionCtx;
//		//ShowLog(thisComputerID+" MG_EV_CONNECT:" + session->terminal.code);
//		if (flag == 0) {
//			session->status = LoginSessionRecord::Connected;
//			mg_send_websocket_handshake2(nc, ptr->socketUrl.mbc_str(), NULL, "ppos_chat", NULL);
//		}
//		else {
//			session->status = LoginSessionRecord::Failed;
//			ptr->errMessage = strerror(flag);
//		}
//		break;
//	}
//	case MG_EV_CLOSE: {
//#ifdef _SHOWLOG_
//		LoggerTracker __trackker("MG_EV_CLOSE", false);
//#endif
//		ptr->isShuttingDown = ptr->allowShutdownByPeer;
//		if (!ptr->evClosed(nc)) {
//			std::shared_ptr<LoginSessionRecord> session = GetLS(nc);
//			if (session && !session->sessionKey.IsEmpty()) {
//				session->status = LoginSessionRecord::Disconnected; // set to zero;
//				//ptr->CloseConnection(session);
//			}
//			DestroySessionRecord(uintptr_t(nc->user_data)); //-- this will be done by <forcelogout>
//		}
//		break;
//	}
//	case MG_EV_POLL: {
//		ptr->OnPoll(nc);
//		break;
//	}
//	case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST: {
//		break;
//	}
//
//	case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
//#ifdef _SHOWLOG_
//		LoggerTracker __trackker("MG_EV_WEBSOCKET_HANDSHAKE_DONE", false);
//#endif
//		ptr->evHandshaken(nc);
//		std::shared_ptr<LoginSessionRecord> session = GetLoginSession(uintptr_t(nc->user_data), true); // create if not there yet
//		if (nc->user_data == NULL) nc->user_data = (void*)session->connectionCtx;
//		//ShowLog(thisComputerID + " MG_EV_WEBSOCKET_HANDSHAKE_DONE:" + session->terminal.code);
//		session->status = LoginSessionRecord::Handshaken;
//		break;
//	}
//	case MG_EV_HTTP_REQUEST: {
//#ifdef _SHOWLOG_
//		LoggerTracker __trackker("MG_EV_HTTP_REQUEST", false);
//#endif
//		struct http_message* hm = (struct http_message*)ev_data;
//		ptr->evHTTPrequest(nc, hm);
//		std::shared_ptr<LoginSessionRecord> session = GetLoginSession(uintptr_t(nc->user_data), true); // create if not there yet
//		if (nc->user_data == NULL) nc->user_data = (void*)session->connectionCtx;
//		//ShowLog(thisComputerID + " MG_EV_HTTP_REQUEST:" + session->terminal.code);
//		if (session->status <= LoginSessionRecord::Failed) session->status = LoginSessionRecord::Connected;
//		session->isHTTP = true;
//		std::shared_ptr<WorkerThreadQueue::Queue> q(new WorkerThreadQueue::Queue(ptr, uintptr_t(nc->user_data), hm)); // TODO: parse http_message header to get the targetID, originID etc.
//		WorkerThreadQueue::Execute(q);
//		break;
//	}
//	case MG_EV_HTTP_REPLY: {
//#ifdef _SHOWLOG_
//		LoggerTracker __trackker("MG_EV_HTTP_REPLY", false);
//#endif
//		struct http_message* hm = (struct http_message*)ev_data;
//		ptr->evHTTPreply(nc, hm);
//		std::shared_ptr<LoginSessionRecord> session = GetLoginSession(uintptr_t(nc->user_data), false); // must have been there
//		if (session) {
//			//ShowLog(thisComputerID + " MG_EV_HTTP_REPLY: from " + session->terminal.code);
//			session->isHTTP = true;
//			if (session->status <= LoginSessionRecord::Failed) session->status = LoginSessionRecord::Connected;
//			//nc->flags |= MG_F_CLOSE_IMMEDIATELY; // if HTTP to close every call.
//			std::shared_ptr<QueueData> data(new QueueData(session->connectionCtx)); // TODO: parse hm to get targetID, originID, etc
//			data->httpMessage.reset(new wxMemoryBuffer);
//			data->httpMessage->AppendData(hm->message.p, hm->message.len);
//			data->isHTTP = true;
//			session->incoming.Post(data);
//		}// else ShowLog(thisComputerID + " MG_EV_HTTP_REPLY: **** non existing session ****");
//		ptr->isShuttingDown = ptr->allowShutdownByPeer;
//		break;
//	}
//	case MG_EV_WEBSOCKET_FRAME: { // prefix, msgID, buffer
//#ifdef _SHOWLOG_
//		LoggerTracker __trackker("MG_EV_WEBSOCKET_FRAME", false);
//#endif
//		wpSocketFrameHeader* sfh = (wpSocketFrameHeader*)wm->data;
//		if (strncmp((const char*)sfh->sig, socketFrameVersion, socketFrameVersionLength) != 0) { // wrong version
//			nc->flags |= MG_F_CLOSE_IMMEDIATELY;
//			break; // don't process other socket frame;
//		}
//		if (!IsValidMessage(sfh)) {
//#ifdef _SHOWLOG_
//			ShowLog("Received message-invalid, ignored. " + UniversalUniqueID(sfh->originServer)() + " -> " + UniversalUniqueID(sfh->targetID)());
//#endif
//			//fnReturnError(sfh);
//			break; // don't process other socket frame;
//		}
//		if (!MessageRunHere(sfh)) {
//#ifdef _SHOWLOG_
//			ShowLog(UniversalUniqueID(sfh->originServer)() + " -> " + UniversalUniqueID(sfh->targetID)());
//#endif
//			if (sfh->targetType == wpSocketFrameHeader::targetServer) {
//				if (!ptr->SendToServer(wm)) { // cannot find server.
//					fnReturnError(sfh);
//				}
//			}
//			else if (sfh->targetType == wpSocketFrameHeader::targetGroup) {
//				ptr->SendToGroup(wm);
//			}
//			break;
//		}
//#ifdef _SHOWLOG_
//		ShowLog(UniversalUniqueID(sfh->originServer)() + " oo " + UniversalUniqueID(sfh->targetID)());
//#endif
//		// this one line causes the whole thing hanged after one broadcast.
//		//if (sfh->targetType == wpSocketFrameHeader::targetGroup) {
//		//	ptr->SendToGroup(wm);
//		//}
//
//		std::shared_ptr<LoginSessionRecord> session = GetLoginSession(uintptr_t(nc->user_data), true);
//		if (nc->user_data == NULL) nc->user_data = (void*)session->connectionCtx;
//		session->isHTTP = false;
//		if (session->status <= LoginSessionRecord::Failed) session->status = LoginSessionRecord::Connected;
//
//		std::shared_ptr<wxMemoryBuffer> httpMessage(new wxMemoryBuffer);
//		const char* p = (const char*)wm->data + sizeof(wpSocketFrameHeader);
//		int len = wm->size - sizeof(wpSocketFrameHeader);
//		if (sfh->httpMessageLength > 0) {
//			httpMessage->AppendData(p, sfh->httpMessageLength);
//			p += sfh->httpMessageLength;
//			len -= sfh->httpMessageLength;
//		}
//		switch (sfh->prefix) {
//		case broadcastChar: {
//			wxString mesg(p, len);
//			wxCommandEvent e(wpEVT_DB_SERVER_MESSAGE);
//#ifdef _SHOWLOG_
//			ShowLog("Received broadcast data " + mesg);
//#endif
//			e.SetString(mesg);
//			e.SetEventObject(NULL);
//			wxQueueEvent(wxTheApp, e.Clone());
//			break;
//		}
//		case commandChar: {
//			if (ptr) {
//				ptr->ExecuteCommand(sfh, nc, wxString(p, len), httpMessage);
//			}
//			break;
//		}
//		default: {
//			std::shared_ptr<QueueData> data(new QueueData(session->connectionCtx));
//			data->messageID.Copy(sfh->messageID);
//			data->buf.reset(new wxMemoryBuffer);
//			data->httpMessage = httpMessage;
//			data->buf->AppendData(p, len);
//			data->targetID.Copy(sfh->targetID);
//			data->targetType = sfh->targetType;
//			session->incoming.Post(data);
//			break;
//		}
//		}
//	}
//	}
//}
