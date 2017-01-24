#include <node.h>
#include <v8.h>
#include <string>
#include <map>
#include <uv.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <chrono>
#include <thread>
#include <queue>
#include <nan.h>
#include <memory>
#include "render.h"

using namespace v8;

typedef Persistent<Function, CopyablePersistentTraits<Function>> v8_persistent_copyable_function;

// Structure of this is borrowed from https://gist.github.com/matzoe/11082417
class WorkItem {
    typedef struct payload {
        v8_persistent_copyable_function *func;
        WorkItem *work_ref;
        uint8_t *data; // I own this but I will give it away later
        size_t data_length;
        std::string *error; // I own this
    } payload;

    uv_async_t *handle; // I own this, play nice with libuv before freeing.
    v8_persistent_copyable_function func;

    // I own this
    uint8_t *html;
    size_t html_length;

    bool init_success;

    public:
        typedef std::unique_ptr<WorkItem> ptr;

        WorkItem(uint8_t *html, size_t html_length, v8_persistent_copyable_function& f)
            : func(f), html(html), html_length(html_length), init_success(false) {
            handle = (uv_async_t*)malloc(sizeof(uv_async_t));

            if (handle == NULL) {
                return;
            }
            int err = 0;
            err = uv_async_init(uv_default_loop(), handle, WorkItem::uv_cb);
            if (err < 0) {
                return;
            }
            init_success = true;
        }

        ~WorkItem() {
            if (html) {
                delete html;
            }
            uv_close((uv_handle_t*)handle, close_cb);
        }

        bool did_init() const { return init_success; }

        const uint8_t* get_html() const { return html; }
        size_t get_html_length() const { return html_length; }

        // Assume ownership of err
        void done(std::string *err) {
            auto p = new payload();
            p->func = &func;
            p->work_ref = this;
            p->error = err;

            send_payload(p);
        }

        void done(uint8_t* data, size_t data_length) {
            auto p = new payload();
            p->data = data;
            p->data_length = data_length;
            p->func = &func;
            p->work_ref = this;
            p->error = nullptr;

            send_payload(p);
        }

    private:
        void send_payload(payload *p) {
            handle->data = (void *)p;
            // Nothing much we can do if this fails, because we can't talk to the main thread.
            uv_async_send(handle);
        }

        static void uv_cb(uv_async_t *handle) {
            auto p = (payload *)handle->data;
            auto isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);

            const unsigned argc = 2;
            Local<Value> argv[argc];
            if (p->error) {
                argv[0] = String::NewFromUtf8(isolate, p->error->c_str());
                argv[1] = Null(isolate);
                delete p->error;
            } else {
                argv[0] = Null(isolate);
                argv[1] = Nan::NewBuffer((char*)p->data, p->data_length).ToLocalChecked(); // This transfers ownership of p->data to Node
            }

            Local<Function>::New(isolate, *p->func)->Call(Null(isolate), argc, argv);

            delete p->work_ref; // Effectively delete this;
            delete p;
        }

        static void close_cb (uv_handle_t* handle) {
            if (handle) {
                free(handle);
            }
        };
};


std::queue<WorkItem*> work_queue;
uv_mutex_t work_mutex;


void worker_thread(void *arg) {
    // TODO: This should be configurable from Node
    setup_printer(
        GTK_PAGE_ORIENTATION_PORTRAIT,
        0.5, 0.5, 0.5, 0.5,
        GTK_UNIT_INCH
    );

    // This is the main render loop
    while(true) {
         std::this_thread::sleep_for(std::chrono::milliseconds(1));

         WorkItem *work = nullptr;

         // Safely check for work
         uv_mutex_lock(&work_mutex);
         if (!work_queue.empty()) {
            work = work_queue.front();
            work_queue.pop();
         }
         uv_mutex_unlock(&work_mutex);

         // Perform the work if we need to
         if (work != nullptr) {
            debug_print("doing work\n");

            size_t pdf_length = 0;
            std::string *error = nullptr;
            // Accept ownership of error
            auto pdf_data = render(work->get_html(), work->get_html_length(), &pdf_length, &error);

            if (error != nullptr) {
                // Transfer ownership of error
                work->done(error);
            } else {
                work->done(pdf_data, pdf_length);
            }
            work = nullptr;
         }
    }
}


void start(const FunctionCallbackInfo<Value>& args) {
    auto isolate = args.GetIsolate();

    debug_print("Starting background thread\n");
    int err = 0;
    err = uv_mutex_init(&work_mutex);
    if (err < 0) {
        isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Could not create uv mutex.")));
        return;
    }

    uv_thread_t id;
    err = uv_thread_create(&id, worker_thread, NULL);
    if (err < 0) {
        isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Could not create worker thread.")));
    }
}

void render(const FunctionCallbackInfo<Value>& args) {
    auto isolate = args.GetIsolate();

    // Arg 0 is our html in a buffer
    // Arg 1 is our callback
    auto func = v8_persistent_copyable_function(Isolate::GetCurrent(), Local<Function>::Cast(args[1]));
    auto html = args[0]->ToObject();
    auto html_data = node::Buffer::Data(html);
    auto html_length = node::Buffer::Length(html);

    // Copy the html into our own heap. TODO: Might be able to avoid this
    auto html_copy = new uint8_t[html_length];
    memcpy(html_copy, html_data, html_length);

    auto new_work = new WorkItem(html_copy, html_length, func);

    if (!new_work->did_init()) {
        delete new_work;
        isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Could not create work item.")));
        return;
    }

    // Safely add this work to the queue
    uv_mutex_lock(&work_mutex);
    work_queue.push(new_work);
    uv_mutex_unlock(&work_mutex);
}

void init(Handle <Object> exports, Handle<Object> module) {
    NODE_SET_METHOD(exports, "start", start);
    NODE_SET_METHOD(exports, "render", render);
}

// associates the module name with initialization logic
NODE_MODULE(PDFRenderer, init)

