/* gil_napi.cc — N-API C++ wrapper for libgil using node-addon-api
 *
 * All public types (Script, Frontier, Intent) are wrapped as Napi::ObjectWrap
 * subclasses. Memory is freed automatically when the garbage collector
 * reclaims the JS object (via Finalize).
 *
 * GilResult (from queries) is consumed immediately and returned as a plain
 * JS object — no wrapper needed.
 */

#include "gil_napi.h"
#include <cstring>
#include <string>
#include <vector>

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Parse a JS value (string or string[]) into a C args array.
 * If the value is undefined/null, args remains empty.
 * If the value is a string, it's treated as a single argument.
 * If the value is an array, each element must be a string.
 * Returns true on success, false on type mismatch (throws JS TypeError). */
static bool parse_args(const Napi::CallbackInfo& info, int index,
                       std::vector<const char*>& out)
{
    Napi::Env env = info.Env();
    if (info.Length() <= (size_t)index || info[index].IsUndefined() || info[index].IsNull()) {
        return true;
    }

    if (info[index].IsString()) {
        std::string s = info[index].As<Napi::String>().Utf8Value();
        char *copy = strdup(s.c_str());
        out.push_back(copy);
        return true;
    }

    if (info[index].IsArray()) {
        Napi::Array arr = info[index].As<Napi::Array>();
        for (uint32_t i = 0; i < arr.Length(); i++) {
            Napi::Value elem = arr[i];
            if (!elem.IsString()) {
                /* Emit a clear error. Do NOT free here: every caller already
                   calls free_args() on the false path, so freeing here would
                   double-free the previously strdup'd strings. */
                Napi::TypeError::New(env, "each argument must be a string")
                    .ThrowAsJavaScriptException();
                return false;
            }
            std::string s = elem.As<Napi::String>().Utf8Value();
            char *copy = strdup(s.c_str());
            out.push_back(copy);
        }
        return true;
    }

    Napi::TypeError::New(env, "args must be a string, string[], or omitted")
        .ThrowAsJavaScriptException();
    return false;
}

/* Free strdup'd strings allocated by parse_args. */
static void free_args(std::vector<const char*>& args)
{
    for (size_t i = 0; i < args.size(); i++) {
        free((void*)args[i]);
    }
}

/* ------------------------------------------------------------------ */
/* Script                                                             */
/* ------------------------------------------------------------------ */

Napi::FunctionReference Script::constructor;

Script::Script(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<Script>(info), script_(nullptr)
{
}

void Script::Finalize(Napi::Env /*env*/)
{
    if (script_) {
        gil_script_free(script_);
        script_ = nullptr;
    }
}

Napi::Value Script::Load(const Napi::CallbackInfo& info)
{
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "source must be a string").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    std::string source = info[0].As<Napi::String>().Utf8Value();

    const char *error = NULL;
    GilScript *s = gil_load(source.c_str(), &error);
    if (!s) {
        std::string msg = error ? error : "failed to load script";
        Napi::Error::New(env, msg).ThrowAsJavaScriptException();
        return env.Undefined();
    }

    Napi::Object obj = constructor.New({});
    Script *wrap = Napi::ObjectWrap<Script>::Unwrap(obj);
    wrap->script_ = s;
    return obj;
}
Napi::Value Script::LoadFile(const Napi::CallbackInfo& info)
{
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "path must be a string").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    std::string path = info[0].As<Napi::String>().Utf8Value();

    const char *error = NULL;
    GilScript *s = gil_load_file(path.c_str(), &error);
    if (!s) {
        std::string msg = error ? error : "failed to load file";
        Napi::Error::New(env, msg).ThrowAsJavaScriptException();
        return env.Undefined();
    }

    Napi::Object obj = constructor.New({});
    Script *wrap = Napi::ObjectWrap<Script>::Unwrap(obj);
    wrap->script_ = s;
    return obj;
}

Napi::Value Script::Intent(const Napi::CallbackInfo& info)
{
    Napi::Env env = info.Env();
    if (!script_) {
        Napi::Error::New(env, "Script has been freed").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "name must be a string").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    std::string name = info[0].As<Napi::String>().Utf8Value();

    GilIntent *intent = gil_intent_get(script_, name.c_str());
    if (!intent) {
        return env.Undefined(); /* not found — return undefined */
    }

    return Intent::New(env, intent, info.This().As<Napi::Object>());
}

Napi::Object Script::Init(Napi::Env env, Napi::Object exports)
{
    Napi::HandleScope scope(env);

    Napi::Function func = DefineClass(env, "Script", {
        StaticMethod("load",     &Script::Load),
        StaticMethod("loadFile", &Script::LoadFile),
        InstanceMethod("intent", &Script::Intent),
    });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();

    exports.Set("Script", func);
    return exports;
}

/* ------------------------------------------------------------------ */
/* Frontier                                                           */
/* ------------------------------------------------------------------ */

Napi::FunctionReference Frontier::constructor;

Frontier::Frontier(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<Frontier>(info), frontier_(nullptr)
{
    Napi::Env env = info.Env();
    frontier_ = gil_frontier_new(NULL);
    if (!frontier_) {
        Napi::Error::New(env, "failed to create frontier").ThrowAsJavaScriptException();
    }
}

void Frontier::Finalize(Napi::Env /*env*/)
{
    if (frontier_) {
        gil_frontier_free(frontier_);
        frontier_ = nullptr;
    }
}

Napi::Value Frontier::Get(const Napi::CallbackInfo& info)
{
    Napi::Env env = info.Env();
    if (!frontier_) {
        Napi::Error::New(env, "Frontier has been freed").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "name must be a string").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    std::string name = info[0].As<Napi::String>().Utf8Value();

    std::vector<const char*> args;
    if (!parse_args(info, 1, args)) {
        free_args(args);
        return env.Undefined();
    }

    GilVal v = gil_frontier_get(frontier_, name.c_str(),
                                args.empty() ? NULL : args.data(),
                                args.size());
    free_args(args);
    return Napi::Number::New(env, (int)v);
}
void Frontier::Set(const Napi::CallbackInfo& info)
{
    Napi::Env env = info.Env();
    if (!frontier_) {
        Napi::Error::New(env, "Frontier has been freed").ThrowAsJavaScriptException();
        return;
    }
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "name must be a string").ThrowAsJavaScriptException();
        return;
    }
    std::string name = info[0].As<Napi::String>().Utf8Value();

    std::vector<const char*> args;
    int val_index;

    /* Detect whether info[1] is the value (number/boolean, no pred args)
       or predicate args (string/array, value at index 2). */
    if (info.Length() > 1 &&
        (info[1].IsNumber() || info[1].IsBoolean())) {
        val_index = 1;
    } else {
        if (!parse_args(info, 1, args)) {
            free_args(args);
            return;
        }
        val_index = 2;
    }

    GilVal value = GIL_TRUE;
    if (info.Length() > (size_t)val_index) {
        if (info[val_index].IsBoolean()) {
            /* Accept the natural JS boolean form as a predicate value. */
            value = info[val_index].As<Napi::Boolean>().Value()
                        ? GIL_TRUE : GIL_FALSE;
        } else if (info[val_index].IsNumber()) {
            double v = info[val_index].As<Napi::Number>().DoubleValue();
            /* Reject NaN and non-integral values (previously NaN folded to
               garbage and 1.9 silently truncated to 1). */
            if (!(v == 0.0 || v == 1.0 || v == 2.0)) {
                free_args(args);
                Napi::RangeError::New(env,
                    "value must be 0 (FALSE), 1 (TRUE), 2 (BOTH), true, or false")
                    .ThrowAsJavaScriptException();
                return;
            }
            value = (GilVal)(int)v;
        } else {
            free_args(args);
            Napi::TypeError::New(env, "value must be a number, boolean, or omitted")
                .ThrowAsJavaScriptException();
            return;
        }
    }

    gil_frontier_set(frontier_, name.c_str(),
                     args.empty() ? NULL : args.data(),
                     args.size(), value);
    free_args(args);
}

void Frontier::Del(const Napi::CallbackInfo& info)
{
    Napi::Env env = info.Env();
    if (!frontier_) {
        Napi::Error::New(env, "Frontier has been freed").ThrowAsJavaScriptException();
        return;
    }
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "name must be a string").ThrowAsJavaScriptException();
        return;
    }
    std::string name = info[0].As<Napi::String>().Utf8Value();

    std::vector<const char*> args;
    if (!parse_args(info, 1, args)) {
        free_args(args);
        return;
    }

    gil_frontier_del(frontier_, name.c_str(),
                     args.empty() ? NULL : args.data(),
                     args.size());
    free_args(args);
}

Napi::Value Frontier::Query(const Napi::CallbackInfo& info)
{
    Napi::Env env = info.Env();
    if (!frontier_) {
        Napi::Error::New(env, "Frontier has been freed").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "name must be a string").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    std::string name = info[0].As<Napi::String>().Utf8Value();

    std::vector<const char*> pattern;
    if (!parse_args(info, 1, pattern)) {
        free_args(pattern);
        return env.Undefined();
    }

    GilResult result = gil_frontier_query(frontier_, name.c_str(),
                                          pattern.empty() ? NULL : pattern.data(),
                                          pattern.size());

    Napi::Array matches = Napi::Array::New(env, result.count);
    for (size_t i = 0; i < result.count; i++) {
        Napi::Object match = Napi::Object::New(env);
        Napi::Array m_args = Napi::Array::New(env, result.matches[i].argc);
        for (size_t j = 0; j < result.matches[i].argc; j++) {
            m_args[j] = Napi::String::New(env, result.matches[i].args[j]);
        }
        match["args"]  = m_args;
        match["value"] = Napi::Number::New(env, (int)result.matches[i].value);
        matches[i] = match;
    }

    gil_result_free(&result);
    free_args(pattern);

    Napi::Object obj = Napi::Object::New(env);
    obj["matches"] = matches;
    return obj;
}
void Frontier::Lock(const Napi::CallbackInfo&)
{
    if (frontier_) gil_frontier_lock(frontier_);
}

void Frontier::Unlock(const Napi::CallbackInfo&)
{
    if (frontier_) gil_frontier_unlock(frontier_);
}

Napi::Object Frontier::Init(Napi::Env env, Napi::Object exports)
{
    Napi::HandleScope scope(env);

    Napi::Function func = DefineClass(env, "Frontier", {
        InstanceMethod("get",    &Frontier::Get),
        InstanceMethod("set",    &Frontier::Set),
        InstanceMethod("del",    &Frontier::Del),
        InstanceMethod("query",  &Frontier::Query),
        InstanceMethod("lock",   &Frontier::Lock),
        InstanceMethod("unlock", &Frontier::Unlock),
    });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();

    exports.Set("Frontier", func);
    return exports;
}
/* ------------------------------------------------------------------ */
/* Intent                                                             */
/* ------------------------------------------------------------------ */

Napi::FunctionReference Intent::constructor;

Intent::Intent(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<Intent>(info), intent_(nullptr)
{
}

void Intent::Execute(const Napi::CallbackInfo& info)
{
    Napi::Env env = info.Env();
    if (!intent_) {
        Napi::Error::New(env, "Intent has been freed").ThrowAsJavaScriptException();
        return;
    }
    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "first argument must be a Frontier").ThrowAsJavaScriptException();
        return;
    }

    Frontier *fwrap = Napi::ObjectWrap<Frontier>::Unwrap(info[0].As<Napi::Object>());
    if (!fwrap || !fwrap->frontier_) {
        Napi::Error::New(env, "invalid Frontier").ThrowAsJavaScriptException();
        return;
    }

    std::vector<const char*> args;
    /* args are at index 1, but if info[1] is undefined/null treat as empty */
    if (!parse_args(info, 1, args)) {
        free_args(args);
        return;
    }

    int ret = gil_intent_execute(intent_, fwrap->frontier_,
                                 args.empty() ? NULL : args.data(),
                                 args.size());
    free_args(args);

    if (ret != 0) {
        Napi::Error::New(env, "intent execution failed").ThrowAsJavaScriptException();
    }
}
Napi::Object Intent::New(Napi::Env env, GilIntent *gil_intent, Napi::Object script_obj)
{
    Napi::Object obj = constructor.New({});
    Intent *wrap = Napi::ObjectWrap<Intent>::Unwrap(obj);
    wrap->intent_ = gil_intent;
    /* Keep the parent Script alive as long as this Intent is alive. */
    wrap->script_ref_ = Napi::Persistent(script_obj);
    return obj;
}

Napi::Object Intent::Init(Napi::Env env, Napi::Object exports)
{
    Napi::Function func = DefineClass(env, "Intent", {
        InstanceMethod("execute", &Intent::Execute),
    });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();

    exports.Set("Intent", func);
    return exports;
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */

Napi::Object InitAll(Napi::Env env, Napi::Object exports)
{
    Script::Init(env, exports);
    Frontier::Init(env, exports);
    Intent::Init(env, exports);
    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, InitAll)