#pragma once

#include <QObject>
#include <QVariantMap>
#include <QVariantList>

class SecureMessageService;

// A simulated route carries actual bytes; receiving never reads the sender's input.
class TransmissionSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap senderTrace READ senderTrace NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap receiverTrace READ receiverTrace NOTIFY stateChanged)
public:
    explicit TransmissionSession(SecureMessageService *service);
    QVariantMap state() const { return m_state; }
    QVariantMap senderTrace() const { return m_senderTrace; }
    QVariantMap receiverTrace() const { return m_receiverTrace; }
    Q_INVOKABLE QVariantMap processTrace(QString endpoint, int blockIndex = 0) const;
    Q_INVOKABLE bool start(QString plaintext, QString key, bool encrypted,
                           QVariantList route, int observer);
    Q_INVOKABLE bool advance();
    Q_INVOKABLE bool forward();
    Q_INVOKABLE bool tamper();
    Q_INVOKABLE bool restore();
    Q_INVOKABLE bool decrypt(QString receiverKey);
    Q_INVOKABLE void reset();
signals:
    void stateChanged();
private:
    bool fail(const QString &error);
    void event(const QString &description);
    SecureMessageService *m_service;
    QVariantMap m_state;
    QVariantMap m_senderTrace, m_receiverTrace;
    QString m_senderKey, m_receiverKey;
    int m_sequence = 0;
};
