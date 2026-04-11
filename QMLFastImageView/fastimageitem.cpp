#include "fastimageitem.h"
#include <QQuickWindow>
#include <QMutexLocker>
#include <QThread>
#include <QCoreApplication>

FastImageItem::FastImageItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void FastImageItem::setImage(const QImage &image)
{
    if (m_rendering.load(std::memory_order_relaxed) || m_updatePending)
        return;
    // enforce GUI thread
    if (QThread::currentThread() == qApp->thread())
    {
        if(!image.isNull())
        {
            m_backImage = image.copy();
            m_newFrame = true;
            update();
        }
    }
}

QSGNode *FastImageItem::updatePaintNode(QSGNode *oldNode,
                                        UpdatePaintNodeData *)
{
    if(m_updatePending)
    {
        return oldNode;
    }
    m_updatePending = true;
    m_rendering.store(true, std::memory_order_relaxed);
    QSGSimpleTextureNode *node =
        static_cast<QSGSimpleTextureNode *>(oldNode);

    if (!node)
        node = new QSGSimpleTextureNode();

    if (!window())
    {
        m_updatePending = false;
        return node;
    }

    QImage frame;

    {

        if (m_newFrame)
        {
            std::swap(m_frontImage, m_backImage);
            frame = m_frontImage;
            m_newFrame = false;
        }
    }

    if (!frame.isNull())
    {
        if (m_texture)
        {
            delete m_texture;
            m_texture = nullptr;
        }

        m_texture = window()->createTextureFromImage(frame);

        node->setTexture(m_texture);
        node->setOwnsTexture(false);

        node->setRect(boundingRect());
    }

    m_rendering.store(false, std::memory_order_relaxed);
    m_updatePending = false;

    return node;
}
