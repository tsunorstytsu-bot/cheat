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
