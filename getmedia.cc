#include "getmedia.h"
#include "clientproxy.h"

using namespace v8;

char *get(v8::Local<v8::Value> value, const char *fallback = "") {
  if (value->IsString()) {
      v8::String::AsciiValue string(value);
      char *str = (char *) malloc(string.length() + 1);
      strcpy(str, *string);
      return str;
  }
  char *str = (char *) malloc(strlen(fallback) + 1);
  strcpy(str, fallback);
  return str;
}

void GetMediaProxy::Init(Handle<Object> exports) {

  exports->Set(Nan::New("getMedia").ToLocalChecked(), Nan::New<FunctionTemplate>(GetMediaProxy::GetMedia)->GetFunction());
}

void GetMediaProxy::GetMedia(const Nan::FunctionCallbackInfo<Value> &info) {
    
    int bp = info[0]->Uint32Value();
    int cp = info[1]->Uint32Value();
    int vp = info[2]->Uint32Value();
    
    char *server = get(info[3], "");
    char *key = get(info[4], "");
    
    printf("\nbp:%d", bp);
    printf("\ncp:%d", cp);
    printf("\nserver:%s", server);
    printf("\nkey:%s\n", key);

    int res = getMediaProxy(bp,cp,vp,server,key);

    info.GetReturnValue().Set(8);
}

