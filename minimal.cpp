#include "wx/wxprec.h"
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif
#include <thread>
#include <string>

#include "mongoose.h"

static void ev_handler(struct mg_connection* nc, int ev, void* p) {
    if (ev == MG_EV_HTTP_REQUEST) {
        auto s_http_server_opts = (mg_serve_http_opts*)nc->user_data;
        mg_serve_http(nc, (struct http_message*)p, *s_http_server_opts);
    }
}

constexpr int LOOPWAITINGTIME = 20;
bool stopIt = false;

auto StartWebServer(int portNo) -> int {

    mg_serve_http_opts s_http_server_opts;
    memset(&s_http_server_opts, 0, sizeof(mg_serve_http_opts));

    struct mg_mgr mgr;
    struct mg_connection* nc;

    mg_mgr_init(&mgr, nullptr);
    std::string p = std::to_string(portNo);
    nc = mg_bind(&mgr, p.c_str(), ev_handler);
    if (nc == nullptr) {
        return -1;
    }

    // Set up HTTP server parameters
    std::string startDir = "./web-";
    startDir.append(p);
    mg_set_protocol_http_websocket(nc);
    s_http_server_opts.document_root = startDir.c_str();  // Serve current directory
    s_http_server_opts.enable_directory_listing = "yes";

    nc->user_data = &s_http_server_opts;

    for (; !stopIt;) {
        mg_mgr_poll(&mgr, LOOPWAITINGTIME);
    }
    mg_mgr_free(&mgr);

    return 0;
}

//--------------------- ui

int startPortNo = 8000;
class MyApp : public wxApp {
    std::unordered_map<int, std::thread> webServerList;
public:
    auto OnInit() -> bool override;
    auto GetNumberOfServers() -> int { return webServerList.size(); }
    auto StartWebServer() ->int {
        webServerList[startPortNo] = std::thread(::StartWebServer, startPortNo);
        return startPortNo++;
    }
    auto OnExit() ->int override {
        stopIt = true;
        for (auto& t : webServerList) {
            if (t.second.joinable()) {
                t.second.join();
            }
        }
        return wxApp::OnExit();
    }
};

class MyFrame : public wxFrame {
public:
    MyFrame(const wxString& title);
};

enum {
    Minimal_Quit = wxID_EXIT,
    Minimal_About = wxID_ABOUT,
    Minimal_StartWebServer 
};

wxIMPLEMENT_APP(MyApp);

auto MyApp::OnInit() -> bool {
    if (!wxApp::OnInit()) {
        return false;
    }
    MyFrame *frame = new MyFrame("Minimal wxWidgets App");
    frame->Show(true);
    return true;
}


MyFrame::MyFrame(const wxString& title) : wxFrame(nullptr, wxID_ANY, title) {
    auto fileMenu = new wxMenu;
    auto helpMenu = new wxMenu;
    helpMenu->Append(Minimal_About, "&About\tF1", "Show about dialog");
    fileMenu->Append(Minimal_StartWebServer, "Start &Web\tAlt-W", "Start Web Server");
    fileMenu->Append(Minimal_Quit, "E&xit\tAlt-X", "Quit this program");
    auto menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, "&File");
    menuBar->Append(helpMenu, "&Help");
    SetMenuBar(menuBar);
    CreateStatusBar(3);
    SetStatusText("Welcome to wxWidgets!");
    Bind(wxEVT_COMMAND_MENU_SELECTED, [this](wxCommandEvent &e) {
        wxMessageBox(wxString::Format(
                    "Welcome to %s!\n"
                    "\n"
                    "This is the minimal wxWidgets sample\n"
                    "running under %s.",
                    wxVERSION_STRING,
                    wxGetOsDescription()
                 ),
                 "About wxWidgets minimal sample",
                 wxOK | wxICON_INFORMATION,
                 this);

    }, Minimal_About);

    Bind(wxEVT_COMMAND_MENU_SELECTED, [this](wxCommandEvent &e) {
       Close(true);
    }, Minimal_Quit);

    Bind(wxEVT_COMMAND_MENU_SELECTED, [this](wxCommandEvent &e) {
        int pNo = wxGetApp().StartWebServer();
        SetStatusText(wxString::Format("Web server started on port %d", pNo), 1);
        SetStatusText(wxString::Format("No of server(s) running : %d", wxGetApp().GetNumberOfServers()), 2);
    }, Minimal_StartWebServer);

}
