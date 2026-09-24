#include "frame_codec.h"

#include "config/network_config.h"

#include <QtEndian>

namespace ncs {

QByteArray FrameCodec::encode(const QByteArray &payload)
{
    if (payload.size() > static_cast<qsizetype>(NetworkConfig::MaximumFramePayload)) {
        return QByteArray();
    }

    QByteArray frame(4, Qt::Uninitialized);
    qToBigEndian<quint32>(static_cast<quint32>(payload.size()),
                          reinterpret_cast<uchar *>(frame.data()));
    frame.append(payload);
    return frame;
}

bool FrameCodec::append(const QByteArray &bytes,
                        QList<QByteArray> *completedFrames,
                        QString *errorMessage)
{
    buffer_.append(bytes);
    while (buffer_.size() >= 4) {
        const quint32 payloadLength = qFromBigEndian<quint32>(
            reinterpret_cast<const uchar *>(buffer_.constData()));
        if (payloadLength > NetworkConfig::MaximumFramePayload) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("frame payload exceeds the 1 MiB limit");
            }
            buffer_.clear();
            return false;
        }

        const qsizetype frameLength = 4 + static_cast<qsizetype>(payloadLength);
        if (buffer_.size() < frameLength) {
            return true;
        }
        completedFrames->append(buffer_.mid(4, payloadLength));
        buffer_.remove(0, frameLength);
    }
    return true;
}

void FrameCodec::reset()
{
    buffer_.clear();
}

qsizetype FrameCodec::bufferedBytes() const
{
    return buffer_.size();
}

}  // namespace ncs
