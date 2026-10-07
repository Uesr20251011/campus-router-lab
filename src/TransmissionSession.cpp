#include "TransmissionSession.h"
#include "SecureMessageService.h"
#include <QSet>

TransmissionSession::TransmissionSession(SecureMessageService *service)
    : QObject(service), m_service(service) { reset(); }

void TransmissionSession::reset() {
    m_senderTrace.clear(); m_receiverTrace.clear();
    m_senderKey.clear(); m_receiverKey.clear();
    m_state = {{"phase", "idle"}, {"packetId", 0}, {"hop", 0}, {"currentNode", -1},
               {"observer", -1}, {"encrypted", true}, {"tampered", false},
               {"route", QVariantList{}}, {"events", QVariantList{}}};
    emit stateChanged();
}

QVariantMap TransmissionSession::processTrace(QString endpoint, int blockIndex) const {
    if (endpoint == "sender" && !m_senderKey.isEmpty())
        return blockIndex == 0 ? m_senderTrace : m_service->tracePacket(m_state.value("senderPacket").toString(), m_senderKey, blockIndex);
    if (endpoint == "receiver" && !m_receiverKey.isEmpty() && !m_receiverTrace.isEmpty())
        return blockIndex == 0 ? m_receiverTrace : m_service->tracePacket(m_state.value("receivedPacket").toString(), m_receiverKey, blockIndex);
    return {{"success", false}, {"error", QStringLiteral("该节点尚无可查看的加解密过程")}};
}

bool TransmissionSession::fail(const QString &error) {
    m_state["error"] = error;
    emit stateChanged();
    return false;
}

void TransmissionSession::event(const QString &description) {
    QVariantList events = m_state.value("events").toList();
    events.append(description);
    m_state["events"] = events;
}

bool TransmissionSession::start(QString plaintext, QString key, bool encrypted,
                                QVariantList route, int observer) {
    if (route.size() < 2) return fail(QStringLiteral("起点与终点之间没有可用路径"));
    QSet<int> seen;
    for (QVariant &item : route) {
        bool valid = false;
        const int node = item.toInt(&valid);
        if (!valid || node <= 0 || seen.contains(node))
            return fail(QStringLiteral("传输路径包含无效或重复节点"));
        seen.insert(node);
        item = node;
    }
    if (observer != -1 && (!seen.contains(observer) || observer == route.first().toInt()
                           || observer == route.last().toInt()))
        return fail(QStringLiteral("观察节点必须位于传输路径中间"));
    if (plaintext.isEmpty() || plaintext.toUtf8().size() > 16384)
        return fail(QStringLiteral("请输入 1 至 16 KB 的消息"));
    QString packet = plaintext;
    QByteArray bytes = plaintext.toUtf8();
    if (encrypted) {
        const QVariantMap result = m_service->encrypt(plaintext, key);
        if (!result.value("success").toBool()) return fail(result.value("error").toString());
        packet = result.value("packet").toString();
        bytes = QByteArray::fromBase64(packet.toLatin1());
    }
    m_senderTrace.clear(); m_receiverTrace.clear();
    m_senderKey = encrypted ? key : QString{}; m_receiverKey.clear();
    m_state = {{"phase", "transit"}, {"packetId", ++m_sequence}, {"original", plaintext},
               {"encrypted", encrypted}, {"route", route}, {"hop", 0},
               {"currentNode", route.first()}, {"source", route.first()}, {"target", route.last()},
               {"observer", observer}, {"senderPacket", packet}, {"wirePacket", packet},
               {"packetBytes", bytes.size()}, {"tampered", false}, {"events", QVariantList{}}};
    if (encrypted) {
        m_state["nonce"] = QString::fromLatin1(bytes.left(12).toHex(' '));
        m_state["tag"] = QString::fromLatin1(bytes.mid(12, 16).toHex(' '));
        m_state["ciphertext"] = QString::fromLatin1(bytes.mid(28).toHex(' '));
        m_senderTrace = m_service->tracePacket(packet, key, 0);
    }
    event(QStringLiteral("R%1 %2，生成包 #%3（%4 字节）")
          .arg(route.first().toInt()).arg(encrypted ? QStringLiteral("完成 AES 加密") : QStringLiteral("发送明文"))
          .arg(m_sequence).arg(bytes.size()));
    emit stateChanged();
    return true;
}

bool TransmissionSession::advance() {
    if (m_state.value("phase") != "transit") return false;
    const QVariantList route = m_state.value("route").toList();
    const int hop = m_state.value("hop").toInt() + 1;
    if (hop >= route.size()) return false;
    m_state["hop"] = hop;
    m_state["currentNode"] = route[hop];
    m_state["error"] = "";
    if (hop == route.size() - 1) {
        m_state["receivedPacket"] = m_state.value("wirePacket");
        m_state["receivedMatchesSender"] = m_state.value("receivedPacket") == m_state.value("senderPacket");
        m_state["phase"] = "received";
        event(QStringLiteral("R%1 已收到%2，等待%3")
              .arg(route[hop].toInt()).arg(m_state.value("encrypted").toBool() ? QStringLiteral("密文包") : QStringLiteral("明文包"))
              .arg(m_state.value("encrypted").toBool() ? QStringLiteral("解密校验") : QStringLiteral("读取")));
    } else if (route[hop].toInt() == m_state.value("observer").toInt()) {
        m_state["capturedPacket"] = m_state.value("wirePacket");
        m_state["phase"] = "intercepted";
        event(QStringLiteral("R%1 截获本次传输包，暂停转发").arg(route[hop].toInt()));
    } else {
        event(QStringLiteral("R%1 原样转发数据包").arg(route[hop].toInt()));
    }
    emit stateChanged();
    return true;
}

bool TransmissionSession::forward() {
    if (m_state.value("phase") != "intercepted") return false;
    m_state["phase"] = "transit";
    m_state["error"] = "";
    event(m_state.value("tampered").toBool() ? QStringLiteral("转发已篡改的密文包") : QStringLiteral("截获查看后，原样转发"));
    emit stateChanged();
    return true;
}

bool TransmissionSession::tamper() {
    if (m_state.value("phase") != "intercepted" || !m_state.value("encrypted").toBool()
        || m_state.value("tampered").toBool()) return false;
    QByteArray bytes = QByteArray::fromBase64(m_state.value("wirePacket").toString().toLatin1());
    if (bytes.size() <= 28) return false;
    const int offset = bytes.size() - 1;
    const unsigned char before = static_cast<unsigned char>(bytes.at(offset));
    bytes[offset] = char(before ^ 1);
    m_state["byteOffset"] = offset;
    m_state["beforeByte"] = QStringLiteral("%1").arg(before, 2, 16, QLatin1Char('0')).toUpper();
    m_state["afterByte"] = QStringLiteral("%1").arg(before ^ 1, 2, 16, QLatin1Char('0')).toUpper();
    m_state["wirePacket"] = QString::fromLatin1(bytes.toBase64());
    m_state["tampered"] = true;
    event(QStringLiteral("密文第 %1 字节：%2 → %3（翻转 1 位）")
          .arg(offset - 28 + 1).arg(m_state.value("beforeByte").toString(), m_state.value("afterByte").toString()));
    emit stateChanged();
    return true;
}

bool TransmissionSession::restore() {
    if (m_state.value("phase") != "intercepted") return false;
    m_state["wirePacket"] = m_state.value("capturedPacket");
    m_state["tampered"] = false;
    event(QStringLiteral("恢复截获时的原始密文包"));
    emit stateChanged();
    return true;
}

bool TransmissionSession::decrypt(QString receiverKey) {
    const QString phase = m_state.value("phase").toString();
    if (phase != "received" && phase != "rejected" && phase != "decrypted")
        return fail(QStringLiteral("数据包尚未到达接收端"));
    m_state["plaintext"] = "";
    m_state["error"] = "";
    const QString received = m_state.value("receivedPacket").toString();
    if (m_state.value("encrypted").toBool()) {
        const QVariantMap result = m_service->decrypt(received, receiverKey);
        m_receiverKey = receiverKey;
        m_receiverTrace = m_service->tracePacket(received, receiverKey, 0);
        if (!result.value("success").toBool()) {
            m_state["phase"] = "rejected";
            event(QStringLiteral("接收端解密校验失败，未输出明文"));
            return fail(result.value("error").toString());
        }
        m_state["plaintext"] = result.value("plaintext");
    } else {
        m_state["plaintext"] = received;
    }
    m_state["matchesOriginal"] = m_state.value("plaintext") == m_state.value("original");
    m_state["phase"] = "decrypted";
    event(m_state.value("encrypted").toBool() ? QStringLiteral("认证通过，解密原文与发送快照一致") : QStringLiteral("读取明文，与发送快照一致"));
    emit stateChanged();
    return true;
}
