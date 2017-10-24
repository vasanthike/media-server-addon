#ifndef CPROXY_H
#define CPROXY_H

#include <nan.h>

class GetMediaProxy {
public:
static void Init(v8::Handle<v8::Object> exports);

private:
static void GetMedia(const Nan::FunctionCallbackInfo<v8::Value> &info);
};

#endif