#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace ncs {

class FrameCodec
{
public:
    static QByteArray encode(const QByteArray &payload);

    bool append(const QByteArray &bytes,
                QList<QByteArray> *completedFrames,
                QString *errorMessage);
    void reset();
    qsizetype bufferedBytes() const;

private:
    QByteArray buffer_;
};

}  // namespace ncs
