#pragma once

#include <QQuickItem>
#include <QImage>
#include <QSGTexture>
#include <QSGSimpleTextureNode>
#include <atomic>

class FastImageItem : public QQuickItem
{
    Q_OBJECT

public:
    explicit FastImageItem(QQuickItem *parent = nullptr);

    Q_INVOKABLE void setImage(const QImage &image);

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode,
                             UpdatePaintNodeData *) override;

private:

    QImage m_frontImage;
    QImage m_backImage;
    std::atomic_bool m_rendering{false};

    bool m_newFrame = false,m_updatePending = false;

    QSGTexture *m_texture = nullptr;
};
