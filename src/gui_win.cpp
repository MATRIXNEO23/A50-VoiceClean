// Win32 editor with first-stage WASAPI capture and level meter.
#include "transform.h"
#include "live_capture_win.h"
#include <windows.h>
#include <objbase.h>
#include <commctrl.h>
#include <cwctype>
#include <memory>
#include <commdlg.h>
#include <mmsystem.h>
#include <algorithm>
#include <array>
#include <initializer_list>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <cwchar>

namespace {
constexpr int OpenButton=100, PreviewButton=101, SaveButton=102;
constexpr int GateCheck=103, VoiceCheck=104, ProfileCombo=105, SaveProfileButton=106;
constexpr int ProfileNameEdit=107, DeviceCombo=108, DeviceRefresh=109, LiveToggle=110;
constexpr int CaptureTimer=1, SliderStart=200;
constexpr std::array<const wchar_t*,8> names = {
    L"Guadagno voce", L"Soglia rumore", L"Tonalita (semitoni)", L"Timbro / risonanza",
    L"Bassi", L"Brillantezza", L"Intensita effetto", L"Velocita registrazione"
};
constexpr std::array<int,8> mins={-12,-75,-12,-100,-12,-12,0,70};
constexpr std::array<int,8> maxs={24,-15,12,100,12,12,100,130};
constexpr std::array<int,8> defaults={0,-38,0,0,0,0,100,100};
HWND main_window=nullptr, status_label=nullptr, gate_check=nullptr, voice_check=nullptr;
HWND profile_combo=nullptr, profile_edit=nullptr;
HWND device_combo=nullptr, device_toggle=nullptr, device_meter=nullptr, device_status=nullptr;
std::vector<a50::InputDevice> input_devices;
a50::LiveCapture live_capture;
a50::CaptureStatus previous_capture_status = a50::CaptureStatus::stopped;
std::array<HWND,8> sliders{};
std::array<HWND,8> values{};
std::filesystem::path input_path, preview_path;
HFONT font=nullptr;
std::wstring ini_path() {
    wchar_t base[MAX_PATH]{};
    DWORD n=GetEnvironmentVariableW(L"APPDATA",base,MAX_PATH);
    std::filesystem::path p;
    if(n && n<MAX_PATH) p=base;
    else p=std::filesystem::temp_directory_path();
    p/=L"A50VoiceClean";
    std::error_code ec;
    std::filesystem::create_directories(p,ec);
    return (p/L"profiles.ini").wstring();
}
void set_font(HWND w) { SendMessageW(w,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE); }
HWND control(const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id=0) {
    HWND h=CreateWindowExW(0,cls,text,style|WS_CHILD|WS_VISIBLE,x,y,width,height,
      main_window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    set_font(h);
    return h;
}
void status(const std::wstring& s) { SetWindowTextW(status_label,s.c_str()); }
int get_slider(std::size_t i) { return static_cast<int>(SendMessageW(sliders[i],TBM_GETPOS,0,0)); }
void update_values() {
    for(std::size_t i=0;i<sliders.size();++i) {
        std::wstring value=std::to_wstring(get_slider(i));
        if(i==3 || i==6 || i==7) value+=L" %";
        if(i==0 || i==1 || i==4 || i==5) value+=L" dB";
        SetWindowTextW(values[i],value.c_str());
    }
}
void set_slider(std::size_t i,int n) { SendMessageW(sliders[i],TBM_SETPOS,TRUE,std::clamp(n,mins[i],maxs[i])); }
a50::Settings current_settings() {
    a50::Settings s;
    s.gain_db=static_cast<float>(get_slider(0));
    s.threshold_dbfs=static_cast<float>(get_slider(1));
    s.pitch_semitones=static_cast<float>(get_slider(2));
    s.resonance=get_slider(3)/100.0f;
    s.bass_db=static_cast<float>(get_slider(4));
    s.brightness_db=static_cast<float>(get_slider(5));
    s.effect_mix=get_slider(6)/100.0f;
    s.gate_enabled=SendMessageW(gate_check,BM_GETCHECK,0,0)==BST_CHECKED;
    s.voice_enabled=SendMessageW(voice_check,BM_GETCHECK,0,0)==BST_CHECKED;
    return s;
}
void set_profile(int p) {
    for(std::size_t i=0;i<sliders.size();++i) set_slider(i,defaults[i]);
    SendMessageW(gate_check,BM_SETCHECK,BST_CHECKED,0);
    SendMessageW(voice_check,BM_SETCHECK,p==0?BST_UNCHECKED:BST_CHECKED,0);
    if(p==1) { set_slider(2,4);set_slider(3,35);set_slider(4,-3);set_slider(5,2);set_slider(6,100); }
    else if(p==2) { set_slider(2,7);set_slider(3,60);set_slider(4,-6);set_slider(5,4);set_slider(6,100); }
    update_values();
}
void fill_profiles() {
    SendMessageW(profile_combo,CB_RESETCONTENT,0,0);
    for(auto x:{L"Originale",L"Voce piu chiara",L"Voce piu acuta"})
        SendMessageW(profile_combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(x));
    wchar_t buf[8192]{};
    const auto path=ini_path();
    GetPrivateProfileSectionNamesW(buf,8192,path.c_str());
    for(wchar_t* p=buf;*p;p+=std::wcslen(p)+1)
        SendMessageW(profile_combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(p));
    SendMessageW(profile_combo,CB_SETCURSEL,0,0);
}
void load_named_profile() {
    wchar_t name[200]{};
    const int index=static_cast<int>(SendMessageW(profile_combo,CB_GETCURSEL,0,0));
    if(index<0) return;
    if(index<3) { set_profile(index); return; }
    SendMessageW(profile_combo,CB_GETLBTEXT,index,reinterpret_cast<LPARAM>(name));
    const auto file=ini_path();
    for(int i=0;i<8;++i) {
        const auto key=L"slider"+std::to_wstring(i);
        set_slider(i,GetPrivateProfileIntW(name,key.c_str(),defaults[i],file.c_str()));
    }
    SendMessageW(gate_check,BM_SETCHECK,GetPrivateProfileIntW(name,L"gate",1,file.c_str())?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(voice_check,BM_SETCHECK,GetPrivateProfileIntW(name,L"voice",1,file.c_str())?BST_CHECKED:BST_UNCHECKED,0);
    update_values();
}
void save_profile() {
    wchar_t name[100]{};
    GetWindowTextW(profile_edit,name,100);
    std::wstring n(name);
    if(n.empty() || n.find_first_of(L"[]=\r\n;")!=std::wstring::npos ||
       n==L"Originale" || n==L"Voce piu chiara" || n==L"Voce piu acuta") {
        MessageBoxW(main_window,L"Inserisci un nome profilo valido e diverso dai preset.",L"Profilo",MB_OK|MB_ICONWARNING);
        return;
    }
    const auto file=ini_path();
    for(int i=0;i<8;++i) {
        const auto key=L"slider"+std::to_wstring(i);
        const auto val=std::to_wstring(get_slider(i));
        WritePrivateProfileStringW(n.c_str(),key.c_str(),val.c_str(),file.c_str());
    }
    WritePrivateProfileStringW(n.c_str(),L"gate",SendMessageW(gate_check,BM_GETCHECK,0,0)==BST_CHECKED?L"1":L"0",file.c_str());
    WritePrivateProfileStringW(n.c_str(),L"voice",SendMessageW(voice_check,BM_GETCHECK,0,0)==BST_CHECKED?L"1":L"0",file.c_str());
    fill_profiles();
    const int i=static_cast<int>(SendMessageW(profile_combo,CB_FINDSTRINGEXACT,-1,reinterpret_cast<LPARAM>(n.c_str())));
    if(i>=0) SendMessageW(profile_combo,CB_SETCURSEL,i,0);
    status(L"Profilo salvato: "+n);
}
void refresh_microphones() {
    if (live_capture.status() == a50::CaptureStatus::running ||
        live_capture.status() == a50::CaptureStatus::starting) {
        SetWindowTextW(device_status,L"Ferma l'acquisizione prima di aggiornare la lista.");
        return;
    }
    SendMessageW(device_combo,CB_RESETCONTENT,0,0);
    input_devices.clear();
    try {
        input_devices = a50::enumerate_input_devices();
        int prefer = 0;
        for (std::size_t i=0; i<input_devices.size(); ++i) {
            const auto& d = input_devices[i];
            SendMessageW(device_combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(d.name.c_str()));
            std::wstring name = d.name;
            std::transform(name.begin(),name.end(),name.begin(),
                           [](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
            if (name.find(L"astro a50 voice")!=std::wstring::npos)
                prefer=static_cast<int>(i);
        }
        if (!input_devices.empty()) {
            SendMessageW(device_combo,CB_SETCURSEL,prefer,0);
            SetWindowTextW(device_status,L"Ingresso selezionato. Premi Avvia per controllare il livello voce.");
        } else {
            SetWindowTextW(device_status,L"Nessun microfono attivo trovato.");
        }
    } catch (...) {
        SetWindowTextW(device_status,L"Errore: impossibile leggere i microfoni di Windows.");
    }
}
void toggle_microphone() {
    const auto capture_state = live_capture.status();
    if (capture_state == a50::CaptureStatus::running ||
        capture_state == a50::CaptureStatus::starting) {
        live_capture.stop();
        SetWindowTextW(device_status,L"Acquisizione microfono interrotta.");
        previous_capture_status=a50::CaptureStatus::stopped;
        SetWindowTextW(device_toggle,L"Avvia");
        SendMessageW(device_meter,PBM_SETPOS,0,0);
        return;
    }
    const int index=static_cast<int>(SendMessageW(device_combo,CB_GETCURSEL,0,0));
    if(index<0 || static_cast<std::size_t>(index)>=input_devices.size()) {
        SetWindowTextW(device_status,L"Seleziona prima un microfono.");
        return;
    }
    try {
        live_capture.start(input_devices[static_cast<std::size_t>(index)].id);
        SetWindowTextW(device_toggle,L"Ferma");
        SetWindowTextW(device_status,L"Avvio acquisizione...");
    } catch (...) {
        SetWindowTextW(device_status,L"Errore nell'avvio del thread audio.");
    }
}
void refresh_meter() {
    const auto state=live_capture.status();
    const auto level=state==a50::CaptureStatus::running
        ? live_capture.meter_percent() : 0;
    SendMessageW(device_meter,PBM_SETPOS,level,0);
    if(state!=previous_capture_status) {
        previous_capture_status=state;
        if(state==a50::CaptureStatus::running) {
            SetWindowTextW(device_status,L"Microfono attivo: livello acquisito, senza uscita audio.");
            SetWindowTextW(device_toggle,L"Ferma");
        } else if(state==a50::CaptureStatus::failed) {
            wchar_t error[100]{};
            std::swprintf(error,100,L"Errore WASAPI 0x%08lX. Riprova o cambia ingresso.",
                         static_cast<unsigned long>(live_capture.error()));
            SetWindowTextW(device_status,error);
            SetWindowTextW(device_toggle,L"Avvia");
        }
    }
}
std::filesystem::path choose_file(bool save) {
    wchar_t path[32768]{};
    if(save && !input_path.empty()) wcscpy_s(path,32768,L"voce_modificata.wav");
    OPENFILENAMEW ofn{};
    ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=main_window;
    ofn.lpstrFilter=L"Audio WAV mono PCM16\0*.wav\0Tutti i file\0*.*\0\0";
    ofn.lpstrFile=path;ofn.nMaxFile=32768;
    ofn.Flags=OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    ofn.lpstrDefExt=L"wav";
    if(save ? GetSaveFileNameW(&ofn):GetOpenFileNameW(&ofn)) return std::filesystem::path(path);
    return {};
}
void process_wav(bool preview) {
    if(input_path.empty()) {status(L"Apri prima una registrazione WAV.");return;}
    PlaySoundW(nullptr,nullptr,0);
    try {
        status(L"Elaborazione in corso...");UpdateWindow(main_window);
        auto wave=a50::read_pcm16_mono_wave(input_path);
        if(preview && wave.samples.size()>10ULL*wave.sample_rate)
            wave.samples.resize(10ULL*wave.sample_rate);
        auto result=a50::transform(std::move(wave),current_settings(),get_slider(7)/100.f);
        std::filesystem::path out;
        if(preview) {
            if(preview_path.empty()) preview_path=std::filesystem::temp_directory_path()/
                (L"A50VoiceClean-preview-"+std::to_wstring(GetCurrentProcessId())+L".wav");
            out=preview_path;
        } else {
            out=choose_file(true);
            if(out.empty()){status(L"Salvataggio annullato.");return;}
            if(std::filesystem::absolute(input_path).lexically_normal()==
               std::filesystem::absolute(out).lexically_normal()){
                status(L"Scegli un nome diverso dall'originale.");return;
            }
        }
        a50::write_pcm16_mono_wave(out,result);
        if(preview) {
            if(!PlaySoundW(out.c_str(),nullptr,SND_FILENAME|SND_ASYNC|SND_NODEFAULT))
                throw std::runtime_error("Playback WAV failed");
            status(L"Anteprima: primi 10 secondi. Volume cuffie basso per sicurezza.");
        } else status(L"File elaborato e salvato.");
    } catch(const std::exception& e) {
        status(L"Errore: verifica il file WAV mono PCM16.");
        MessageBoxA(main_window,e.what(),"A50 VoiceClean",MB_OK|MB_ICONERROR);
    }
}
LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp) {
    switch(m) {
    case WM_CREATE: {
        main_window=w;
        font=static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        control(L"STATIC",L"A50 VoiceClean | Acquisizione microfono + Editor WAV",0,18,10,680,24);
        control(L"STATIC",L"Ingresso microfono",0,18,40,154,20);
        device_combo=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,165,35,326,250,DeviceCombo);
        control(L"BUTTON",L"Aggiorna",BS_PUSHBUTTON,500,35,98,28,DeviceRefresh);
        device_toggle=control(L"BUTTON",L"Avvia",BS_PUSHBUTTON,610,35,111,28,LiveToggle);
        device_meter=control(PROGRESS_CLASSW,L"",PBS_SMOOTH,18,73,703,18);
        SendMessageW(device_meter,PBM_SETRANGE32,0,100);
        device_status=control(L"STATIC",L"Seleziona un microfono.",0,18,101,704,23);
        control(L"STATIC",L"Acquisizione live: solo misurazione. Filtro e uscita virtuale non ancora attivi.",0,18,126,704,20);
        control(L"BUTTON",L"Apri WAV",BS_PUSHBUTTON,18,152,105,28,OpenButton);
        control(L"BUTTON",L"Ascolta 10 s",BS_PUSHBUTTON,132,152,116,28,PreviewButton);
        control(L"BUTTON",L"Salva WAV",BS_PUSHBUTTON,256,152,116,28,SaveButton);
        gate_check=control(L"BUTTON",L"Riduzione rumori (gate)",BS_AUTOCHECKBOX,390,154,205,22,GateCheck);
        voice_check=control(L"BUTTON",L"Modifica voce",BS_AUTOCHECKBOX,595,154,126,22,VoiceCheck);
        for(int i=0;i<8;++i) {
            const int y=195+i*48;
            control(L"STATIC",names[i],0,20,y,230,18);
            sliders[i]=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS,238,y-6,368,33,SliderStart+i);
            SendMessageW(sliders[i],TBM_SETRANGEMIN,FALSE,mins[i]);
            SendMessageW(sliders[i],TBM_SETRANGEMAX,FALSE,maxs[i]);
            values[i]=control(L"STATIC",L"",0,615,y,110,20);
        }
        control(L"STATIC",L"Profili",0,20,589,95,20);
        profile_combo=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,102,585,220,200,ProfileCombo);
        control(L"STATIC",L"Nuovo nome",0,333,590,92,20);
        profile_edit=control(L"EDIT",L"Mio profilo",WS_BORDER|ES_AUTOHSCROLL,424,586,150,24,ProfileNameEdit);
        control(L"BUTTON",L"Salva profilo",BS_PUSHBUTTON,583,585,138,27,SaveProfileButton);
        status_label=control(L"STATIC",L"Editor WAV: apri una registrazione.",0,20,630,695,40);
        fill_profiles();set_profile(0);
        refresh_microphones();
        SetTimer(w,CaptureTimer,60,nullptr);
        return 0;
    }
    case WM_HSCROLL: update_values();return 0;
    case WM_TIMER: if(wp==CaptureTimer) refresh_meter();return 0;
    case WM_COMMAND:
        switch(LOWORD(wp)) {
        case OpenButton: input_path=choose_file(false); if(!input_path.empty()) status(L"File: "+input_path.filename().wstring());return 0;
        case PreviewButton: process_wav(true);return 0;
        case SaveButton: process_wav(false);return 0;
        case SaveProfileButton: save_profile();return 0;
        case DeviceRefresh: refresh_microphones();return 0;
        case LiveToggle: toggle_microphone();return 0;
        case ProfileCombo: if(HIWORD(wp)==CBN_SELCHANGE) load_named_profile();return 0;
        }
        return 0;
    case WM_DESTROY:
        KillTimer(w,CaptureTimer);
        live_capture.stop();
        PlaySoundW(nullptr,nullptr,0);
        if(!preview_path.empty()) {std::error_code ec;std::filesystem::remove(preview_path,ec);}
        PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(w,m,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(com)) return 1;
    INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_BAR_CLASSES|ICC_PROGRESS_CLASS};InitCommonControlsEx(&cc);
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=instance;wc.lpszClassName=L"A50VoiceCleanOffline";
    wc.lpfnWndProc=proc;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    RegisterClassExW(&wc);
    HWND w=CreateWindowExW(0,wc.lpszClassName,L"A50 VoiceClean - Editor vocale",WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME,
      CW_USEDEFAULT,CW_USEDEFAULT,758,717,nullptr,nullptr,instance,nullptr);
    if(!w){CoUninitialize();return 1;}
    ShowWindow(w,show);UpdateWindow(w);
    MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
