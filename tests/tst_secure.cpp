#include <QtTest>

#include "SecureMessageService.h"

class SecureTests : public QObject {
    Q_OBJECT
private slots:
    void twoDifferentMessagesRoundTrip();
    void wrongKeyAndChangedPacketFail();
    void invalidInputsAreRejected();
    void deliveryMustPrecedeDecryption();
    void interceptedCiphertextIsThePacketDelivered();
    void plaintextComparisonAndIndependentReceiverKey();
    void invalidRouteDoesNotDestroyAnExistingPacket();
    void aesStagesMatchThePublishedAes256Vector();
    void authenticationTraceMatchesTheGcmVector();
    void livePacketTraceAgreesWithWindowsEncryption();
    void changedPacketsExplainFailureWithoutReleasingPlaintext();
    void routeEditsReplayDownstreamAndRefreshReceiver();
    void insertedBytesDoNotFreezeUnchangedDownstreamContent();
    void arbitraryInsertionsAndDeletionsReachReceiver();
};

namespace {
bool startChannel(QObject *channel, const QString &message, const QString &key, bool encrypted,
                  const QVariantList &route, int observer) {
    bool result = false;
    QMetaObject::invokeMethod(channel, "start", Q_RETURN_ARG(bool, result),
        Q_ARG(QString, message), Q_ARG(QString, key), Q_ARG(bool, encrypted),
        Q_ARG(QVariantList, route), Q_ARG(int, observer));
    return result;
}

}

void SecureTests::aesStagesMatchThePublishedAes256Vector() {
    SecureMessageService service;
    QVariantMap trace;
    const QString key = QStringLiteral("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    QVERIFY2(QMetaObject::invokeMethod(&service, "traceAes", Q_RETURN_ARG(QVariantMap, trace),
        Q_ARG(QString, QStringLiteral("00112233445566778899aabbccddeeff")), Q_ARG(QString, key)),
        "Clickable AES operations need a computed trace");
    QVERIFY(trace.value("success").toBool());
    QCOMPARE(trace.value("output").toString(), QStringLiteral("8ea2b7ca516745bfeafc49904b496089"));
    const QVariantList operations = trace.value("operations").toList();
    QCOMPARE(operations.size(), 56);
    QCOMPARE(operations.first().toMap().value("output").toString(), QStringLiteral("00102030405060708090a0b0c0d0e0f0"));
    QCOMPARE(trace.value("roundKeys").toList().size(), 15);
    for (int i = 1; i < operations.size(); ++i)
        QCOMPARE(operations[i].toMap().value("input"), operations[i - 1].toMap().value("output"));
    for (const QVariant &operation : operations)
        QVERIFY(!(operation.toMap().value("round").toInt() == 14 && operation.toMap().value("kind") == "mix"));
}

void SecureTests::authenticationTraceMatchesTheGcmVector() {
    SecureMessageService service;
    const QByteArray cipher = QByteArray::fromHex("cea7403d4d606b6e074ec5d3baf39d18");
    const QByteArray tag = QByteArray::fromHex("d0d1c8a799996bf0265b98b5d48ab919");
    const QString packet = QString::fromLatin1((QByteArray(12, 0) + tag + cipher).toBase64());
    QVariantMap trace;
    QVERIFY2(QMetaObject::invokeMethod(&service, "tracePacket", Q_RETURN_ARG(QVariantMap, trace),
        Q_ARG(QString, packet), Q_ARG(QString, QString(64, '0')), Q_ARG(int, 0)),
        "The GCM diagram must recalculate a real authentication tag");
    QVERIFY(trace.value("success").toBool());
    QVERIFY(trace.value("tagMatches").toBool());
    QVERIFY(trace.value("referenceMatches").toBool());
    QCOMPARE(trace.value("computedTag").toString(), QString::fromLatin1(tag.toHex()));
    QCOMPARE(trace.value("ghash").toString(), QStringLiteral("83de425c5edc5d498f382c441041ca92"));
    QCOMPARE(trace.value("plaintextHex").toString(), QString(32, '0'));
    QCOMPARE(trace.value("aes").toMap().value("output").toString(), QString::fromLatin1(cipher.toHex()));
}

void SecureTests::livePacketTraceAgreesWithWindowsEncryption() {
    SecureMessageService service;
    const QString key = service.generateKey().value("key").toString();
    for (const QString &message : {QStringLiteral("A"), QString(15, 'b'), QString(16, 'c'),
                                  QString(17, 'd'), QStringLiteral("现场输入：实验室口令 #8642 🔑")}) {
        const QString packet = service.encrypt(message, key).value("packet").toString();
        const int lastBlock = (message.toUtf8().size() - 1) / 16;
        QVariantMap trace;
        QVERIFY(QMetaObject::invokeMethod(&service, "tracePacket", Q_RETURN_ARG(QVariantMap, trace),
            Q_ARG(QString, packet), Q_ARG(QString, key), Q_ARG(int, lastBlock)));
        QVERIFY(trace.value("referenceMatches").toBool());
        QVERIFY(trace.value("tagMatches").toBool());
        QCOMPARE(trace.value("plaintext").toString(), message);
        QCOMPARE(trace.value("blockIndex").toInt(), lastBlock);
        const QByteArray block = QByteArray::fromHex(trace.value("selectedCipher").toString().toLatin1());
        const QByteArray stream = QByteArray::fromHex(trace.value("aes").toMap().value("output").toString().toLatin1());
        const QByteArray clear = message.toUtf8().mid(lastBlock * 16, 16);
        QCOMPARE(block.size(), clear.size());
        for (int i = 0; i < clear.size(); ++i) QCOMPARE(char(block[i] ^ stream[i]), clear[i]);
    }
}

void SecureTests::changedPacketsExplainFailureWithoutReleasingPlaintext() {
    SecureMessageService service;
    const QString key(64, 'a');
    const QString original = service.encrypt(QStringLiteral("不能提前显示原文 #5271"), key).value("packet").toString();
    QByteArray modified = QByteArray::fromBase64(original.toLatin1());
    modified[modified.size() - 1] = char(modified.back() ^ 1);
    for (const auto &probe : {qMakePair(QString::fromLatin1(modified.toBase64()), key),
                              qMakePair(original, QString(64, 'b'))}) {
        QVariantMap trace;
        QVERIFY(QMetaObject::invokeMethod(&service, "tracePacket", Q_RETURN_ARG(QVariantMap, trace),
            Q_ARG(QString, probe.first), Q_ARG(QString, probe.second), Q_ARG(int, 0)));
        QVERIFY(trace.value("success").toBool());
        QVERIFY(trace.value("referenceMatches").toBool());
        QVERIFY(!trace.value("tagMatches").toBool());
        QVERIFY(trace.value("computedTag") != trace.value("receivedTag"));
        QVERIFY(trace.value("plaintext").toString().isEmpty());
        QVERIFY(trace.value("plaintextHex").toString().isEmpty());
        QVERIFY(trace.value("selectedPlain").toString().isEmpty());
    }
    QVariantMap inspected;
    const QString changed = QString::fromLatin1(modified.toBase64());
    QVERIFY(QMetaObject::invokeMethod(&service, "inspectPacket", Q_RETURN_ARG(QVariantMap, inspected), Q_ARG(QString, changed)));
    QCOMPARE(inspected.value("ciphertext").toString(), QString::fromLatin1(modified.mid(28).toHex()));
    QVERIFY(!inspected.contains("key"));
    QVERIFY(!inspected.contains("plaintext"));
}
namespace {
bool decryptChannel(QObject *channel, const QString &key) {
    bool result = false;
    QMetaObject::invokeMethod(channel, "decrypt", Q_RETURN_ARG(bool, result), Q_ARG(QString, key));
    return result;
}
QVariantMap state(QObject *channel) { return channel->property("state").toMap(); }
void deliver(QObject *channel) {
    for (int i = 0; i < 8; ++i) {
        const QString phase = state(channel).value("phase").toString();
        if (phase == "received") return;
        QMetaObject::invokeMethod(channel, phase == "intercepted" ? "forward" : "advance");
    }
}
}

void SecureTests::deliveryMustPrecedeDecryption() {
    SecureMessageService service;
    QObject *channel = service.property("channel").value<QObject *>();
    QVERIFY2(channel, "A real transport session is needed between encryption and decryption");
    const QString key(64, QLatin1Char('a'));
    QVERIFY(startChannel(channel, QStringLiteral("现场消息 7391"), key, true, {1, 3, 7, 8}, 3));
    QVERIFY(!decryptChannel(channel, key));
    QVERIFY(state(channel).value("receivedPacket").toString().isEmpty());
    QVERIFY(state(channel).value("plaintext").toString().isEmpty());
    QMetaObject::invokeMethod(channel, "advance");
    QCOMPARE(state(channel).value("phase").toString(), QStringLiteral("intercepted"));
    QCOMPARE(state(channel).value("currentNode").toInt(), 3);
    QVERIFY(!decryptChannel(channel, key));
    deliver(channel);
    QCOMPARE(state(channel).value("phase").toString(), QStringLiteral("received"));
    QVERIFY(state(channel).value("plaintext").toString().isEmpty());
    QVERIFY(decryptChannel(channel, key));
    QCOMPARE(state(channel).value("plaintext").toString(), QStringLiteral("现场消息 7391"));
}

void SecureTests::interceptedCiphertextIsThePacketDelivered() {
    SecureMessageService service;
    QObject *channel = service.property("channel").value<QObject *>();
    QVERIFY(channel);
    const QString key(64, QLatin1Char('b'));
    QVERIFY(startChannel(channel, QStringLiteral("机房温度 24℃"), key, true, {1, 3, 8}, 3));
    const QString original = state(channel).value("senderPacket").toString();
    QMetaObject::invokeMethod(channel, "tamper");
    QCOMPARE(state(channel).value("wirePacket").toString(), original);
    QMetaObject::invokeMethod(channel, "advance");
    QCOMPARE(state(channel).value("capturedPacket").toString(), original);
    QMetaObject::invokeMethod(channel, "tamper");
    const QString changed = state(channel).value("wirePacket").toString();
    QVERIFY(changed != original);
    QCOMPARE(state(channel).value("senderPacket").toString(), original);
    const QByteArray before = QByteArray::fromBase64(original.toLatin1());
    const QByteArray after = QByteArray::fromBase64(changed.toLatin1());
    QCOMPARE(before.size(), after.size());
    int changedBytes = 0;
    for (int i = 0; i < before.size(); ++i) {
        if (before[i] != after[i]) {
            ++changedBytes;
            QCOMPARE(static_cast<unsigned char>(before[i] ^ after[i]), 1);
            QVERIFY(i >= 28); // Change ciphertext, not Base64 syntax, nonce or tag.
        }
    }
    QCOMPARE(changedBytes, 1);
    deliver(channel);
    QCOMPARE(state(channel).value("receivedPacket").toString(), changed);
    QVERIFY(!decryptChannel(channel, key));
    QCOMPARE(state(channel).value("phase").toString(), QStringLiteral("rejected"));
    QVERIFY(state(channel).value("plaintext").toString().isEmpty());
}

void SecureTests::plaintextComparisonAndIndependentReceiverKey() {
    SecureMessageService service;
    QObject *channel = service.property("channel").value<QObject *>();
    QVERIFY(channel);
    const QString key(64, QLatin1Char('c'));
    QVERIFY(startChannel(channel, QStringLiteral("明文对照 #42"), {}, false, {1, 3, 8}, 3));
    QMetaObject::invokeMethod(channel, "advance");
    QCOMPARE(state(channel).value("capturedPacket").toString(), QStringLiteral("明文对照 #42"));
    deliver(channel);
    QVERIFY(decryptChannel(channel, {}));
    QCOMPARE(state(channel).value("plaintext").toString(), QStringLiteral("明文对照 #42"));
    QVERIFY(startChannel(channel, QStringLiteral("中文与符号 !?&"), key, true, {1, 8}, -1));
    deliver(channel);
    const QString received = state(channel).value("receivedPacket").toString();
    QVERIFY(!decryptChannel(channel, QString(64, QLatin1Char('d'))));
    QVERIFY(state(channel).value("plaintext").toString().isEmpty());
    QVERIFY(decryptChannel(channel, key));
    QCOMPARE(state(channel).value("receivedPacket").toString(), received);
    QCOMPARE(state(channel).value("plaintext").toString(), QStringLiteral("中文与符号 !?&"));
    // A detached verifier knows only the intercepted packet and key.
    QCOMPARE(service.decrypt(received, key).value("plaintext").toString(), QStringLiteral("中文与符号 !?&"));
}

void SecureTests::invalidRouteDoesNotDestroyAnExistingPacket() {
    SecureMessageService service;
    QObject *channel = service.property("channel").value<QObject *>();
    QVERIFY(channel);
    const QString key(64, QLatin1Char('e'));
    QVERIFY(startChannel(channel, "snapshot", key, true, {1, 3, 8}, 3));
    const QString packet = state(channel).value("senderPacket").toString();
    QVERIFY(!startChannel(channel, "new", key, true, {1, 8}, 9));
    QCOMPARE(state(channel).value("senderPacket").toString(), packet);
    QVERIFY(!startChannel(channel, "new", key, true, {1, 1}, -1));
    QCOMPARE(state(channel).value("senderPacket").toString(), packet);
}

void SecureTests::twoDifferentMessagesRoundTrip() {
    SecureMessageService service;
    const QString key = QStringLiteral("000102030405060708090a0b0c0d0e0f"
                                       "101112131415161718191a1b1c1d1e1f");
    const QStringList messages = {QStringLiteral("R1发给R2：校园网络已就绪"),
                                  QStringLiteral("实验消息 #2 / 价格 42.50，符号 !?&")};
    for (const QString &message : messages) {
        const QVariantMap sent = service.encrypt(message, key);
        QVERIFY2(sent.value("success").toBool(), qPrintable(sent.value("error").toString()));
        const QString packet = sent.value("packet").toString();
        QVERIFY(!packet.isEmpty());
        QVERIFY(!packet.contains(message));
        const QVariantMap received = service.decrypt(packet, key);
        QVERIFY2(received.value("success").toBool(), qPrintable(received.value("error").toString()));
        QCOMPARE(received.value("plaintext").toString(), message);
        const QVariantMap sentAgain = service.encrypt(message, key);
        QVERIFY(sentAgain.value("success").toBool());
        QVERIFY(sentAgain.value("packet").toString() != packet);
    }
}

void SecureTests::wrongKeyAndChangedPacketFail() {
    SecureMessageService service;
    const QString key(64, QLatin1Char('a'));
    const QVariantMap sent = service.encrypt(QStringLiteral("secret message"), key);
    QVERIFY(sent.value("success").toBool());
    const QString packet = sent.value("packet").toString();
    QVERIFY(!service.decrypt(packet, QString(64, QLatin1Char('b'))).value("success").toBool());
    QByteArray bytes = QByteArray::fromBase64(packet.toLatin1());
    QVERIFY(bytes.size() > 28);
    bytes[bytes.size() - 1] = char(bytes.at(bytes.size() - 1) ^ 1);
    QVERIFY(!service.decrypt(QString::fromLatin1(bytes.toBase64()), key).value("success").toBool());
}

void SecureTests::invalidInputsAreRejected() {
    SecureMessageService service;
    const QString key(64, QLatin1Char('c'));
    QVERIFY(!service.encrypt(QStringLiteral("hello"), QStringLiteral("short")).value("success").toBool());
    QVERIFY(!service.decrypt(QStringLiteral("not base64!"), key).value("success").toBool());
    QVERIFY(!service.decrypt(QString::fromLatin1(QByteArray(12, 'x').toBase64()), key)
                 .value("success").toBool());
    const QVariantMap generated = service.generateKey();
    QVERIFY(generated.value("success").toBool());
    QCOMPARE(generated.value("key").toString().size(), 64);
}

void SecureTests::routeEditsReplayDownstreamAndRefreshReceiver() {
    SecureMessageService service;
    QObject *channel = service.channel();
    const QString key(64, 'a');
    QVERIFY(startChannel(channel, "route edits", key, true, {1, 3, 7, 8}, -1));
    bool ok = false;
    QVERIFY(QMetaObject::invokeMethod(channel, "completeRoute", Q_RETURN_ARG(bool, ok), Q_ARG(QString, key)));
    QVERIFY(ok);
    QCOMPARE(state(channel).value("phase").toString(), QStringLiteral("decrypted"));
    const QString original = state(channel).value("senderPacket").toString();
    QByteArray bytes = QByteArray::fromBase64(original.toLatin1());
    auto edit = [&](int node, const QString &packet) {
        bool result = false;
        QMetaObject::invokeMethod(channel, "editNode", Q_RETURN_ARG(bool, result), Q_ARG(int, node), Q_ARG(QString, packet));
        return result;
    };
    bytes[29] = char(bytes[29] ^ 1);
    QVERIFY(edit(7, QString::fromLatin1(bytes.toBase64())));
    bytes[28] = char(bytes[28] ^ 2);
    QByteArray upstream = QByteArray::fromBase64(original.toLatin1());
    upstream[28] = char(upstream[28] ^ 2);
    QVERIFY(edit(3, QString::fromLatin1(upstream.toBase64())));
    const auto nodes = state(channel).value("nodeStates").toList();
    QCOMPARE(nodes[2].toMap().value("inputPacket").toString(), QString::fromLatin1(upstream.toBase64()));
    QCOMPARE(state(channel).value("receivedPacket").toString(), QString::fromLatin1(bytes.toBase64()));
    QCOMPARE(state(channel).value("phase").toString(), QStringLiteral("rejected"));
    QVERIFY(state(channel).value("plaintext").toString().isEmpty());
    QVERIFY(!edit(1, "bad"));
    QVERIFY(!edit(8, "bad"));
    QVERIFY(edit(3, original + "!"));
    QVERIFY(state(channel).value("receivedPacket").toString().endsWith('!'));
    QVERIFY(state(channel).value("error").toString().contains("Base64"));
    QVERIFY(edit(7, "not base64!"));
    QVERIFY(channel->property("receiverTrace").toMap().value("plaintext").toString().isEmpty());
    QVERIFY(state(channel).value("plaintext").toString().isEmpty());
    for (int node : {3, 7}) {
        QVERIFY(QMetaObject::invokeMethod(channel, "restoreNode", Q_RETURN_ARG(bool, ok), Q_ARG(int, node)));
        QVERIFY(ok);
    }
    QCOMPARE(state(channel).value("receivedPacket").toString(), original);
    QCOMPARE(state(channel).value("plaintext").toString(), QStringLiteral("route edits"));
    QVERIFY(!decryptChannel(channel, QString(64, 'b')));
    QVERIFY(!state(channel).value("matchesOriginal").toBool());
}

void SecureTests::insertedBytesDoNotFreezeUnchangedDownstreamContent() {
    SecureMessageService service;
    QObject *channel = service.channel();
    QVERIFY(startChannel(channel, "abcdefgh", {}, false, {1, 3, 7, 8}, -1));
    bool ok = false;
    QMetaObject::invokeMethod(channel, "completeRoute", Q_RETURN_ARG(bool, ok), Q_ARG(QString, QString{}));
    QVERIFY(ok);
    QMetaObject::invokeMethod(channel, "editNode", Q_RETURN_ARG(bool, ok), Q_ARG(int, 7), Q_ARG(QString, QStringLiteral("aXbcdefgYh")));
    QVERIFY(ok);
    QMetaObject::invokeMethod(channel, "editNode", Q_RETURN_ARG(bool, ok), Q_ARG(int, 3), Q_ARG(QString, QStringLiteral("abcQefgh")));
    QVERIFY(ok);
    QCOMPARE(state(channel).value("receivedPacket").toString(), QStringLiteral("aXbcQefgYh"));
    QCOMPARE(state(channel).value("plaintext").toString(), QStringLiteral("aXbcQefgYh"));
    QVERIFY(!state(channel).value("matchesOriginal").toBool());
}

void SecureTests::arbitraryInsertionsAndDeletionsReachReceiver() {
    SecureMessageService service;
    QObject *channel = service.channel();
    for (const auto &fixture : {qMakePair(QStringLiteral("abcdefgh"), QStringLiteral("abcgh")),
                               qMakePair(QStringLiteral("abcdefgh"), QStringLiteral("XbcdefgYh")),
                               qMakePair(QStringLiteral("abcdefgh"), QStringLiteral("abc12de34fgh56")),
                               qMakePair(QStringLiteral("abcdefgh"), QString{}),
                               qMakePair(QStringLiteral("中文网络"), QStringLiteral("中文X网络!"))}) {
        QVERIFY(startChannel(channel, fixture.first, {}, false, {1, 3, 7, 8}, -1));
        bool ok = false;
        QMetaObject::invokeMethod(channel, "completeRoute", Q_RETURN_ARG(bool, ok), Q_ARG(QString, QString{}));
        QVERIFY(ok);
        QMetaObject::invokeMethod(channel, "editNode", Q_RETURN_ARG(bool, ok), Q_ARG(int, 3), Q_ARG(QString, fixture.second));
        QVERIFY(ok);
        QCOMPARE(state(channel).value("receivedPacket").toString(), fixture.second);
        QCOMPARE(state(channel).value("plaintext").toString(), fixture.second);
    }
}

QTEST_MAIN(SecureTests)
#include "tst_secure.moc"
