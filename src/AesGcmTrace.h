#pragma once
#include <QString>
#include <QVariantMap>

// Educational calculations, kept separate from the Windows CNG transport cipher.
// FIPS 197 column-major state and SP 800-38D GCM, with a 96-bit nonce and no AAD.
namespace AesGcmTrace {
QVariantMap inspect(const QString &packet);
QVariantMap aes(const QString &blockHex, const QString &keyHex);
QVariantMap packet(const QString &encodedPacket, const QString &keyHex, int blockIndex);
}
