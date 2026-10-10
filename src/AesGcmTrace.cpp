#include "AesGcmTrace.h"
#include <QByteArray>
#include <QVariantList>
#include <array>
#include <cstdint>

namespace {
using Block = std::array<unsigned char, 16>;
using Keys = std::array<Block, 15>;
QVariantMap failure(const QString &text) { return {{"success", false}, {"error", text}}; }
QString hex(const QByteArray &bytes) { return QString::fromLatin1(bytes.toHex()); }
QByteArray bytes(const Block &block) {
    return QByteArray(reinterpret_cast<const char *>(block.data()), 16);
}
QString hex(const Block &block) { return hex(bytes(block)); }
Block block(const QByteArray &data) {
    Block result{};
    for (int i = 0; i < qMin(16, int(data.size())); ++i) result[i] = static_cast<unsigned char>(data[i]);
    return result;
}
QByteArray parseHex(const QString &text, int size) {
    const QByteArray encoded = text.trimmed().toLatin1();
    if (encoded.size() != size * 2) return {};
    for (char c : encoded)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return {};
    return QByteArray::fromHex(encoded);
}
unsigned char multiply8(unsigned char a, unsigned char b) {
    // GF(2^8) 乘法用于 AES 列混合：加法是异或，左移溢出时按
    // x^8+x^4+x^3+x+1 约简（低 8 位常数 0x1b）。
    unsigned char result = 0;
    for (int i = 0; i < 8; ++i) {
        if (b & 1) result ^= a;
        const bool high = a & 0x80;
        a = static_cast<unsigned char>(a << 1);
        if (high) a ^= 0x1b;
        b >>= 1;
    }
    return result;
}
const std::array<unsigned char, 256> &sbox() {
    // S 盒由有限域乘法逆元（非零 a 的 a^254）和仿射变换构造，
    // static table 只生成一次；0 的逆元按 AES 规则取 0。
    static const auto table = [] {
        std::array<unsigned char, 256> result{};
        for (int i = 0; i < 256; ++i) {
            unsigned char inverse = 0;
            if (i) {
                inverse = 1;
                unsigned char base = static_cast<unsigned char>(i);
                for (int exponent = 254; exponent; exponent >>= 1) {
                    if (exponent & 1) inverse = multiply8(inverse, base);
                    base = multiply8(base, base);
                }
            }
            auto rotate = [inverse](int n) {
                return static_cast<unsigned char>((inverse << n) | (inverse >> (8 - n)));
            };
            result[i] = inverse ^ rotate(1) ^ rotate(2) ^ rotate(3) ^ rotate(4) ^ 0x63;
        }
        return result;
    }();
    return table;
}
Keys expand(const QByteArray &key) {
    // AES-256：32 字节主密钥扩展为 15 组 16 字节轮密钥（第 0~14 轮）。
    // 每 32 字节进行 RotWord/SubWord/Rcon，半周期额外 SubWord，这是 256 位版特点。
    std::array<unsigned char, 240> words{};
    for (int i = 0; i < 32; ++i) words[i] = static_cast<unsigned char>(key[i]);
    unsigned char rcon = 1;
    for (int offset = 32; offset < 240; offset += 4) {
        std::array<unsigned char, 4> temp{};
        for (int j = 0; j < 4; ++j) temp[j] = words[offset - 4 + j];
        if (offset % 32 == 0) {
            const unsigned char first = temp[0];
            for (int j = 0; j < 3; ++j) temp[j] = sbox()[temp[j + 1]];
            temp[3] = sbox()[first];
            temp[0] ^= rcon;
            rcon = multiply8(rcon, 2);
        } else if (offset % 32 == 16) {
            for (auto &value : temp) value = sbox()[value];
        }
        for (int j = 0; j < 4; ++j) words[offset + j] = words[offset - 32 + j] ^ temp[j];
    }
    Keys result{};
    for (int round = 0; round < 15; ++round)
        for (int j = 0; j < 16; ++j) result[round][j] = words[round * 16 + j];
    return result;
}
Block xorBlocks(Block left, const Block &right) {
    for (int i = 0; i < 16; ++i) left[i] ^= right[i];
    return left;
}
Block encrypt(Block state, const Keys &keys, QVariantList *operations = nullptr) {
    // AES 状态按列存放：state[col*4+row]。先进行第 0 轮加轮密钥，
    // 再执行 14 轮 SubBytes→ShiftRows→MixColumns→AddRoundKey。
    // 最后一轮省略 MixColumns；operations 可选，用来逐工序保存输入/输出矩阵。
    auto record = [&](const Block &before, int round, const QString &kind) {
        if (!operations) return;
        QVariantMap row{{"round", round}, {"kind", kind}, {"input", hex(before)}, {"output", hex(state)}};
        if (kind == "key") row["roundKey"] = hex(keys[round]);
        operations->append(row);
    };
    Block before = state;
    state = xorBlocks(state, keys[0]);
    record(before, 0, "key");
    for (int round = 1; round <= 14; ++round) {
        before = state;
        for (auto &value : state) value = sbox()[value];
        record(before, round, "sub");
        before = state;
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col) state[col * 4 + row] = before[((col + row) % 4) * 4 + row];
        record(before, round, "shift");
        if (round != 14) {
            before = state;
            for (int col = 0; col < 4; ++col) {
                const int i = col * 4;
                const auto a = before[i], b = before[i + 1], c = before[i + 2], d = before[i + 3];
                state[i] = multiply8(a, 2) ^ multiply8(b, 3) ^ c ^ d;
                state[i + 1] = a ^ multiply8(b, 2) ^ multiply8(c, 3) ^ d;
                state[i + 2] = a ^ b ^ multiply8(c, 2) ^ multiply8(d, 3);
                state[i + 3] = multiply8(a, 3) ^ b ^ c ^ multiply8(d, 2);
            }
            record(before, round, "mix");
        }
        before = state;
        state = xorBlocks(state, keys[round]);
        record(before, round, "key");
    }
    return state;
}
QVariantMap aesTrace(const Block &input, const Keys &keys) {
    QVariantList operations, roundKeys;
    for (const Block &key : keys) roundKeys.append(hex(key));
    const Block output = encrypt(input, keys, &operations);
    return {{"success", true}, {"input", hex(input)}, {"output", hex(output)},
            {"operations", operations}, {"roundKeys", roundKeys}};
}
Block multiply128(const Block &x, const Block &h) {
    // GHASH 在 GF(2^128) 中相乘，逐位累加并右移；移出的最低位为 1 时
    // 用 0xe1 对应的约简多项式修正。它是认证计算，不是 AES 的列混合。
    Block z{}, v = h;
    for (int bit = 0; bit < 128; ++bit) {
        if (x[bit / 8] & (0x80 >> (bit % 8))) z = xorBlocks(z, v);
        const bool low = v[15] & 1;
        for (int i = 15; i >= 0; --i) v[i] = (v[i] >> 1) | (i > 0 && (v[i - 1] & 1) ? 0x80 : 0);
        if (low) v[0] ^= 0xe1;
    }
    return z;
}
void setCounter(Block &counter, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) counter[15 - i] = static_cast<unsigned char>(value >> (i * 8));
}
}

QVariantMap AesGcmTrace::inspect(const QString &encoded) {
    if (encoded.size() > 22000) return failure(QStringLiteral("数据包过大"));
    const auto decoded = QByteArray::fromBase64Encoding(encoded.trimmed().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || (*decoded).size() <= 28) return failure(QStringLiteral("数据包格式不完整"));
    const QByteArray data = *decoded;
    if (data.size() > 16412) return failure(QStringLiteral("数据包超过 16 KB 消息限制"));
    return {{"success", true}, {"nonce", hex(data.left(12))}, {"tag", hex(data.mid(12, 16))},
            {"ciphertext", hex(data.mid(28))}, {"packetBytes", data.size()},
            {"messageBytes", data.size() - 28}, {"blockCount", (data.size() - 28 + 15) / 16}};
}

QVariantMap AesGcmTrace::aes(const QString &inputHex, const QString &keyHex) {
    const QByteArray input = parseHex(inputHex, 16), key = parseHex(keyHex, 32);
    if (input.isEmpty() || key.isEmpty()) return failure(QStringLiteral("AES 工位需要 16 字节输入块和 32 字节密钥"));
    return aesTrace(block(input), expand(key));
}

QVariantMap AesGcmTrace::packet(const QString &encoded, const QString &keyHex, int selected) {
    QVariantMap result = inspect(encoded);
    if (!result.value("success").toBool()) return result;
    result["rawPacket"] = encoded;
    const QByteArray key = parseHex(keyHex, 32);
    if (key.isEmpty()) return failure(QStringLiteral("密钥必须是 64 个十六进制字符（256 位）"));
    const Keys keys = expand(key);
    const QByteArray nonce = QByteArray::fromHex(result.value("nonce").toString().toLatin1());
    const QByteArray cipher = QByteArray::fromHex(result.value("ciphertext").toString().toLatin1());
    const QByteArray tag = QByteArray::fromHex(result.value("tag").toString().toLatin1());
    const int count = result.value("blockCount").toInt();
    if (selected < 0 || selected >= count) return failure(QStringLiteral("请选择有效的数据块"));
    Block j0 = block(nonce);
    // 本实验 nonce 为 96 位：J0 = nonce || 00000001。
    // H = AES_K(0^128)，tagMask = AES_K(J0)，消息首块用 nonce || 00000002。
    setCounter(j0, 1);
    const Block h = encrypt(Block{}, keys), mask = encrypt(j0, keys);
    Block accumulator{};
    QVariantList hashSteps;
    auto hashBlock = [&](const Block &input, const QString &kind, int index) {
        // GHASH 递推：Y_i = (Y_(i-1) XOR X_i) * H。
        const Block before = accumulator;
        const Block combined = xorBlocks(accumulator, input);
        accumulator = multiply128(combined, h);
        hashSteps.append(QVariantMap{{"index", index}, {"kind", kind}, {"input", hex(input)},
                         {"before", hex(before)}, {"xor", hex(combined)}, {"output", hex(accumulator)}});
    };
    for (int i = 0; i < count; ++i) hashBlock(block(cipher.mid(i * 16, 16)), "cipher", i);
    // 本实验 AAD 为空；长度块前 64 位为 0，后 64 位是密文比特数，大端编码。
    // 不足 16 字节的密文尾块由 block() 右侧补零后参与 GHASH。
    Block lengths{};
    const std::uint64_t bits = std::uint64_t(cipher.size()) * 8;
    for (int i = 0; i < 8; ++i) lengths[15 - i] = static_cast<unsigned char>(bits >> (i * 8));
    hashBlock(lengths, "length", count);
    // 认证标签 = GHASH XOR AES_K(J0)。累积差异后统一判断，避免提前退出比较。
    const Block computed = xorBlocks(accumulator, mask);
    unsigned char difference = 0;
    for (int i = 0; i < 16; ++i) difference |= computed[i] ^ static_cast<unsigned char>(tag[i]);
    const bool accepted = difference == 0;
    Block selectedCounter = j0;
    setCounter(selectedCounter, std::uint32_t(selected + 2));
    const QVariantMap selectedAes = aesTrace(selectedCounter, keys);
    result["aes"] = selectedAes;
    result["key"] = hex(key);
    result["j0"] = hex(j0);
    result["h"] = hex(h);
    result["mask"] = hex(mask);
    result["lengthBlock"] = hex(lengths);
    result["ghash"] = hex(accumulator);
    result["hashSteps"] = hashSteps;
    result["receivedTag"] = hex(tag);
    result["computedTag"] = hex(computed);
    result["tagMatches"] = accepted;
    result["blockIndex"] = selected;
    result["counter"] = hex(selectedCounter);
    result["selectedCipher"] = hex(cipher.mid(selected * 16, 16));
    // 先验标签、后释放明文。GCM 的恢复过程也使用正向 AES 生成计数器密钥流，
    // 再与密文异或；所以结构图的接收端仍是正向 AES，而不是 InvSubBytes 等逆轮。
    if (accepted) {
        QByteArray clear = cipher;
        for (int i = 0; i < count; ++i) {
            Block counter = j0;
            setCounter(counter, std::uint32_t(i + 2));
            const Block stream = encrypt(counter, keys);
            for (int j = 0; j < 16 && i * 16 + j < clear.size(); ++j) clear[i * 16 + j] ^= char(stream[j]);
        }
        result["plaintext"] = QString::fromUtf8(clear);
        result["plaintextHex"] = hex(clear);
        result["selectedPlain"] = hex(clear.mid(selected * 16, 16));
    }
    return result;
}
