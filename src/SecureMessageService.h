#pragma once

#include <QObject>
#include <QVariantMap>

class SecureMessageService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *channel READ channel CONSTANT)
public:
    explicit SecureMessageService(QObject *parent = nullptr);
    QObject *channel() const { return m_channel; }

    Q_INVOKABLE QVariantMap generateKey() const;
    Q_INVOKABLE QVariantMap encrypt(const QString &plaintext, const QString &keyHex) const;
    Q_INVOKABLE QVariantMap decrypt(const QString &packet, const QString &keyHex) const;
    Q_INVOKABLE QVariantMap inspectPacket(const QString &packet) const;
    Q_INVOKABLE QVariantMap traceAes(const QString &blockHex, const QString &keyHex) const;
    Q_INVOKABLE QVariantMap tracePacket(const QString &packet, const QString &keyHex, int blockIndex = 0) const;
private:
    QObject *m_channel;
};
