#include "SecureMessageService.h"
#include "TransmissionSession.h"
#include "AesGcmTrace.h"

#include <QByteArray>
#include <QString>

#ifdef Q_OS_WIN
#include <windows.h>
#include <bcrypt.h>
#endif

namespace {
constexpr int keySize = 32;
constexpr int nonceSize = 12;
constexpr int tagSize = 16;

QVariantMap failure(const QString &error) {
    return {{"success", false}, {"error", error}};
}

QByteArray parseKey(const QString &input) {
    const QByteArray hex = input.trimmed().toLatin1();
    if (hex.size() != keySize * 2) return {};
    for (char c : hex)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return {};
    return QByteArray::fromHex(hex);
}

#ifdef Q_OS_WIN
struct AesKey {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_KEY_HANDLE handle = nullptr;
    QByteArray object;

    ~AesKey() {
        if (handle) BCryptDestroyKey(handle);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    }

    bool open(QByteArray &material) {
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0)))
            return false;
        if (!BCRYPT_SUCCESS(BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE,
                         reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_GCM)),
                         sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) return false;
        ULONG objectLength = 0, returned = 0;
        if (!BCRYPT_SUCCESS(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                         reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength), &returned, 0)))
            return false;
        object.resize(int(objectLength));
        return BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(algorithm, &handle,
                                reinterpret_cast<PUCHAR>(object.data()), objectLength,
                                reinterpret_cast<PUCHAR>(material.data()), ULONG(material.size()), 0));
    }
};

BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo(QByteArray &nonce, QByteArray &tag) {
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = reinterpret_cast<PUCHAR>(nonce.data());
    info.cbNonce = ULONG(nonce.size());
    info.pbTag = reinterpret_cast<PUCHAR>(tag.data());
    info.cbTag = ULONG(tag.size());
    return info;
}
#endif
} // namespace

SecureMessageService::SecureMessageService(QObject *parent)
    : QObject(parent), m_channel(new TransmissionSession(this)) {}

QVariantMap SecureMessageService::inspectPacket(const QString &packet) const {
    return AesGcmTrace::inspect(packet);
}
QVariantMap SecureMessageService::traceAes(const QString &block, const QString &key) const {
    return AesGcmTrace::aes(block, key);
}
QVariantMap SecureMessageService::tracePacket(const QString &packet, const QString &key, int blockIndex) const {
    QVariantMap trace = AesGcmTrace::packet(packet, key, blockIndex);
    if (!trace.value("success").toBool()) return trace;
    const QVariantMap reference = decrypt(packet, key);
    const bool accepted = trace.value("tagMatches").toBool();
    const bool consistent = accepted == reference.value("success").toBool()
                         && (!accepted || trace.value("plaintext") == reference.value("plaintext"));
    trace["referenceMatches"] = consistent;
    if (!consistent) return failure(QStringLiteral("教学计算与实际 AES-GCM 结果不一致"));
    return trace;
}

QVariantMap SecureMessageService::generateKey() const {
#ifdef Q_OS_WIN
    QByteArray key(keySize, Qt::Uninitialized);
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(key.data()),
                                        ULONG(key.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
        return failure(QStringLiteral("无法生成随机密钥"));
    return {{"success", true}, {"key", QString::fromLatin1(key.toHex())}};
#else
    return failure(QStringLiteral("此版本只支持 Windows 加密服务"));
#endif
}

QVariantMap SecureMessageService::encrypt(const QString &plaintext, const QString &keyHex) const {
    QByteArray key = parseKey(keyHex);
    if (key.size() != keySize) return failure(QStringLiteral("密钥必须是 64 个十六进制字符（256 位）"));
    const QByteArray clear = plaintext.toUtf8();
    if (clear.isEmpty()) return failure(QStringLiteral("请输入要发送的消息"));
    if (clear.size() > 16384) return failure(QStringLiteral("消息不能超过 16 KB"));
#ifdef Q_OS_WIN
    QByteArray nonce(nonceSize, Qt::Uninitialized);
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(nonce.data()),
                                        ULONG(nonce.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
        return failure(QStringLiteral("无法生成随机数"));
    QByteArray tag(tagSize, 0);
    QByteArray cipher(clear.size(), 0);
    AesKey aes;
    if (!aes.open(key)) return failure(QStringLiteral("无法初始化 AES-GCM"));
    auto info = authInfo(nonce, tag);
    ULONG written = 0;
    const NTSTATUS status = BCryptEncrypt(aes.handle,
        reinterpret_cast<PUCHAR>(const_cast<char *>(clear.constData())), ULONG(clear.size()),
        &info, nullptr, 0, reinterpret_cast<PUCHAR>(cipher.data()), ULONG(cipher.size()),
        &written, 0);
    if (!BCRYPT_SUCCESS(status)) return failure(QStringLiteral("加密失败"));
    cipher.resize(int(written));
    const QByteArray packet = nonce + tag + cipher;
    return {{"success", true}, {"packet", QString::fromLatin1(packet.toBase64())}};
#else
    return failure(QStringLiteral("此版本只支持 Windows 加密服务"));
#endif
}

QVariantMap SecureMessageService::decrypt(const QString &packetText, const QString &keyHex) const {
    QByteArray key = parseKey(keyHex);
    if (key.size() != keySize) return failure(QStringLiteral("密钥必须是 64 个十六进制字符（256 位）"));
    const auto decoded = QByteArray::fromBase64Encoding(packetText.trimmed().toLatin1(),
                                                        QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded) return failure(QStringLiteral("传输数据不是有效的 Base64"));
    const QByteArray packet = *decoded;
    if (packet.size() <= nonceSize + tagSize)
        return failure(QStringLiteral("传输数据不完整"));
#ifdef Q_OS_WIN
    QByteArray nonce = packet.left(nonceSize);
    QByteArray tag = packet.mid(nonceSize, tagSize);
    QByteArray cipher = packet.mid(nonceSize + tagSize);
    QByteArray clear(cipher.size(), 0);
    AesKey aes;
    if (!aes.open(key)) return failure(QStringLiteral("无法初始化 AES-GCM"));
    auto info = authInfo(nonce, tag);
    ULONG written = 0;
    const NTSTATUS status = BCryptDecrypt(aes.handle,
        reinterpret_cast<PUCHAR>(cipher.data()), ULONG(cipher.size()), &info, nullptr, 0,
        reinterpret_cast<PUCHAR>(clear.data()), ULONG(clear.size()), &written, 0);
    if (!BCRYPT_SUCCESS(status)) return failure(QStringLiteral("解密失败：密钥错误或密文已被修改"));
    clear.resize(int(written));
    return {{"success", true}, {"plaintext", QString::fromUtf8(clear)}};
#else
    return failure(QStringLiteral("此版本只支持 Windows 加密服务"));
#endif
}
