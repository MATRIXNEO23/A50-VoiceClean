// Native Windows controller for experimental live audio routing.
// Does not install drivers, change ASTRO firmware or claim a virtual microphone.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "live_shared.h"
#include "live_profiles_win.h"
#include <commctrl.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <vector>
#include <cwctype>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
namespace {
enum : int {InputSelect=101,OutputSelect=102,Refresh=103,Start=104,Stop=105,
             GainSlider=201,ProbabilitySlider=202,FloorSlider=203,
             DenoiseCheck=301,VadCheck=302,VoiceCheck=303,
             PitchSlider=204,ResonanceSlider=205,BassSlider=206,
             BrightnessSlider=207,MixSlider=208,
             ProfileCombo=401,ProfileName=402,LoadProfile=403,SaveProfile=404};
HWND window=nullptr,in_combo=nullptr,out_combo=nullptr,gain_slider=nullptr,
     probability_slider=nullptr,floor_slider=nullptr,denoise_check=nullptr,
     vad_check=nullptr,status_text=nullptr,gain_value=nullptr,
     probability_value=nullptr,floor_value=nullptr,start_button=nullptr,stop_button=nullptr;
HWND voice_check=nullptr,pitch_slider=nullptr,resonance_slider=nullptr,
     bass_slider=nullptr,brightness_slider=nullptr,mix_slider=nullptr;
HWND pitch_value=nullptr,resonance_value=nullptr,bass_value=nullptr,
     brightness_value=nullptr,mix_value=nullptr;
HWND profile_combo=nullptr,profile_name=nullptr;
HFONT font=nullptr;
PROCESS_INFORMATION process{};
HANDLE stop_event=nullptr,job=nullptr,config_mapping=nullptr;
a50::LiveShared* shared_config=nullptr;
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
    SetWindowTextW(pitch_value,(std::to_wstring(slider(pitch_slider))+L" st").c_str());
    SetWindowTextW(resonance_value,
      (std::to_wstring(slider(resonance_slider))+L" %").c_str());
    SetWindowTextW(bass_value,(std::to_wstring(slider(bass_slider))+L" dB").c_str());
    SetWindowTextW(brightness_value,
      (std::to_wstring(slider(brightness_slider))+L" dB").c_str());
    SetWindowTextW(mix_value,(std::to_wstring(slider(mix_slider))+L" %").c_str());
}
void publish_live_settings() {
    if(!shared_config) return;
    a50::LiveValues v;
    v.gain_tenths=slider(gain_slider)*10;
    v.probability_milli=slider(probability_slider)*10;
    v.min_voice_db_tenths=slider(floor_slider)*10;
    v.voice_enabled=SendMessageW(voice_check,BM_GETCHECK,0,0)==BST_CHECKED?1:0;
    v.pitch_tenths=slider(pitch_slider)*10;
    v.resonance_milli=slider(resonance_slider)*10;
    v.bass_tenths=slider(bass_slider)*10;
    v.brightness_tenths=slider(brightness_slider)*10;
    v.mix_milli=slider(mix_slider)*10;
    a50::publish(shared_config,v);
}
std::filesystem::path profile_path() {
    wchar_t root[32768]{};
    const DWORD n=GetEnvironmentVariableW(L"APPDATA",root,32768);
    if(!n||n>=32768) throw std::runtime_error("APPDATA");
    const auto folder=std::filesystem::path(root)/L"A50VoiceClean";
    std::filesystem::create_directories(folder);
    return folder/L"live_profiles.ini";
}
std::array<HWND,8> profile_sliders() {
    return {gain_slider,probability_slider,floor_slider,pitch_slider,
            resonance_slider,bass_slider,brightness_slider,mix_slider};
}
a50::StoredProfile current_profile() {
    a50::StoredProfile p;
    const auto sliders=profile_sliders();
    for(std::size_t i=0;i<8;++i)p.sliders[i]=slider(sliders[i]);
    p.denoise=SendMessageW(denoise_check,BM_GETCHECK,0,0)==BST_CHECKED;
    p.vad=SendMessageW(vad_check,BM_GETCHECK,0,0)==BST_CHECKED;
    p.voice=SendMessageW(voice_check,BM_GETCHECK,0,0)==BST_CHECKED;
    return p;
}
void apply_profile(const a50::StoredProfile& p) {
    const auto handles=profile_sliders();
    constexpr std::array<int,8> low{-12,20,-80,-12,-100,-12,-12,0};
    constexpr std::array<int,8> high{24,95,-20,12,100,12,12,100};
    for(std::size_t i=0;i<8;++i)
        SendMessageW(handles[i],TBM_SETPOS,TRUE,std::clamp(p.sliders[i],low[i],high[i]));
    SendMessageW(denoise_check,BM_SETCHECK,p.denoise?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(vad_check,BM_SETCHECK,p.vad?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(voice_check,BM_SETCHECK,p.voice?BST_CHECKED:BST_UNCHECKED,0);
    values();publish_live_settings();
}
void refresh_profiles() {
    SendMessageW(profile_combo,CB_RESETCONTENT,0,0);
    for(const wchar_t* p:{L"Originale",L"Voce chiara",L"Voce femminile leggera"})
        SendMessageW(profile_combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(p));
    for(const auto& p:a50::list_profiles(profile_path()))
        SendMessageW(profile_combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(p.c_str()));
    SendMessageW(profile_combo,CB_SETCURSEL,0,0);
}
void load_selected_profile() {
    const int selected=static_cast<int>(SendMessageW(profile_combo,CB_GETCURSEL,0,0));
    if(selected<0)return;
    try {
        a50::StoredProfile p;
        if(selected<3) {
            p.voice=selected!=0;
            p.sliders={0,65,-46,selected==2?4:0,selected==2?35:(selected==1?20:0),
                       selected==2?-3:(selected==1?-2:0),
                       selected==2?2:(selected==1?4:0),selected==1?60:100};
        } else {
            wchar_t name[128]{};
            SendMessageW(profile_combo,CB_GETLBTEXT,selected,reinterpret_cast<LPARAM>(name));
            p=a50::load_profile_file(profile_path(),name);
        }
        apply_profile(p);
        message(L"Profilo caricato. Le caselle RNNoise/VAD richiedono riavvio.");
    } catch(...) {message(L"Impossibile leggere il profilo.");}
}
void save_current_profile() {
    wchar_t text[128]{};
    GetWindowTextW(profile_name,text,128);
    const std::wstring name(text);
    if(!a50::valid_profile_name(name)||name==L"Originale"||
       name==L"Voce chiara"||name==L"Voce femminile leggera") {
        MessageBoxW(window,L"Nome profilo non valido.",L"A50 VoiceClean",MB_OK|MB_ICONWARNING);
        return;
    }
    try {
        const auto path=profile_path();
        if(a50::profile_exists(path,name) &&
           MessageBoxW(window,L"Sovrascrivere il profilo esistente?",L"A50 VoiceClean",
                       MB_YESNO|MB_ICONQUESTION)!=IDYES) return;
        a50::save_profile_file(path,name,current_profile());
        refresh_profiles();
        const int i=static_cast<int>(SendMessageW(profile_combo,CB_FINDSTRINGEXACT,
                                        -1,reinterpret_cast<LPARAM>(name.c_str())));
        if(i>=0)SendMessageW(profile_combo,CB_SETCURSEL,i,0);
        message(L"Profilo salvato: "+name);
    } catch(...) {message(L"Impossibile salvare il profilo.");}
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
    if(shared_config) {UnmapViewOfFile(shared_config);shared_config=nullptr;}
    if(config_mapping) {CloseHandle(config_mapping);config_mapping=nullptr;}
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
        const std::wstring config_name=L"Local\\A50VoiceCleanConfig_"+
                                     std::to_wstring(GetCurrentProcessId());
        stop_event=CreateEventW(nullptr,TRUE,FALSE,event_name.c_str());
        if(!stop_event) {CloseHandle(logfile);CloseHandle(nul);throw std::runtime_error("Errore evento stop");}
        config_mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,
                                           0,sizeof(a50::LiveShared),config_name.c_str());
        if(!config_mapping) {
            CloseHandle(logfile);CloseHandle(nul);throw std::runtime_error("Errore memoria condivisa");
        }
        shared_config=static_cast<a50::LiveShared*>(MapViewOfFile(
                         config_mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(a50::LiveShared)));
        if(!shared_config) {
            CloseHandle(logfile);CloseHandle(nul);throw std::runtime_error("Errore mappatura");
        }
        publish_live_settings();
        std::wstring args=L"\""+executable.wstring()+L"\" --input "+
            std::to_wstring(input)+L" --output "+std::to_wstring(output)+
            L" --gain-db "+std::to_wstring(slider(gain_slider))+
            L" --vad-prob "+std::to_wstring(slider(probability_slider)/100.0)+
            L" --voice-min-db "+std::to_wstring(slider(floor_slider))+
            L" --stop-event \""+event_name+L"\" --config-map \""+config_name+L"\"";
        if(SendMessageW(denoise_check,BM_GETCHECK,0,0)!=BST_CHECKED) args+=L" --no-denoise";
        if(SendMessageW(vad_check,BM_GETCHECK,0,0)!=BST_CHECKED) args+=L" --no-vad";
        if(SendMessageW(voice_check,BM_GETCHECK,0,0)==BST_CHECKED) {
            args+=L" --voice --pitch "+std::to_wstring(slider(pitch_slider))+
                  L" --resonance "+std::to_wstring(slider(resonance_slider)/100.0)+
                  L" --bass-db "+std::to_wstring(slider(bass_slider))+
                  L" --brightness-db "+std::to_wstring(slider(brightness_slider))+
                  L" --voice-mix "+std::to_wstring(slider(mix_slider)/100.0);
        }
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
        control(L"STATIC",L"A50 VoiceClean | ASTRO A50 Gen 4 | Live",0,18,15,960,22);
        control(L"STATIC",L"Microfono in ingresso",0,20,56,240,20);
        in_combo=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,20,79,828,300,InputSelect);
        control(L"STATIC",L"Uscita audio virtuale (VB-CABLE)",0,20,127,380,20);
        out_combo=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,20,151,828,300,OutputSelect);
        control(L"BUTTON",L"Aggiorna",BS_PUSHBUTTON,868,79,120,29,Refresh);

        control(L"STATIC",L"Pulizia microfono",0,20,202,450,24);
        control(L"STATIC",L"Volume/guadagno voce",0,20,250,190,19);
        gain_slider=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS,215,242,290,35,GainSlider);
        gain_value=control(L"STATIC",L"",0,518,250,90,18);
        control(L"STATIC",L"Sensibilita VAD",0,20,310,190,19);
        probability_slider=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS,215,302,290,35,ProbabilitySlider);
        probability_value=control(L"STATIC",L"",0,518,310,90,18);
        control(L"STATIC",L"Soglia minima voce",0,20,370,190,19);
        floor_slider=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS,215,362,290,35,FloorSlider);
        floor_value=control(L"STATIC",L"",0,518,370,90,18);
        denoise_check=control(L"BUTTON",L"RNNoise (pulizia)",BS_AUTOCHECKBOX,20,423,185,24,DenoiseCheck);
        vad_check=control(L"BUTTON",L"Attiva solo con voce (VAD)",BS_AUTOCHECKBOX,245,423,255,24,VadCheck);
        SendMessageW(denoise_check,BM_SETCHECK,BST_CHECKED,0);
        SendMessageW(vad_check,BM_SETCHECK,BST_CHECKED,0);
        set_slider(gain_slider,-12,24,0);
        set_slider(probability_slider,20,95,65);
        set_slider(floor_slider,-80,-20,-46);

        control(L"STATIC",L"Modificatore vocale leggero (sperimentale)",0,624,202,380,24);
        voice_check=control(L"BUTTON",L"Attiva trasformazione",BS_AUTOCHECKBOX,624,232,245,22,VoiceCheck);
        control(L"STATIC",L"Tonalita",0,624,277,110,18);
        pitch_slider=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS,742,270,162,31,PitchSlider);
        pitch_value=control(L"STATIC",L"",0,916,277,80,18);
        control(L"STATIC",L"Timbro",0,624,320,110,18);
        resonance_slider=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS,742,313,162,31,ResonanceSlider);
        resonance_value=control(L"STATIC",L"",0,916,320,80,18);
        control(L"STATIC",L"Bassi",0,624,363,110,18);
        bass_slider=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS,742,356,162,31,BassSlider);
        bass_value=control(L"STATIC",L"",0,916,363,80,18);
        control(L"STATIC",L"Brillantezza",0,624,406,110,18);
        brightness_slider=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS,742,399,162,31,BrightnessSlider);
        brightness_value=control(L"STATIC",L"",0,916,406,80,18);
        control(L"STATIC",L"Intensita",0,624,449,110,18);
        mix_slider=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS,742,442,162,31,MixSlider);
        mix_value=control(L"STATIC",L"",0,916,449,80,18);
        set_slider(pitch_slider,-12,12,4);
        set_slider(resonance_slider,-100,100,35);
        set_slider(bass_slider,-12,12,-3);
        set_slider(brightness_slider,-12,12,2);
        set_slider(mix_slider,0,100,100);
        values();
        start_button=control(L"BUTTON",L"Avvia",BS_PUSHBUTTON,20,483,130,34,Start);
        stop_button=control(L"BUTTON",L"Ferma",BS_PUSHBUTTON,165,483,130,34,Stop);
        EnableWindow(stop_button,FALSE);
        control(L"STATIC",L"Profilo",0,316,492,72,18);
        profile_combo=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,390,483,182,220,ProfileCombo);
        control(L"BUTTON",L"Carica",BS_PUSHBUTTON,580,483,74,29,LoadProfile);
        profile_name=control(L"EDIT",L"Nome profilo",WS_BORDER|ES_AUTOHSCROLL,668,486,165,23,ProfileName);
        control(L"BUTTON",L"Salva",BS_PUSHBUTTON,843,483,105,29,SaveProfile);
        try{refresh_profiles();}catch(...){message(L"Errore lettura profili.");}
        control(L"STATIC",L"Cursori regolabili in diretta. RNNoise/VAD richiedono riavvio. Velocita live invariata.",0,20,528,955,22);
        status_text=control(L"STATIC",L"Caricamento dispositivi...",0,20,557,955,35);
        try {populate();} catch(...) {message(L"Impossibile leggere i dispositivi audio.");}
        SetTimer(h,1,1000,nullptr);
        return 0;
    }
    case WM_HSCROLL: values();publish_live_settings();return 0;
    case WM_COMMAND:
        switch(LOWORD(w)) {
        case Refresh: if(!running()){try{populate();}catch(...){message(L"Errore lettura dispositivi.");}}return 0;
        case Start: start_engine();return 0;
        case Stop: stop_engine();return 0;
        case VoiceCheck: publish_live_settings();return 0;
        case LoadProfile: load_selected_profile();return 0;
        case SaveProfile: save_current_profile();return 0;
        case DenoiseCheck: case VadCheck:
            if(running()) message(L"Per cambiare RNNoise o VAD premi Ferma e poi Avvia.");
            return 0;
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
        CW_USEDEFAULT,CW_USEDEFAULT,1035,651,
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
