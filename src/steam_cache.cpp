#include "steam_cache.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

// Format references (independent implementation, no third-party parser code):
// https://github.com/xPaw/SteamAppInfo#appinfovdf
// https://github.com/mexus/steam-vdf-parser#binary-format-reference
namespace mpg {
namespace {
constexpr std::uint32_t kV40 = 0x07564428u, kV41 = 0x07564429u;
constexpr std::uint64_t kMaxFile = 4ull * 1024 * 1024 * 1024;
constexpr std::uint64_t kMaxTableBytes = 64ull * 1024 * 1024;
constexpr std::uint32_t kMaxKeys = 1024 * 1024;
constexpr std::uint32_t kMaxPayload = 8 * 1024 * 1024;
constexpr std::uint64_t kMaxRequestedBytes = 64ull * 1024 * 1024;
enum class Key : unsigned char { Other, AppInfo, Common, Type };

bool asciiEqual(const std::string& text, const char* expected) {
    std::size_t index = 0;
    for (const unsigned char byte : text) {
        const auto lower = byte >= 'A' && byte <= 'Z' ? byte + ('a' - 'A') : byte;
        if (!expected[index] || lower != static_cast<unsigned char>(expected[index++])) return false;
    }
    return expected[index] == 0;
}
Key keyOf(const std::string& text) {
    if (asciiEqual(text, "appinfo")) return Key::AppInfo;
    if (asciiEqual(text, "common")) return Key::Common;
    if (asciiEqual(text, "type")) return Key::Type;
    return Key::Other;
}
std::uint32_t little32(const unsigned char* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
        (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

class Reader {
public:
    explicit Reader(std::istream& input) : input_(input) {
        input_.clear(); input_.seekg(0, std::ios::end);
        const auto end = input_.tellg();
        if (end < 0 || static_cast<std::uint64_t>(end) > kMaxFile) return;
        size = static_cast<std::uint64_t>(end); limit = size;
        valid = seek(0);
    }
    bool seek(std::uint64_t offset) {
        if (offset > size) return false;
        input_.clear(); input_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!input_) return false;
        position = offset; return true;
    }
    bool read(void* data, std::size_t bytes) {
        if (position > limit || bytes > limit - position) return false;
        input_.read(static_cast<char*>(data), static_cast<std::streamsize>(bytes));
        if (input_.gcount() != static_cast<std::streamsize>(bytes)) return false;
        position += bytes; return true;
    }
    bool u32(std::uint32_t& value) {
        unsigned char bytes[4]{};
        if (!read(bytes, sizeof(bytes))) return false;
        value = little32(bytes); return true;
    }
    bool u64(std::uint64_t& value) {
        std::uint32_t low = 0, high = 0;
        if (!u32(low) || !u32(high)) return false;
        value = low | (static_cast<std::uint64_t>(high) << 32); return true;
    }
    bool stringKey(Key& result) {
        std::string prefix;
        for (unsigned length = 0; length < 65536; ++length) {
            unsigned char byte = 0;
            if (!read(&byte, 1)) return false;
            if (!byte) { result = keyOf(prefix); return true; }
            if (prefix.size() < 16) prefix.push_back(static_cast<char>(byte));
        }
        return false;
    }
    bool zeroTail(std::uint64_t end) {
        if (position > end || end - position > 4096) return false;
        while (position < end) {
            unsigned char byte = 0;
            if (!read(&byte, 1) || byte) return false;
        }
        return true;
    }
    std::uint64_t size = 0, position = 0, limit = 0;
    bool valid = false;
private:
    std::istream& input_;
};

std::uint32_t rotate(std::uint32_t value, unsigned bits) {
    return (value << bits) | (value >> (32 - bits));
}
// SHA-1 is used solely to check Steam's existing local-record checksum, not for
// authentication. Keep this small calculation portable so malformed fixtures
// exercise the exact same integrity check on Windows and other build hosts.
std::array<unsigned char, 20> binaryDigest(const std::vector<unsigned char>& bytes) {
    std::array<std::uint32_t, 5> state{0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u, 0xc3d2e1f0u};
    const auto block = [&](const unsigned char* data) {
        std::array<std::uint32_t, 80> words{};
        for (unsigned i = 0; i < 16; ++i)
            words[i] = (static_cast<std::uint32_t>(data[i*4]) << 24) | (static_cast<std::uint32_t>(data[i*4+1]) << 16) |
                (static_cast<std::uint32_t>(data[i*4+2]) << 8) | data[i*4+3];
        for (unsigned i = 16; i < 80; ++i) words[i] = rotate(words[i-3] ^ words[i-8] ^ words[i-14] ^ words[i-16], 1);
        auto a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
        for (unsigned i = 0; i < 80; ++i) {
            const auto f = i < 20 ? (b & c) | (~b & d) : i < 40 ? b ^ c ^ d : i < 60 ? (b & c) | (b & d) | (c & d) : b ^ c ^ d;
            const auto k = i < 20 ? 0x5a827999u : i < 40 ? 0x6ed9eba1u : i < 60 ? 0x8f1bbcdcu : 0xca62c1d6u;
            const auto next = rotate(a, 5) + f + e + k + words[i];
            e = d; d = c; c = rotate(b, 30); b = a; a = next;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
    };
    std::size_t offset = 0;
    while (bytes.size() - offset >= 64) { block(bytes.data() + offset); offset += 64; }
    std::array<unsigned char, 128> tail{};
    const auto remaining = bytes.size() - offset;
    std::copy(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end(), tail.begin());
    tail[remaining] = 0x80;
    const unsigned padded = remaining < 56 ? 64 : 128;
    const auto bits = static_cast<std::uint64_t>(bytes.size()) * 8;
    for (unsigned i = 0; i < 8; ++i) tail[padded - 1 - i] = static_cast<unsigned char>(bits >> (i * 8));
    block(tail.data()); if (padded == 128) block(tail.data() + 64);
    std::array<unsigned char, 20> result{};
    for (unsigned i = 0; i < 5; ++i) for (unsigned j = 0; j < 4; ++j)
        result[i*4+j] = static_cast<unsigned char>(state[i] >> (24 - j * 8));
    return result;
}

class Metadata {
public:
    Metadata(const std::vector<unsigned char>& data, const std::vector<Key>& keys, bool indexed)
        : data_(data), keys_(keys), indexed_(indexed) {}
    bool classify(bool& game) {
        const bool valid = entries(Scope::Root, 0, false) && position_ == data_.size();
        game = valid && typeSeen_ && game_;
        return valid;
    }
private:
    enum class Scope { Root, App, Common, Other };
    const std::vector<unsigned char>& data_;
    const std::vector<Key>& keys_;
    bool indexed_;
    std::size_t position_ = 0;
    unsigned nodes_ = 0;
    bool typeSeen_ = false, game_ = false;
    bool skip(std::size_t bytes) {
        if (bytes > data_.size() - position_) return false;
        position_ += bytes; return true;
    }
    bool string(std::string& result) {
        result.clear();
        for (unsigned length = 0; length < 1024 * 1024 && position_ < data_.size(); ++length) {
            const auto byte = data_[position_++];
            if (!byte) return true;
            if (result.size() < 16) result.push_back(static_cast<char>(byte));
        }
        return false;
    }
    bool key(Key& value) {
        if (indexed_) {
            if (data_.size() - position_ < 4) return false;
            const auto index = little32(data_.data() + position_); position_ += 4;
            if (index >= keys_.size()) return false;
            value = keys_[index]; return true;
        }
        std::string text;
        if (!string(text)) return false;
        value = keyOf(text); return true;
    }
    bool entries(Scope scope, unsigned depth, bool closingRequired) {
        if (depth > 32) return false;
        while (position_ < data_.size()) {
            const auto type = data_[position_++];
            if (type == 8) return true;
            if (++nodes_ > 250000) return false;
            Key name = Key::Other;
            if (!key(name)) return false;
            const bool classification = scope == Scope::Common && name == Key::Type;
            if (classification) {
                if (typeSeen_) return false;
                typeSeen_ = true;
            }
            if (type == 0) {
                auto next = Scope::Other;
                if (scope == Scope::Root && name == Key::AppInfo) next = Scope::App;
                else if ((scope == Scope::Root || scope == Scope::App) && name == Key::Common) next = Scope::Common;
                if (!entries(next, depth + 1, true)) return false;
            } else if (type == 1) {
                std::string text;
                if (!string(text)) return false;
                if (classification) game_ = asciiEqual(text, "game");
            } else if (type == 2 || type == 3 || type == 4 || type == 6) {
                if (!skip(4)) return false;
            } else if (type == 7) {
                if (!skip(8)) return false;
            } else if (type == 5) {
                bool terminated = false;
                for (unsigned chars = 0; chars < 512 * 1024; ++chars) {
                    if (data_.size() - position_ < 2) return false;
                    const auto value = data_[position_] | (static_cast<unsigned>(data_[position_ + 1]) << 8);
                    position_ += 2;
                    if (!value) { terminated = true; break; }
                }
                if (!terminated) return false;
            } else return false;
        }
        return !closingRequired;
    }
};

std::set<unsigned> parse(std::istream& input, const std::set<unsigned>& requested) {
    if (requested.empty() || requested.size() > 16384) return {};
    Reader reader(input);
    if (!reader.valid) return {};
    std::uint32_t version = 0, universe = 0;
    if (!reader.u32(version) || !reader.u32(universe) || universe != 1 || (version != kV40 && version != kV41)) return {};
    std::vector<Key> keys;
    std::uint64_t entriesEnd = reader.size;
    if (version == kV41) {
        if (!reader.u64(entriesEnd) || entriesEnd < 20 || entriesEnd > reader.size || reader.size - entriesEnd > kMaxTableBytes) return {};
        if (!reader.seek(entriesEnd)) return {};
        std::uint32_t count = 0;
        if (!reader.u32(count) || count > kMaxKeys || count > reader.size - reader.position) return {};
        keys.resize(count);
        for (auto& key : keys) if (!reader.stringKey(key)) return {};
        if (!reader.zeroTail(reader.size) || !reader.seek(16)) return {};
    }
    reader.limit = entriesEnd;
    std::set<unsigned> result, seenRequested;
    std::uint64_t requestedBytes = 0;
    for (unsigned records = 0; records < 1000000; ++records) {
        std::uint32_t appId = 0, size = 0;
        if (!reader.u32(appId)) return {};
        if (!appId) return reader.zeroTail(entriesEnd) ? result : std::set<unsigned>{};
        if (!reader.u32(size) || size < 60 || size > entriesEnd - reader.position) return {};
        const auto next = reader.position + size;
        if (requested.count(appId)) {
            if (!seenRequested.insert(appId).second || size - 60 > kMaxPayload) return {};
            requestedBytes += size - 60;
            if (requestedBytes > kMaxRequestedBytes) return {};
            std::array<unsigned char, 60> header{};
            if (!reader.read(header.data(), header.size())) return {};
            std::vector<unsigned char> payload(size - 60);
            if (!reader.read(payload.data(), payload.size())) return {};
            const auto digest = binaryDigest(payload);
            if (!std::equal(digest.begin(), digest.end(), header.begin() + 40)) return {};
            bool game = false;
            if (!Metadata(payload, keys, version == kV41).classify(game)) return {};
            if (game) result.insert(appId);
        }
        if (!reader.seek(next)) return {};
    }
    return {};
}
}

std::set<unsigned> parseSteamGameIds(std::istream& cache, const std::set<unsigned>& installedIds) {
    try { return parse(cache, installedIds); } catch (...) { return {}; }
}
std::set<unsigned> readSteamGameIds(const std::wstring& path, const std::set<unsigned>& installedIds) {
    try {
        if (installedIds.empty()) return {};
        std::ifstream file(std::filesystem::path(path), std::ios::binary);
        return file ? parseSteamGameIds(file, installedIds) : std::set<unsigned>{};
    } catch (...) { return {}; }
}
}
