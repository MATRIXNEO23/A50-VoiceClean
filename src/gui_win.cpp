// Win32 offline WAV editor. Not a live microphone; WASAPI is a future milestone.
#include "transform.h"
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <cwchar>

namespace {
constexpr int OpenButton=100, PreviewButton=101, SaveButton=102;
constexpr int GateCheck=103, VoiceCheck=104, ProfileCombo=105, SaveProfileButton=106;
constexpr int ProfileNameEdit=107, SliderStart=200;
constexpr std::array<const wchar_t*,8> names = {
    L"Guadagno voce", L"Soglia rumore", L"Tonalita (semitoni)", L"Timbro / risonanza",
    L"Bassi", L"Brillantezza", L"Intensita effetto", L"Velocita registrazione"
};
constexpr std::array<int,8> mins={-12,-75,-12,-100,-12,-12,0,70};
constexpr std::array<int,8> maxs={24,-15,12,100,12,12,100,130};
constexpr std::array<int,8> defaults={0,-38,0,0,0,0,100,100};
HWND main_window=nullptr, status_label=nullptr, gate_check=nullptr, voice_check=nullptr;
HWND profile_combo=nullptr, profile_edit=nullptr;
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
    if(n.empty() || n.find_first_of(L"[]=\\r\\n;")!=std::wstring::npos ||
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
        control(L"STATIC",L"A50 VoiceClean | Editor WAV v0.2",0,18,10,460,24);
        control(L"STATIC",L"Per ora: registrazioni WAV mono PCM16; NON modifica il microfono in diretta.",0,18,32,650,20);
        control(L"BUTTON",L"Apri WAV",BS_PUSHBUTTON,18,58,105,28,OpenButton);
        control(L"BUTTON",L"Ascolta 10 s",BS_PUSHBUTTON,132,58,116,28,PreviewButton);
        control(L"BUTTON",L"Salva WAV",BS_PUSHBUTTON,256,58,116,28,SaveButton);
        gate_check=control(L"BUTTON",L"Riduzione rumori (gate)",BS_AUTOCHECKBOX,390,60,205,22,GateCheck);
        voice_check=control(L"BUTTON",L"Modifica voce",BS_AUTOCHECKBOX,595,60,126,22,VoiceCheck);
        for(int i=0;i<8;++i) {
            const int y=104+i*53;
            control(L"STATIC",names[i],0,20,y,230,18);
            sliders[i]=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS,238,y-6,368,33,SliderStart+i);
            SendMessageW(sliders[i],TBM_SETRANGEMIN,FALSE,mins[i]);
            SendMessageW(sliders[i],TBM_SETRANGEMAX,FALSE,maxs[i]);
            values[i]=control(L"STATIC",L"",0,615,y,110,20);
        }
        control(L"STATIC",L"Profili",0,20,535,95,20);
        profile_combo=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,102,530,220,220,ProfileCombo);
        control(L"STATIC",L"Nuovo nome",0,333,535,92,20);
        profile_edit=control(L"EDIT",L"Mio profilo",WS_BORDER|ES_AUTOHSCROLL,424,531,150,24,ProfileNameEdit);
        control(L"BUTTON",L"Salva profilo",BS_PUSHBUTTON,583,529,138,27,SaveProfileButton);
        status_label=control(L"STATIC",L"Apri un file WAV per iniziare.",0,20,576,695,40);
        fill_profiles();set_profile(0);
        return 0;
    }
    case WM_HSCROLL: update_values();return 0;
    case WM_COMMAND:
        switch(LOWORD(wp)) {
        case OpenButton: input_path=choose_file(false); if(!input_path.empty()) status(L"File: "+input_path.filename().wstring());return 0;
        case PreviewButton: process_wav(true);return 0;
        case SaveButton: process_wav(false);return 0;
        case SaveProfileButton: save_profile();return 0;
        case ProfileCombo: if(HIWORD(wp)==CBN_SELCHANGE) load_named_profile();return 0;
        }
        return 0;
    case WM_DESTROY:
        PlaySoundW(nullptr,nullptr,0);
        if(!preview_path.empty()) {std::error_code ec;std::filesystem::remove(preview_path,ec);}
        PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(w,m,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_BAR_CLASSES};InitCommonControlsEx(&cc);
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=instance;wc.lpszClassName=L"A50VoiceCleanOffline";
    wc.lpfnWndProc=proc;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    RegisterClassExW(&wc);
    HWND w=CreateWindowExW(0,wc.lpszClassName,L"A50 VoiceClean - Editor vocale",WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME,
      CW_USEDEFAULT,CW_USEDEFAULT,758,669,nullptr,nullptr,instance,nullptr);
    if(!w)return 1;
    ShowWindow(w,show);UpdateWindow(w);
    MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    return static_cast<int>(msg.wParam);
}
