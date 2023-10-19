#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickView>
#include <QtWebEngineQuick/private/qquickwebengineview_p.h>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#include <QOpenGLContext>
#include <QQuickRenderControl>
#include <QQuickGraphicsDevice>
#include <QQuickRenderTarget>
#include <QOpenGLFramebufferObject>
#include <QOffscreenSurface>
#include <qopenglfunctions.h>
#include <QFuture>
#include <QTimer>
#include <QColorSpace>

#include <iostream>
#include <thread>

#include <WebViewRenderer.h>

static void print(const QSurfaceFormat &format)
{
    qWarning("QSurfaceFormat: RGBA=%d%d%d%d, Depth=%d, Stencil=%d, Swap=%d, Type=%d, Profile=%d, Version=%d.%d",
             format.redBufferSize(), format.greenBufferSize(), format.blueBufferSize(), format.alphaBufferSize(),
             format.depthBufferSize(), format.stencilBufferSize(),
             format.swapBehavior(), format.renderableType(), format.profile(), format.majorVersion(), format.minorVersion());
}

WebViewRenderer::WebViewRenderer(QOpenGLContext *context, int width, int height, pfn_webview_texture_changed textureChangedCallback)
    : QObject(),
      _context(context),
      _viewportSize(width, height),
      _notifyTextureChanged(textureChangedCallback)
{
    _surface.setFormat(context->format());
    _surface.create();
    if (!context->makeCurrent(&_surface))
    {
        qWarning("Unable to make OpenGL context current.");
    }

    _updateTimer.setInterval(0);
    _updateTimer.setSingleShot(true);
    connect(&_updateTimer, &QTimer::timeout, this, &WebViewRenderer::render);

    _view = new QQuickView(QUrl(QStringLiteral("qrc:/main/webview.qml")), &_renderer);
    connect(_view, &QQuickView::sceneGraphInitialized, this, &WebViewRenderer::createTexture);
    connect(_view, &QQuickView::sceneGraphInvalidated, this, &WebViewRenderer::destroyTexture);
    auto device = QQuickGraphicsDevice::fromOpenGLContext(context);
    _view->setGraphicsDevice(device);
    _view->setFormat(context->format());
    updateSizes();

    connect(&_renderer, &QQuickRenderControl::renderRequested, this, std::bind(&WebViewRenderer::requestUpdate, this, false));
    connect(&_renderer, &QQuickRenderControl::sceneChanged, this, std::bind(&WebViewRenderer::requestUpdate, this, true));

    connect(webview(), &QQuickWebEngineView::loadProgressChanged, this, &WebViewRenderer::loadProgressChanged);

    if (!_renderer.initialize())
    {
        qWarning("Failed to initialize renderer.");
    }
}

void WebViewRenderer::loadProgressChanged()
{
    qWarning("Load progress >>> %d, State = %d, Visible = %d, HWND = %p.",
             webview()->loadProgress(), webview()->lifecycleState(), webview()->isVisible(),
             webview()->window());
}

WebViewRenderer::~WebViewRenderer()
{
    delete _view;
}

void WebViewRenderer::createTexture()
{
    QOpenGLFunctions *f = _context->functions();
    f->glGenTextures(1, &_textureId);
    f->glBindTexture(GL_TEXTURE_2D, _textureId);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    f->glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, _viewportSize.width(), _viewportSize.height(), 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    _view->setRenderTarget(QQuickRenderTarget::fromOpenGLTexture(_textureId, _viewportSize));

    if (_notifyTextureChanged)
    {
        _notifyTextureChanged(this, _textureId);
    }
}

void WebViewRenderer::destroyTexture()
{
    if (_notifyTextureChanged)
    {
        _notifyTextureChanged(this, 0);
    }

    _context->functions()->glDeleteTextures(1, &_textureId);
    _textureId = 0;
}

void WebViewRenderer::requestUpdate(bool sceneChanged)
{
    if (!_hasPendingChange && sceneChanged)
    {
        _hasPendingChange = true;
    }

    if (!_updateTimer.isActive())
    {
        _updateTimer.start();
    }
}

void WebViewRenderer::render()
{
    if (!_context->makeCurrent(&_surface))
    {
        return;
    }

    _renderer.beginFrame();

    if (_hasPendingChange)
    {
        _renderer.polishItems();
        if (_renderer.sync())
        {
            _renderer.render();
        }
        _hasPendingChange = false;
    }
    else
    {
        _renderer.render();
    }

    _renderer.endFrame();
    _context->doneCurrent();
}

void WebViewRenderer::resizeTexture()
{
    if (_view->contentItem() && _context->makeCurrent(&_surface))
    {
        if (_textureId != 0)
        {
            destroyTexture();
        }

        createTexture();
        _context->doneCurrent();
        updateSizes();
        render();
    }
}

void WebViewRenderer::updateSizes()
{
    // Behave like SizeRootObjectToView.
    _view->contentItem()->setWidth(_viewportSize.width());
    _view->contentItem()->setHeight(_viewportSize.height());

    _view->setGeometry(0, 0, _viewportSize.width(), _viewportSize.height());
}

void WebViewRenderer::setSize(int width, int height)
{
    _viewportSize.setWidth(width);
    _viewportSize.setHeight(height);

    resizeTexture();
}

