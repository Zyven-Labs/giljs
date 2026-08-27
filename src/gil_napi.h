#ifndef GIL_NAPI_H
#define GIL_NAPI_H

#include <napi.h>
#include "gil.h"

/* ------------------------------------------------------------------ */
/* Script — wraps GilScript*, GC-auto-freed via Finalize               */
/* ------------------------------------------------------------------ */
class Script : public Napi::ObjectWrap<Script> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    static Napi::FunctionReference constructor;

    Script(const Napi::CallbackInfo& info);
    void Finalize(Napi::Env env) override;

    // Static factories
    static Napi::Value Load(const Napi::CallbackInfo& info);
    static Napi::Value LoadFile(const Napi::CallbackInfo& info);

    // Instance methods
    Napi::Value Intent(const Napi::CallbackInfo& info);

private:
    GilScript *script_ = nullptr;
};

/* ------------------------------------------------------------------ */
/* Frontier — wraps GilFrontier*, GC-auto-freed via Finalize           */
/* ------------------------------------------------------------------ */
class Frontier : public Napi::ObjectWrap<Frontier> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    static Napi::FunctionReference constructor;

    Frontier(const Napi::CallbackInfo& info);
    void Finalize(Napi::Env env) override;

    Napi::Value Get(const Napi::CallbackInfo& info);
    void       Set(const Napi::CallbackInfo& info);
    void       Del(const Napi::CallbackInfo& info);
    Napi::Value Query(const Napi::CallbackInfo& info);
    void       Lock(const Napi::CallbackInfo& info);
    void       Unlock(const Napi::CallbackInfo& info);

private:
    GilFrontier *frontier_ = nullptr;
    friend class Intent;
};

/* ------------------------------------------------------------------ */
/* Intent — wraps GilIntent*, does NOT own memory                       */
/* Keeps parent Script alive via Napi::Reference                        */
/* ------------------------------------------------------------------ */
class Intent : public Napi::ObjectWrap<Intent> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    static Napi::FunctionReference constructor;

    Intent(const Napi::CallbackInfo& info);

    void Execute(const Napi::CallbackInfo& info);

    // Factory: called from Script::Intent to create a new Intent JS object
    static Napi::Object New(Napi::Env env, GilIntent *gil_intent,
                            Napi::Object script_obj);

private:
    GilIntent *intent_ = nullptr;
    Napi::Reference<Napi::Object> script_ref_;
};

/* ------------------------------------------------------------------ */
/* Module initialisation                                               */
/* ------------------------------------------------------------------ */
Napi::Object InitAll(Napi::Env env, Napi::Object exports);

#endif /* GIL_NAPI_H */