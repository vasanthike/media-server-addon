#include <nan.h>
#include "getmedia.h"

void InitAll(v8::Local<v8::Object> exports) {

  Nan::HandleScope scope;
  GetMediaProxy::Init(exports);
}

NODE_MODULE(skybell, InitAll)
