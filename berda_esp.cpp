// BERDA Survival - simple IL2CPP ESP (x64)
// Inject with Extreme Injector. F8 = toggle, End = unload.

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

typedef void* tDomain;
typedef void* tAssembly;
typedef void* tImage;
typedef void* tClass;
typedef void* tObject;
typedef void* tType;
typedef void* tMethod;
typedef void* tString;
typedef void* tException;

typedef tDomain     (*fn_domain_get)();
typedef tDomain     (*fn_thread_attach)(tDomain);
typedef tAssembly*  (*fn_domain_get_assemblies)(tDomain, size_t*);
typedef tImage      (*fn_assembly_get_image)(tAssembly);
typedef const char* (*fn_image_get_name)(tImage);
typedef tClass      (*fn_class_from_name)(tImage, const char*, const char*);
typedef const char* (*fn_class_get_name)(tClass);
typedef tMethod     (*fn_class_get_method_from_name)(tClass, const char*, int);
typedef tType       (*fn_class_get_type)(tClass);
typedef tObject     (*fn_type_get_object)(tType);
typedef tObject     (*fn_runtime_invoke)(tMethod, tObject, void**, tException*);
typedef tObject     (*fn_object_unbox)(tObject);
typedef tClass      (*fn_object_get_class)(tObject);

static fn_domain_get                 p_domain_get;
static fn_thread_attach              p_thread_attach;
static fn_domain_get_assemblies      p_domain_get_assemblies;
static fn_assembly_get_image         p_assembly_get_image;
static fn_image_get_name             p_image_get_name;
static fn_class_from_name            p_class_from_name;
static fn_class_get_name             p_class_get_name;
static fn_class_get_method_from_name p_class_get_method_from_name;
static fn_class_get_type             p_class_get_type;
static fn_type_get_object            p_type_get_object;
static fn_runtime_invoke             p_runtime_invoke;
static fn_object_unbox               p_object_unbox;
static fn_object_get_class           p_object_get_class;

static bool  g_run = true;
static bool  g_esp = true;
static HWND  g_wnd = nullptr;
static int   g_w = 0;
static int   g_h = 0;

static tClass K_CAMERA = nullptr;
static tClass K_TRANSFORM = nullptr;
static tClass K_COMPONENT = nullptr;
static tClass K_OBJECT = nullptr;
static tClass K_MONO = nullptr;

static tMethod M_CAM_MAIN = nullptr;
static tMethod M_W2S = nullptr;
static tMethod M_FIND = nullptr;
static tMethod M_GET_TRANSFORM = nullptr;
static tMethod M_GET_POSITION = nullptr;

struct Vec3 { float x, y, z; };

struct Target {
    std::string name;
    float dist;
    float hx, hy;
    float fx, fy;
};

static std::vector<Target> g_targets;

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static tObject Call(tMethod m, tObject obj, void** args) {
    tException exc = nullptr;
    return p_runtime_invoke(m, obj, args, &exc);
}

static Vec3 GetPosition(tObject component) {
    Vec3 v = { 0.0f, 0.0f, 0.0f };
    if (!M_GET_TRANSFORM || !M_GET_POSITION) return v;
    tObject tr = Call(M_GET_TRANSFORM, component, nullptr);
    if (!tr) return v;
    tObject boxed = Call(M_GET_POSITION, tr, nullptr);
    if (!boxed) return v;
    float* f = (float*)p_object_unbox(boxed);
    if (!f) return v;
    v.x = f[0]; v.y = f[1]; v.z = f[2];
    return v;
}

static std::string ReadName(tObject obj) {
    if (!obj || !p_class_get_name) return "";
    tClass k = p_object_get_class(obj);
    if (!k) return "";
    const char* n = p_class_get_name(k);
    return n ? std::string(n) : std::string();
}

static bool IsTargetClass(tClass k) {
    if (!k) return false;
    std::string n = ToLower(p_class_get_name(k));
    static const char* keys[] = {
        "player", "character", "unit", "bot", "enemy",
        "mob", "actor", "humanoid", "survivor", "zombie"
    };
    for (int i = 0; i < 10; i++) {
        if (n.find(keys[i]) != std::string::npos) return true;
    }
    return false;
}

static bool InitIl2cpp() {
    HMODULE ga = GetModuleHandleA("GameAssembly.dll");
    if (!ga) return false;

    p_domain_get                 = (fn_domain_get)GetProcAddress(ga, "il2cpp_domain_get");
    p_thread_attach              = (fn_thread_attach)GetProcAddress(ga, "il2cpp_thread_attach");
    p_domain_get_assemblies      = (fn_domain_get_assemblies)GetProcAddress(ga, "il2cpp_domain_get_assemblies");
    p_assembly_get_image         = (fn_assembly_get_image)GetProcAddress(ga, "il2cpp_assembly_get_image");
    p_image_get_name             = (fn_image_get_name)GetProcAddress(ga, "il2cpp_image_get_name");
    p_class_from_name            = (fn_class_from_name)GetProcAddress(ga, "il2cpp_class_from_name");
    p_class_get_name             = (fn_class_get_name)GetProcAddress(ga, "il2cpp_class_get_name");
    p_class_get_method_from_name = (fn_class_get_method_from_name)GetProcAddress(ga, "il2cpp_class_get_method_from_name");
    p_class_get_type             = (fn_class_get_type)GetProcAddress(ga, "il2cpp_class_get_type");
    p_type_get_object            = (fn_type_get_object)GetProcAddress(ga, "il2cpp_type_get_object");
    p_runtime_invoke             = (fn_runtime_invoke)GetProcAddress(ga, "il2cpp_runtime_invoke");
    p_object_unbox               = (fn_object_unbox)GetProcAddress(ga, "il2cpp_object_unbox");
    p_object_get_class           = (fn_object_get_class)GetProcAddress(ga, "il2cpp_object_get_class");

    if (!p_domain_get || !p_runtime_invoke || !p_class_from_name) return false;

    tDomain d = p_domain_get();
    if (p_thread_attach) p_thread_attach(d);

    size_t n = 0;
    tAssembly* asms = p_domain_get_assemblies ? p_domain_get_assemblies(d, &n) : nullptr;
    if (!asms) return false;

    tImage core = nullptr;
    for (size_t i = 0; i < n; i++) {
        tImage img = p_assembly_get_image(asms[i]);
        std::string nm = p_image_get_name(img);
        if (nm == "UnityEngine.CoreModule.dll") { core = img; break; }
    }
    if (!core) return false;

    K_CAMERA    = p_class_from_name(core, "UnityEngine", "Camera");
    K_TRANSFORM = p_class_from_name(core, "UnityEngine", "Transform");
    K_COMPONENT = p_class_from_name(core, "UnityEngine", "Component");
    K_OBJECT    = p_class_from_name(core, "UnityEngine", "Object");
    K_MONO      = p_class_from_name(core, "UnityEngine", "MonoBehaviour");
    if (!K_CAMERA || !K_OBJECT) return false;

    M_CAM_MAIN      = p_class_get_method_from_name(K_CAMERA, "get_main", 0);
    M_W2S           = p_class_get_method_from_name(K_CAMERA, "WorldToViewportPoint", 1);
    M_FIND          = p_class_get_method_from_name(K_OBJECT, "FindObjectsOfType", 1);
    M_GET_TRANSFORM = p_class_get_method_from_name(K_COMPONENT, "get_transform", 0);
    M_GET_POSITION  = p_class_get_method_from_name(K_TRANSFORM, "get_position", 0);

    return M_CAM_MAIN && M_W2S && M_FIND && M_GET_TRANSFORM && M_GET_POSITION;
}
static void CollectTargets(tObject cam) {
    g_targets.clear();
    if (!cam || !K_MONO || !M_FIND) return;

    tType monoType = p_class_get_type(K_MONO);
    tObject typeObj = p_type_get_object(monoType);
    if (!typeObj) return;

    void* args[1] = { typeObj };
    tObject arr = Call(M_FIND, nullptr, args);
    if (!arr) return;

    int len = *(int*)((char*)arr + 0x18);
    if (len <= 0 || len > 20000) return;
    void** data = (void**)((char*)arr + 0x20);

    Vec3 camPos = GetPosition(cam);

    for (int i = 0; i < len; i++) {
        tObject obj = data[i];
        if (!obj) continue;

        tClass k = p_object_get_class(obj);
        if (!IsTargetClass(k)) continue;

        Vec3 p = GetPosition(obj);
        if (p.x == 0.0f && p.y == 0.0f && p.z == 0.0f) continue;

        float dx = p.x - camPos.x;
        float dy = p.y - camPos.y;
        float dz = p.z - camPos.z;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        if (dist < 1.5f || dist > 500.0f) continue;

        float hb[3] = { p.x, p.y + 1.8f, p.z };
        void* a1[1] = { hb };
        tObject bh = Call(M_W2S, cam, a1);
        if (!bh) continue;
        float* hv = (float*)p_object_unbox(bh);
        if (!hv || hv[2] <= 0.0f) continue;

        float fb[3] = { p.x, p.y, p.z };
        void* a2[1] = { fb };
        tObject bf = Call(M_W2S, cam, a2);
        if (!bf) continue;
        float* fv = (float*)p_object_unbox(bf);
        if (!fv || fv[2] <= 0.0f) continue;

        Target t;
        t.dist = dist;
        t.hx = hv[0]; t.hy = hv[1];
        t.fx = fv[0]; t.fy = fv[1];
        t.name = ReadName(obj);
        if (t.name.empty()) t.name = "unit";

        g_targets.push_back(t);
    }
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY) { g_run = false; return 0; }
    return DefWindowProcA(h, m, w, l);
}

static void DrawOverlay(HDC screen) {
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, g_w, g_h);
    HGDIOBJ oldBmp = SelectObject(mem, bmp);

    RECT full = { 0, 0, g_w, g_h };
    HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(mem, &full, bg);
    DeleteObject(bg);

    HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 150, 255));
    HGDIOBJ oldPen = SelectObject(mem, pen);
    HGDIOBJ oldBr = SelectObject(mem, GetStockObject(NULL_BRUSH));

    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, RGB(200, 220, 255));

    for (size_t i = 0; i < g_targets.size(); i++) {
        const Target& t = g_targets[i];
        int x1 = (int)t.hx;
        int y1 = (int)(g_h - t.hy);
        int x2 = (int)t.fx;
        int y2 = (int)(g_h - t.fy);
        if (x1 > x2) { int tmp = x1; x1 = x2; x2 = tmp; }
        if (y1 > y2) { int tmp = y1; y1 = y2; y2 = tmp; }
        if (x2 - x1 < 6 || y2 - y1 < 6) continue;

        Rectangle(mem, x1, y1, x2, y2);

        char buf[160];
        sprintf(buf, "%s [%dm]", t.name.c_str(), (int)t.dist);
        TextOutA(mem, x1, y1 - 16, buf, (int)strlen(buf));
    }

    char info[160];
    sprintf(info, "BERDA ESP %s  targets: %d  [F8]", g_esp ? "ON" : "OFF", (int)g_targets.size());
    SetTextColor(mem, RGB(120, 200, 255));
    TextOutA(mem, 20, 20, info, (int)strlen(info));

    SelectObject(mem, oldBr);
    SelectObject(mem, oldPen);
    DeleteObject(pen);

    BitBlt(screen, 0, 0, g_w, g_h, mem, 0, 0, SRCCOPY);

    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
}

static DWORD WINAPI Main(LPVOID) {
    while (!GetModuleHandleA("GameAssembly.dll")) Sleep(200);

    int tries = 0;
    while (!InitIl2cpp() && tries++ < 100) Sleep(200);

    g_w = GetSystemMetrics(SM_CXSCREEN);
    g_h = GetSystemMetrics(SM_CYSCREEN);

    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "BerdaOverlay";
    RegisterClassA(&wc);

    g_wnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "BerdaOverlay", "berda", WS_POPUP,
        0, 0, g_w, g_h, nullptr, nullptr, wc.hInstance, nullptr);
    SetLayeredWindowAttributes(g_wnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(g_wnd, SW_SHOW);

    HDC screen = GetDC(nullptr);

    while (g_run) {
        if (GetAsyncKeyState(VK_F8) & 1) g_esp = !g_esp;
        if (GetAsyncKeyState(VK_END) & 1) break;

        if (g_esp) {
            tObject cam = Call(M_CAM_MAIN, nullptr, nullptr);
            if (cam) CollectTargets(cam);
            else g_targets.clear();
        }

        DrawOverlay(screen);
        Sleep(16);
    }

    ReleaseDC(nullptr, screen);
    if (g_wnd) DestroyWindow(g_wnd);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        CreateThread(nullptr, 0, Main, nullptr, 0, nullptr);
    }
    return TRUE;
}
