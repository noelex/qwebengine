#pragma once

#include <QQuickView>
#include <QOffscreenSurface>
#include <QQuickRenderControl>
#include <QTimer>
#include <QtWebEngineQuick/private/qquickwebengineview_p.h>

#include <GL/glx.h>
#include <c_api.h>

#undef Status

class WebViewRenderer : public QObject
{
    Q_OBJECT

public:
    WebViewRenderer(QOpenGLContext *context, int width, int height, pfn_webview_texture_changed textureChangedCallback);
    ~WebViewRenderer();

    inline QQuickWebEngineView *webview()
    {
        return dynamic_cast<QQuickWebEngineView *>(_view->rootObject());
    }

    inline QQuickView *view()
    {
        return _view;
    }

    void setSize(int width, int height);

private:
    void createTexture();
    void destroyTexture();
    void requestUpdate(bool sceneChanged);
    void render();
    void resizeTexture();
    void updateSizes();

    void loadProgressChanged();

private:
    QQuickView *_view;
    QQuickRenderControl _renderer;
    QSize _viewportSize;
    QOpenGLContext *_context;
    QOffscreenSurface _surface;
    GLuint _textureId;
    QTimer _updateTimer;
    pfn_webview_texture_changed _notifyTextureChanged;

    bool _hasPendingChange;
};