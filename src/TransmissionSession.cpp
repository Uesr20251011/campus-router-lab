#include "TransmissionSession.h"
#include "SecureMessageService.h"
#include <QSet>
#include <functional>
#include <QVector>

namespace {
// Myers' bidirectional search finds a shared boundary in linear space.
QPair<int, int> diffBoundary(const QByteArray &a, const QByteArray &b) {
    const int n = int(a.size()), m = int(b.size()), depth = (n + m + 1) / 2;
    const int offset = depth + 1, size = 2 * depth + 3, delta = n - m;
    QVector<int> forward(size, -1), reverse(size, -1);
    forward[offset + 1] = reverse[offset + 1] = 0;
    const bool odd = delta % 2 != 0;
    int fStart = 0, fEnd = 0, rStart = 0, rEnd = 0;
    for (int d = 0; d <= depth; ++d) {
        for (int k = -d + fStart; k <= d - fEnd; k += 2) {
            const int p = offset + k;
            int x = k == -d || (k != d && forward[p - 1] < forward[p + 1])
                    ? forward[p + 1] : forward[p - 1] + 1;
            int y = x - k;
            while (x < n && y < m && x >= 0 && y >= 0 && a[x] == b[y]) { ++x; ++y; }
            forward[p] = x;
            if (x > n) fEnd += 2;
            else if (y > m) fStart += 2;
            else if (odd) {
                const int other = offset + delta - k;
                if (other >= 0 && other < size && reverse[other] >= 0 && x >= n - reverse[other])
                    return {x, y};
            }
        }
        for (int k = -d + rStart; k <= d - rEnd; k += 2) {
            const int p = offset + k;
            int x = k == -d || (k != d && reverse[p - 1] < reverse[p + 1])
                    ? reverse[p + 1] : reverse[p - 1] + 1;
            int y = x - k;
            while (x < n && y < m && x >= 0 && y >= 0 && a[n-x-1] == b[m-y-1]) { ++x; ++y; }
            reverse[p] = x;
            if (x > n) rEnd += 2;
            else if (y > m) rStart += 2;
            else if (!odd) {
                const int other = offset + delta - k;
                if (other >= 0 && other < size && forward[other] >= 0 && forward[other] >= n - x)
                    return {forward[other], forward[other] - (delta - k)};
            }
        }
    }
    return {-1, -1};
}
}

TransmissionSession::TransmissionSession(SecureMessageService *service)
    : QObject(service), m_service(service) { reset(); }

void TransmissionSession::reset() {
    m_edits.clear();
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
    m_edits.clear();
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
    m_state["matchesOriginal"] = false;
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
    event(m_state.value("matchesOriginal").toBool() ? QStringLiteral("接收结果与发送快照一致") : QStringLiteral("接收结果与发送快照不同"));
    emit stateChanged();
    return true;
}

bool TransmissionSession::completeRoute(QString receiverKey) {
    if (!m_state.value("packetId").toInt()) return false;
    m_state["staticRoute"] = true;
    m_receiverKey = receiverKey;
    rebuildRoute();
    return true;
}

bool TransmissionSession::editNode(int node, QString packet) {
    const QVariantList route = m_state.value("route").toList();
    const int index = route.indexOf(node);
    if (!m_state.value("staticRoute").toBool() || index <= 0 || index >= route.size() - 1)
        return fail(QStringLiteral("只能在本次路径的中间节点实施攻击"));
    if (packet.toUtf8().size() > 65536) return fail(QStringLiteral("修改后的包不能超过 64 KB"));
    const QString input = m_state.value("nodeStates").toList()[index].toMap().value("inputPacket").toString();
    const auto beforeDecoded = QByteArray::fromBase64Encoding(input.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    const auto afterDecoded = QByteArray::fromBase64Encoding(packet.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    NodeEdit edit;
    edit.binary = m_state.value("encrypted").toBool() && beforeDecoded && afterDecoded;
    const QByteArray before = edit.binary ? *beforeDecoded : input.toUtf8();
    const QByteArray after = edit.binary ? *afterDecoded : packet.toUtf8();
    edit.changes = packetEdits(before, after);
    edit.rawChanges = packetEdits(input.toUtf8(), packet.toUtf8());
    if (edit.changes.isEmpty()) m_edits.remove(node);
    else m_edits[node] = edit;
    event(QStringLiteral("R%1 更新包内容，重新计算所有后续节点").arg(node));
    rebuildRoute();
    return true;
}

bool TransmissionSession::restoreNode(int node) {
    if (!m_state.value("staticRoute").toBool() || !m_edits.contains(node)) return false;
    m_edits.remove(node);
    event(QStringLiteral("R%1 撤销本节点攻击，保留其他节点的修改").arg(node));
    rebuildRoute();
    return true;
}

void TransmissionSession::rebuildRoute() {
    const QVariantList route = m_state.value("route").toList();
    QVariantList nodes;
    QString packet = m_state.value("senderPacket").toString();
    for (int i = 0; i < route.size(); ++i) {
        const int node = route[i].toInt();
        const QString input = packet;
        if (m_edits.contains(node)) {
            const NodeEdit &edit = m_edits[node];
            const auto decoded = QByteArray::fromBase64Encoding(packet.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
            const bool binary = edit.binary && decoded;
            QByteArray bytes = binary ? *decoded : packet.toUtf8();
            // If upstream damaged Base64 syntax, replay the corresponding raw edits
            // without silently discarding the invalid characters.
            const auto &changes = edit.binary && !binary ? edit.rawChanges : edit.changes;
            // Apply from the end so insertions/deletions preserve the recorded byte offsets.
            for (auto it = changes.crbegin(); it != changes.crend(); ++it) {
                const int offset = qMin(it->offset, int(bytes.size()));
                bytes.replace(offset, qMin(it->removed, int(bytes.size()) - offset), it->replacement);
            }
            packet = binary ? QString::fromLatin1(bytes.toBase64()) : QString::fromUtf8(bytes);
        }
        nodes.append(QVariantMap{{"node", node}, {"inputPacket", input}, {"outputPacket", packet},
                                 {"attacked", m_edits.contains(node)},
                                 {"changed", packet != m_state.value("senderPacket").toString()}});
    }
    m_state["nodeStates"] = nodes;
    m_state["wirePacket"] = packet;
    m_state["receivedPacket"] = packet;
    m_state["receivedMatchesSender"] = packet == m_state.value("senderPacket").toString();
    m_state["tampered"] = !m_edits.isEmpty();
    m_state["hop"] = route.size() - 1;
    m_state["currentNode"] = route.last();
    m_state["phase"] = "received";
    m_state["matchesOriginal"] = false;
    m_receiverTrace.clear();
    decrypt(m_receiverKey);
}

QList<TransmissionSession::Edit> TransmissionSession::packetEdits(const QByteArray &before, const QByteArray &after) {
    QList<Edit> edits;
    if (before.size() == after.size()) {
        for (int i = 0; i < before.size(); ++i)
            if (before[i] != after[i]) edits.append({i, 1, after.mid(i, 1)});
        return edits;
    }
    std::function<void(QByteArray, QByteArray, int)> diff = [&](QByteArray a, QByteArray b, int base) {
        int prefix = 0, suffix = 0;
        while (prefix < qMin(a.size(), b.size()) && a[prefix] == b[prefix]) ++prefix;
        a.remove(0, prefix); b.remove(0, prefix); base += prefix;
        while (suffix < qMin(a.size(), b.size()) && a[a.size()-1-suffix] == b[b.size()-1-suffix]) ++suffix;
        a.chop(suffix); b.chop(suffix);
        if (a.isEmpty() || b.isEmpty()) {
            if (a != b) edits.append({base, int(a.size()), b});
            return;
        }
        const auto split = diffBoundary(a, b);
        if (split.first < 0 || (split.first == 0 && split.second == 0)
            || (split.first == a.size() && split.second == b.size())) {
            edits.append({base, int(a.size()), b});
            return;
        }
        diff(a.left(split.first), b.left(split.second), base);
        diff(a.mid(split.first), b.mid(split.second), base + split.first);
    };
    diff(before, after, 0);
    return edits;
}
