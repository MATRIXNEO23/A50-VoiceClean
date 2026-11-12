// Native Windows controller for experimental live audio routing.
// Does not install drivers, change ASTRO firmware or claim a virtual microphone.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include <cwctype>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
namespace {
enum : int {InputSelect=101,OutputSelect=102,Refresh=103,Start=104,Stop=105,
             GainSlider=201,ProbabilitySlider=202,FloorSlider=203,
             DenoiseCheck=301,VadCheck=302};
HWND window=nullptr,in_combo=nullptr,out_combo=nullptr,gain_slider=nullptr,
     probability_slider=nullptr,floor_slider=nullptr,denoise_check=nullptr,
     vad_check=nullptr,status_text=nullptr,gain_value=nullptr,
     probability_value=nullptr,floor_value=nullptr,start_button=nullptr,stop_button=nullptr;
HFONT font=nullptr;
PROCESS_INFORMATION process{};
HANDLE stop_event=nullptr,job=nullptr;
std::vector<std::wstring> inputs,outputs;

void check(HRESULT hr,const char* op) {
    if(FAILED(hr)) throw std::runtime_error(op);
}
std::wstring lower(std::wstring s) {
    std::transform(s.begin(),s.end(),s.begin(),[](wchar_t c){
        return static_cast<wchar_t>(std::towlower(c));
    });
    return s;
}
void message(const std::wstring& s) {
    if(status_text) SetWindowTextW(status_text,s.c_str());
}
HWND control(const wchar_t* cls,const wchar_t* caption,DWORD style,
             int x,int y,int w,int h,int id=0) {
    HWND handle=CreateWindowExW(0,cls,caption,WS_CHILD|WS_VISIBLE|style,
      x,y,w,h,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
      GetModuleHandleW(nullptr),nullptr);
    if(handle) SendMessageW(handle,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
    return handle;
}
std::vector<std::wstring> endpoint_names(IMMDeviceEnumerator* en,EDataFlow flow) {
    ComPtr<IMMDeviceCollection> coll;
    check(en->EnumAudioEndpoints(flow,DEVICE_STATE_ACTIVE,&coll),"Enumerazione dispositivi");
    UINT count=0;
    check(coll->GetCount(&count),"Conteggio dispositivi");
    std::vector<std::wstring> result;
    for(UINT i=0;i<count;++i) {
        ComPtr<IMMDevice> device;
        check(coll->Item(i,&device),"Dispositivo");
        ComPtr<IPropertyStore> store;
        check(device->OpenPropertyStore(STGM_READ,&store),"Proprieta");
        PROPVARIANT v;
        PropVariantInit(&v);
        check(store->GetValue(PKEY_Device_FriendlyName,&v),"Nome dispositivo");
        const std::wstring name=(v.vt==VT_LPWSTR && v.pwszVal)?v.pwszVal:L"(senza nome)";
        PropVariantClear(&v);
        result.push_back(name);
    }
    return result;
}
void populate() {
    ComPtr<IMMDeviceEnumerator> enumerator;
    check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,
          IID_PPV_ARGS(&enumerator)),"COM endpoint");
    inputs=endpoint_names(enumerator.Get(),eCapture);
    outputs=endpoint_names(enumerator.Get(),eRender);
    SendMessageW(in_combo,CB_RESETCONTENT,0,0);
    SendMessageW(out_combo,CB_RESETCONTENT,0,0);
    int preferred_input=-1,preferred_output=-1;
    for(std::size_t i=0;i<inputs.size();++i) {
        SendMessageW(in_combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(inputs[i].c_str()));
        auto n=lower(inputs[i]);
        if(n.find(L"astro a50")!=std::wstring::npos &&
           n.find(L"voice")!=std::wstring::npos) preferred_input=static_cast<int>(i);
    }
    for(std::size_t i=0;i<outputs.size();++i) {
        SendMessageW(out_combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(outputs[i].c_str()));
        auto n=lower(outputs[i]);
        if(n.find(L"cable input")!=std::wstring::npos) preferred_output=static_cast<int>(i);
    }
    if(preferred_input>=0) SendMessageW(in_combo,CB_SETCURSEL,preferred_input,0);
    else if(!inputs.empty()) SendMessageW(in_combo,CB_SETCURSEL,0,0);
    if(preferred_output>=0) SendMessageW(out_combo,CB_SETCURSEL,preferred_output,0);
    message(preferred_output>=0?L"Microfoni caricati. Pronto per l'avvio."
                               :L"Uscita CABLE Input non trovata: serve VB-CABLE per Jarvis.");
}
int slider(HWND h) { return static_cast<int>(SendMessageW(h,TBM_GETPOS,0,0)); }
void set_slider(HWND h,int from,int to,int initial) {
    SendMessageW(h,TBM_SETRANGEMIN,FALSE,from);
    SendMessageW(h,TBM_SETRANGEMAX,FALSE,to);
    SendMessageW(h,TBM_SETPOS,TRUE,initial);
}
void values() {
    SetWindowTextW(gain_value,(std::to_wstring(slider(gain_slider))+L" dB").c_str());
    SetWindowTextW(probability_value,
       (std::to_wstring(slider(probability_slider))+L" %").c_str());
    SetWindowTextW(floor_value,
       (std::to_wstring(slider(floor_slider))+L" dBFS").c_str());
}
bool running() {
    if(!process.hProcess) return false;
    return WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT;
}
void cleanup() {
    if(process.hProcess) {
        CloseHandle(process.hThread);CloseHandle(process.hProcess);
        process={};
    }
    if(job) {CloseHandle(job);job=nullptr;}
    if(stop_event) {CloseHandle(stop_event);stop_event=nullptr;}
}
void stop_engine() {
    if(process.hProcess) {
        if(stop_event) SetEvent(stop_event);
        if(WaitForSingleObject(process.hProcess,2500)==WAIT_TIMEOUT) {
            // Last-resort cleanup for a hung audio process.
            TerminateProcess(process.hProcess,1);
            WaitForSingleObject(process.hProcess,1000);
        }
    }
    cleanup();
    EnableWindow(start_button,TRUE);
    EnableWindow(stop_button,FALSE);
    message(L"Audio arrestato. Microfono ASTRO non modificato.");
}
std::filesystem::path engine_executable() {
    wchar_t full[32768]{};
    DWORD n=GetModuleFileNameW(nullptr,full,32768);
    if(!n || n>=32768) throw std::runtime_error("Percorso programma non disponibile");
    return std::filesystem::path(full).parent_path()/L"A50VoiceClean-Live.exe";
}
std::filesystem::path log_path() {
    wchar_t appdata[32768]{};
    DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",appdata,32768);
    if(!n || n>=32768) throw std::runtime_error("LOCALAPPDATA non disponibile");
    auto path=std::filesystem::path(appdata)/L"A50VoiceClean";
    std::filesystem::create_directories(path);
    return path/L"live.log";
}
void start_engine() {
    if(running()) return;
    if(process.hProcess) cleanup();
    const auto input=static_cast<int>(SendMessageW(in_combo,CB_GETCURSEL,0,0));
    const auto output=static_cast<int>(SendMessageW(out_combo,CB_GETCURSEL,0,0));
    if(input<0 || output<0 || static_cast<std::size_t>(input)>=inputs.size() ||
       static_cast<std::size_t>(output)>=outputs.size()) {
        message(L"Seleziona microfono ASTRO e uscita VB-CABLE.");return;
    }
    const auto n=lower(outputs[output]);
    if(n.find(L"cable input")==std::wstring::npos &&
       n.find(L"vb-audio")==std::wstring::npos) {
        message(L"Per sicurezza l'uscita deve essere una periferica audio virtuale.");
        return;
    }
    try {
        const auto executable=engine_executable();
        if(!std::filesystem::exists(executable)) {
            message(L"Manca A50VoiceClean-Live.exe nella stessa cartella.");return;
        }
        const auto log=log_path();
        SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
        HANDLE logfile=CreateFileW(log.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&sa,
                                  CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(logfile==INVALID_HANDLE_VALUE) throw std::runtime_error("Errore file log");
        HANDLE nul=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ,&sa,
                               OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(nul==INVALID_HANDLE_VALUE) {
            CloseHandle(logfile); throw std::runtime_error("Errore input standard");
        }
        const std::wstring event_name=L"Local\\A50VoiceCleanStop_"+
                                     std::to_wstring(GetCurrentProcessId());
        stop_event=CreateEventW(nullptr,TRUE,FALSE,event_name.c_str());
        if(!stop_event) {CloseHandle(logfile);CloseHandle(nul);throw std::runtime_error("Errore evento stop");}
        std::wstring args=L"\""+executable.wstring()+L"\" --input "+
            std::to_wstring(input)+L" --output "+std::to_wstring(output)+
            L" --gain-db "+std::to_wstring(slider(gain_slider))+
            L" --vad-prob "+std::to_wstring(slider(probability_slider)/100.0)+
            L" --voice-min-db "+std::to_wstring(slider(floor_slider))+
            L" --stop-event \""+event_name+L"\"";
        if(SendMessageW(denoise_check,BM_GETCHECK,0,0)!=BST_CHECKED) args+=L" --no-denoise";
        if(SendMessageW(vad_check,BM_GETCHECK,0,0)!=BST_CHECKED) args+=L" --no-vad";
        STARTUPINFOW si{};si.cb=sizeof(si);
        si.dwFlags=STARTF_USESTDHANDLES;
        si.hStdInput=nul;si.hStdOutput=logfile;si.hStdError=logfile;
        const bool created=CreateProcessW(executable.c_str(),args.data(),
                                          nullptr,nullptr,TRUE,CREATE_NO_WINDOW,
                                          nullptr,nullptr,&si,&process)!=FALSE;
        CloseHandle(logfile);CloseHandle(nul);
        if(!created) {
            cleanup();
            message(L"Impossibile avviare la pipeline audio. Controlla i dispositivi.");
            return;
        }
        job=CreateJobObjectW(nullptr,nullptr);
        if(job) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if(SetInformationJobObject(job,JobObjectExtendedLimitInformation,
                                       &limits,sizeof(limits))) {
                AssignProcessToJobObject(job,process.hProcess);
            }
        }
        EnableWindow(start_button,FALSE);
        EnableWindow(stop_button,TRUE);
        message(L"Audio live avviato. In Jarvis scegli CABLE Output come microfono.");
    } catch(const std::exception&) {
        if(process.hProcess) stop_engine();
        else cleanup();
        message(L"Errore avvio. Vedi la cartella del programma e i dispositivi audio.");
    }
}
LRESULT CALLBACK procedure(HWND h,UINT m,WPARAM w,LPARAM l) {
    switch(m) {
    case WM_CREATE: {
        window=h; font=static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        control(L"STATIC",L"A50 VoiceClean - controllo microfono live",0,18,15,700,22);
        control(L"STATIC",L"Microfono in ingresso",0,20,58,200,20);
        in_combo=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,20,82,564,280,InputSelect);
        control(L"STATIC",L"Uscita (VB-CABLE)",0,20,128,200,20);
        out_combo=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,20,152,564,280,OutputSelect);
        control(L"BUTTON",L"Aggiorna",BS_PUSHBUTTON,596,82,112,29,Refresh);
        control(L"STATIC",L"Volume/guadagno voce",0,20,202,200,19);
        gain_slider=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS,215,195,368,35,GainSlider);
        gain_value=control(L"STATIC",L"",0,598,201,110,18);
        control(L"STATIC",L"Sensibilita vocale (VAD)",0,20,259,200,19);
        probability_slider=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS,215,252,368,35,ProbabilitySlider);
        probability_value=control(L"STATIC",L"",0,598,258,110,18);
        control(L"STATIC",L"Soglia minima voce",0,20,316,200,19);
        floor_slider=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS,215,309,368,35,FloorSlider);
        floor_value=control(L"STATIC",L"",0,598,315,110,18);
        denoise_check=control(L"BUTTON",L"RNNoise (pulizia)",BS_AUTOCHECKBOX,20,370,185,24,DenoiseCheck);
        vad_check=control(L"BUTTON",L"Attiva solo con voce (VAD)",BS_AUTOCHECKBOX,245,370,255,24,VadCheck);
        SendMessageW(denoise_check,BM_SETCHECK,BST_CHECKED,0);
        SendMessageW(vad_check,BM_SETCHECK,BST_CHECKED,0);
        set_slider(gain_slider,-12,24,0);
        set_slider(probability_slider,20,95,65);
        set_slider(floor_slider,-80,-20,-46);
        values();
        start_button=control(L"BUTTON",L"Avvia",BS_PUSHBUTTON,20,410,130,34,Start);
        stop_button=control(L"BUTTON",L"Ferma",BS_PUSHBUTTON,165,410,130,34,Stop);
        EnableWindow(stop_button,FALSE);
        control(L"STATIC",L"Le regolazioni si applicano al prossimo Avvia. Niente voce femminile live per ora.",0,20,452,700,22);
        status_text=control(L"STATIC",L"Caricamento dispositivi...",0,20,484,700,39);
        try {populate();} catch(...) {message(L"Impossibile leggere i dispositivi audio.");}
        SetTimer(h,1,1000,nullptr);
        return 0;
    }
    case WM_HSCROLL: values();return 0;
    case WM_COMMAND:
        switch(LOWORD(w)) {
        case Refresh: if(!running()){try{populate();}catch(...){message(L"Errore lettura dispositivi.");}}return 0;
        case Start: start_engine();return 0;
        case Stop: stop_engine();return 0;
        }
        return 0;
    case WM_TIMER:
        if(process.hProcess && !running()) {
            DWORD code=0;GetExitCodeProcess(process.hProcess,&code);
            cleanup();
            EnableWindow(start_button,TRUE);EnableWindow(stop_button,FALSE);
            message(L"Pipeline terminata (codice "+std::to_wstring(code)+
                    L"). Diagnostica: %LOCALAPPDATA%\\A50VoiceClean\\live.log");
        }
        return 0;
    case WM_DESTROY:
        KillTimer(h,1);stop_engine();PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    const HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(co))return 1;
    INITCOMMONCONTROLSEX init{sizeof(init),ICC_BAR_CLASSES};
    InitCommonControlsEx(&init);
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=instance;
    wc.lpszClassName=L"A50VoiceCleanLiveController";
    wc.lpfnWndProc=procedure;
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    RegisterClassExW(&wc);
    HWND h=CreateWindowExW(0,wc.lpszClassName,L"A50 VoiceClean - Live",
        WS_OVERLAPPEDWINDOW&~WS_THICKFRAME,
        CW_USEDEFAULT,CW_USEDEFAULT,752,575,
        nullptr,nullptr,instance,nullptr);
    if(!h) {CoUninitialize();return 1;}
    ShowWindow(h,show);UpdateWindow(h);
    MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)>0) {
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
