#include <EGL/egl.h>

#include <QOpenGLContext>
#include <QGuiApplication>
#include <QFuture>

#include <WebViewRenderer.h>
#include <c_api.h>

class MainThreadEvent : public QEvent
{
    std::function<void()> f_;

public:
    template <typename F>
    explicit MainThreadEvent(F &&f) : QEvent(MainThreadEvent::event_type()),
                                      f_(std::forward<F>(f))
    {
    }

    void invoke()
    {
        f_();
    }

    static QEvent::Type event_type()
    {
        static int et{-1};

        return QEvent::Type(-1 == et ? et = registerEventType() : et);
    }

    template <typename F>
    static void post(F &&f)
    {
        auto const app(QGuiApplication::instance());

        app->postEvent(app, new MainThreadEvent(std::forward<F>(f)));
    }
};

class QUserApplication : public QGuiApplication
{
    using QGuiApplication::QGuiApplication;

    bool event(QEvent *const e) final
    {
        if (MainThreadEvent::event_type() == e->type())
        {
            return static_cast<MainThreadEvent *>(e)->invoke(), true;
        }
        else
        {
            return QGuiApplication::event(e);
        }
    }
};

#define Q_EXEC(_EXPR) \
    QFutureInterface<void> tcs; \
    tcs.reportStarted(); \
    MainThreadEvent::post([&]() \
    { \
        _EXPR \
        tcs.reportResult(nullptr, 0 ); \
        tcs.reportFinished(); \
    }); \
    tcs.future().waitForFinished();

#define Q_EXEC_RETURN(_TYPE, _EXPR, _RETVAL) \
    QFutureInterface<_TYPE> tcs; \
    tcs.reportStarted(); \
    MainThreadEvent::post([&]() \
    { \
        _EXPR \
        tcs.reportResult(_RETVAL, 0 ); \
        tcs.reportFinished(); \
    }); \
    return tcs.future().result();

Q_GUI_EXPORT void qt_gl_set_global_share_context(QOpenGLContext *pContext);
Q_GUI_EXPORT QOpenGLContext *qt_gl_global_share_context();

static GLXContext s_qt_glx_ctx = nullptr;

static EGLContext s_external_egl_ctx = nullptr;
static EGLDisplay s_external_egl_dpy = nullptr;

int qwebengine_load_gl_context()
{
    auto externalGlxCtx = glXGetCurrentContext();
    if (externalGlxCtx != nullptr)
    {
        Display *xdisplay = glXGetCurrentDisplay();
        if (xdisplay == nullptr)
            return -100;

        // Create a helper OpenGL context
        int scrnum = DefaultScreen(xdisplay);
        int attribs[] = {
            GLX_X_RENDERABLE, True,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
            GLX_RED_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            GLX_ALPHA_SIZE, 8,
            GLX_DEPTH_SIZE, 24,
            GLX_STENCIL_SIZE, 8,
            GLX_DOUBLEBUFFER, True,
            // GLX_SAMPLE_BUFFERS  , 1,
            // GLX_SAMPLES         , 4,
            None};

        int numConfigs = 0;
        GLXFBConfig *fbconfig = glXChooseFBConfig(xdisplay, scrnum, attribs, &numConfigs);
        if (numConfigs == 0)
            return -99;

        // int context_attribs[] =
        //     {
        //         GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        //         GLX_CONTEXT_MINOR_VERSION_ARB, 0,
        //         GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
        //         // GLX_CONTEXT_FLAGS_ARB        , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
        //         None};
        // GLXContext sharedCtx = glXCreateContextAttribsARB(xdisplay, *fbconfig, s_external_glx_ctx, true, context_attribs);

        GLXContext sharedCtx = glXCreateNewContext(xdisplay, *fbconfig, GLX_RGBA_TYPE, externalGlxCtx, true);
        if (fbconfig != nullptr)
            XFree(fbconfig);

        if (sharedCtx == nullptr)
            return -98;

        s_qt_glx_ctx = sharedCtx;
    }
    else
    {
        s_external_egl_ctx = eglGetCurrentContext();
        s_external_egl_dpy = eglGetCurrentDisplay();

        if (s_external_egl_ctx == nullptr)
            return -1;
        if (s_external_egl_dpy == nullptr)
            return -2;
    }

    return 0;
}

int qwebengine_host_run(int argc, char **argv)
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::GraphicsApi::OpenGL);

    QUserApplication app(argc, argv);

    int ret;
    if (s_external_egl_ctx && s_external_egl_dpy)
    {
        auto rootCtx = QNativeInterface::QEGLContext::fromNative(s_external_egl_ctx, s_external_egl_dpy);
        if (rootCtx == nullptr)
        {
            return -1000;
        }

        QOpenGLContext appCtx;
        appCtx.setFormat(QSurfaceFormat::defaultFormat());
        appCtx.setShareContext(rootCtx);
        if (!appCtx.create())
        {
            return -1001;
        }

        qt_gl_set_global_share_context(&appCtx);
        ret = app.exec();
        qt_gl_set_global_share_context(nullptr);
        delete rootCtx;
    }
    else
    {
        auto rootCtx = QNativeInterface::QGLXContext::fromNative(s_qt_glx_ctx);
        if (rootCtx == nullptr)
        {
            return -1002;
        }

        qt_gl_set_global_share_context(rootCtx);
        ret = app.exec();
        qt_gl_set_global_share_context(nullptr);
        delete rootCtx;
    }

    return ret;
}

void qwebengine_host_quit()
{
    QCoreApplication::instance()->quit();
}

WebViewRenderer *qwebengine_webview_create(int32_t width, int32_t height, pfn_webview_texture_changed texture_changed_cb)
{
    Q_EXEC_RETURN(
        WebViewRenderer *,
        auto view = new WebViewRenderer(qt_gl_global_share_context(), width, height, texture_changed_cb);,
        view);
}

void qwebengine_webview_destroy(WebViewRenderer *webview)
{
    Q_EXEC(
        delete webview;
    )
}

void qwebengine_webview_set_url(WebViewRenderer *webview, const char *url)
{
    Q_EXEC(
        webview->webview()->setUrl(QUrl(url));
    )
}

void qwebengine_webview_set_size(WebViewRenderer *webview, int32_t width, int32_t height)
{
    Q_EXEC(
        webview->setSize(width, height);
    )
}