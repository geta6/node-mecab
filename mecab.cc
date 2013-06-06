//#define BUILDING_NODE_EXTENSION
//#include </usr/local/Cellar/node/0.6.19/include/node/uv.h>
//#include <uv.h>
//#include <v8.h>
#include <node.h>
#include <iostream>
#include <mecab.h>
//#include "node-mecab.h"

using namespace node;
using namespace v8;
//using namespace std;

// We use a struct to store information about the asynchronous "work request".
struct Baton {
    // libuv's request struct.
    uv_work_t request;

    // This handle holds the callback function we'll call after the work request
    // has been completed in a threadpool thread. It's persistent so that V8
    // doesn't garbage collect it away while our request waits to be processed.
    // This means that we'll have to dispose of it later ourselves.
    Persistent<Function> callback;

    // Tracking errors that happened in the worker function. You can use any
    // variables you want. E.g. in some cases, it might be useful to report
    // an error number.
    bool error;
    std::string error_message;

    // Custom data you can pass through.
    const char* result;
    const char* parseString;
};


Handle<Value> Async(const Arguments& args);
void AsyncWork(uv_work_t* req);
void AsyncAfter(uv_work_t* req);

// This is the function called directly from JavaScript land. It creates a
// work request object and schedules it for execution.
Handle<Value> Async(const Arguments& args) {
    HandleScope scope;

    if (!args[1]->IsFunction() && !args[0]->IsString() ) {
        return ThrowException(Exception::TypeError(
                                                   v8::String::New("First argument must be a callback function")));
    }
    // There's no ToFunction(), use a Cast instead.
    Local<Function> callback = Local<Function>::Cast(args[1]);

    // This creates our work request, including the libuv struct.
    Baton* baton = new Baton();
    baton->error = false;
    //String::Utf8Value input(args[0]->ToString());
    baton->request.data = baton;
    baton->parseString = *String::Utf8Value(args[0]->ToString());
    baton->callback = Persistent<Function>::New(callback);

    // Schedule our work request with libuv. Here you can specify the functions
    // that should be executed in the threadpool and back in the main thread
    // after the threadpool function completed.
    int status = uv_queue_work(uv_default_loop(),&baton->request , AsyncWork, (uv_after_work_cb)AsyncAfter);
    assert(status == 0);

    return Undefined();
}
// This function is executed in another thread at some point after it has been
// scheduled. IT MUST NOT USE ANY V8 FUNCTIONALITY. Otherwise your extension
// will crash randomly and you'll have a lot of fun debugging.
// If you want to use parameters passed into the original call, you have to
// convert them to PODs or some other fancy method.
void AsyncWork(uv_work_t* req) {
    Baton* baton = static_cast<Baton*>(req->data);

    // Do work in threadpool here.

    //MeCabの中身を書いてく。
    MeCab::Tagger *t = MeCab::createTagger("");
    //String::Utf8Value input(baton->request.data);
    const char *result = t->parse(baton->parseString);
    baton->result = result;

    // If the work we do fails, set baton->error_message to the error string
    // and baton->error to true.
}

// This function is executed in the main V8/JavaScript thread. That means it's
// safe to use V8 functions again. Don't forget the HandleScope!
void AsyncAfter(uv_work_t* req) {
    HandleScope scope;
    Baton* baton = static_cast<Baton*>(req->data);

    if (baton->error) {
        Local<Value> err = Exception::Error(v8::String::New(baton->error_message.c_str()));

        // Prepare the parameters for the callback function.
        const unsigned argc = 1;
        Local<Value> argv[argc] = { err };

        // Wrap the callback function call in a TryCatch so that we can call
        // node's FatalException afterwards. This makes it possible to catch
        // the exception from JavaScript land using the
        // process.on('uncaughtException') event.
        TryCatch try_catch;
        baton->callback->Call(Context::GetCurrent()->Global(), argc, argv);
        if (try_catch.HasCaught()) {
            node::FatalException(try_catch);
        }
    } else {
        // In case the operation succeeded, convention is to pass null as the
        // first argument before the result arguments.
        // In case you produced more complex data, this is the place to convert
        // your plain C++ data structures into JavaScript/V8 data structures.
        const unsigned argc = 2;
        Local<Value> argv[argc] = {
            Local<Value>::New(Null()),
            Local<Value>::New(v8::String::New(baton->result))
        };

        // Wrap the callback function call in a TryCatch so that we can call
        // node's FatalException afterwards. This makes it possible to catch
        // the exception from JavaScript land using the
        // process.on('uncaughtException') event.
        TryCatch try_catch;
        baton->callback->Call(Context::GetCurrent()->Global(), argc, argv);
        if (try_catch.HasCaught()) {
            node::FatalException(try_catch);
        }
    }

    // The callback is a permanent handle, so we have to dispose of it manually.
    baton->callback.Dispose();
    delete baton;
}


void RegisterMecabModule(Handle<Object> target){
    target->Set(String::NewSymbol("mecabAsync"),FunctionTemplate::New(Async)->GetFunction());
}
NODE_MODULE(mecab, RegisterMecabModule)

/*
Handle<Value> Tagger::New (const Arguments& args) {
	HandleScope scope;

	Tagger *p = new Tagger();
	p->Wrap(args.This());

	p->tagger = MeCab::createTagger((args.Length() >= 1 && args[0]->IsString()) ? *String::Utf8Value(args[0]->ToString()) : "");

	return args.This();
}

Handle<Value> Tagger::Parse (const Arguments& args) {
	HandleScope scope;
	if (args.Length() != 1 || !args[0]->IsString()) {
		return ThrowException(Exception::Error(String::New("Bad argument.")));
	}

	Tagger *self = ObjectWrap::Unwrap<Tagger>(args.This());

	String::Utf8Value input(args[0]->ToString());

	const char *result = self->tagger->parse(*input);

	return String::New(result);
}

Tagger::~Tagger(){
	delete tagger;
}

void AddonInitialize (Handle<Object> target) {
	HandleScope scope;

	Local<FunctionTemplate> t = FunctionTemplate::New(Tagger::New);
	t->InstanceTemplate()->SetInternalFieldCount(1);
	t->SetClassName(String::NewSymbol("Tagger"));

	NODE_SET_PROTOTYPE_METHOD(t, "parse", Tagger::Parse);

	target->Set(String::New("Tagger"), t->GetFunction());
}

NODE_MODULE(mecab, AddonInitialize);
*/
