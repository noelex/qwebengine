#pragma once

#include <stdint.h>

class WebViewRenderer;

extern "C"{
    typedef void (*pfn_webview_texture_changed)(WebViewRenderer *renderer, uint32_t texture);

    int qwebengine_load_gl_context();

    int32_t qwebengine_host_run(int argc, char **argv);

    void qwebengine_host_quit();

    WebViewRenderer *qwebengine_webview_create(int32_t width, int32_t height, pfn_webview_texture_changed texture_changed_cb);

    void qwebengine_webview_destroy(WebViewRenderer *webview);

    void qwebengine_webview_set_url(WebViewRenderer *webview, const char *url);

    void qwebengine_webview_set_size(WebViewRenderer *webview, int32_t width, int32_t height);
}
