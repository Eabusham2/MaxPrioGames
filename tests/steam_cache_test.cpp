#include "steam_cache.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
unsigned checks = 0;
void require(bool value, const char* message) {
    ++checks;
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void u32(std::string& out, unsigned value) {
    for (unsigned i = 0; i < 4; ++i) out.push_back(static_cast<char>(value >> (i * 8)));
}
void patch32(std::string& out, std::size_t offset, unsigned value) {
    for (unsigned i = 0; i < 4; ++i) out[offset + i] = static_cast<char>(value >> (i * 8));
}
void key(std::string& out, const char* value, bool indexed) {
    const std::array<std::string, 6> names{"appinfo", "common", "type", "config", "number", "misc"};
    if (indexed) {
        for (unsigned i = 0; i < names.size(); ++i) if (names[i] == value) { u32(out, i); return; }
        std::exit(2);
    }
    out += value; out.push_back(0);
}
std::string payload(bool indexed, const char* type, const char* scope = "common", unsigned extra = 0) {
    std::string out(1, 0); key(out, "appinfo", indexed);
    out.push_back(0); key(out, scope, indexed);
    out.push_back(1); key(out, "type", indexed); out += type; out.push_back(0);
    if (extra == 1) { out.push_back(1); key(out, "type", indexed); out += "Application"; out.push_back(0); }
    if (extra == 2) {
        out.push_back(2); key(out, "number", indexed); u32(out, 42);
        out.push_back(7); key(out, "misc", indexed); u32(out, 123456789); u32(out, 0);
    }
    if (extra == 3) { out.push_back(1); key(out, "misc", indexed); out += std::string(100, 'x'); out.push_back(0); }
    out.push_back(8); out.push_back(8);
    return out;
}
struct Record { unsigned id; std::string payload; const char* checksum; };
std::string file(bool indexed, const std::vector<Record>& records) {
    std::string out;
    u32(out, indexed ? 0x07564429u : 0x07564428u); u32(out, 1);
    if (indexed) { u32(out, 0); u32(out, 0); }
    for (const auto& record : records) {
        u32(out, record.id); u32(out, static_cast<unsigned>(60 + record.payload.size()));
        std::string header(60, 0); header[0] = 2;
        if (record.checksum) {
            const std::string digest(record.checksum);
            require(digest.size() == 40, "fixture checksum has twenty bytes");
            for (unsigned i = 0; i < 20; ++i)
                header[40 + i] = static_cast<char>(std::stoul(digest.substr(i * 2, 2), nullptr, 16));
        }
        out += header; out += record.payload;
    }
    u32(out, 0);
    if (indexed) {
        patch32(out, 8, static_cast<unsigned>(out.size()));
        u32(out, 6);
        for (const auto* name : {"appinfo", "common", "type", "config", "number", "misc"}) { out += name; out.push_back(0); }
    }
    return out;
}
std::set<unsigned> classify(const std::string& bytes, const std::set<unsigned>& ids = {100, 101, 102}) {
    std::istringstream input(bytes, std::ios::in | std::ios::binary);
    return mpg::parseSteamGameIds(input, ids);
}
class CountReads : public std::stringbuf {
public:
    explicit CountReads(const std::string& text) : std::stringbuf(text, std::ios::in) {}
    std::size_t bytes = 0;
protected:
    std::streamsize xsgetn(char* data, std::streamsize size) override {
        const auto count = std::stringbuf::xsgetn(data, size); bytes += static_cast<std::size_t>(count); return count;
    }
};
}

int main() {
    // Digests were generated independently with Python hashlib.sha1 from these
    // fixture bytes; tests do not reuse the implementation's digest function.
    const std::array<const char*, 2> gameHash{"5ed336b96f2f3238f1cad1ad6bc0ddf9a90a71b2", "4c1e13a8ff6176a4a260023c5203742b2d451bb2"};
    const std::array<const char*, 2> appHash{"77fe28595ffe663969cb857b2ee9605b3b38edb2", "a9d4709b9781e0fbbc9052448bb66cd831bd1789"};
    const std::array<const char*, 2> demoHash{"59465129b53ef8324e9f9db3de54d4ec18780339", "bd2ac3aca7396f7c92d158be5991cefccd227921"};
    const std::array<const char*, 2> lowerHash{"6452c25bf3cbae23c9888a7da656c7313770d4b4", "afa8d14d984d0231e3695cd4716b9548048dc69d"};
    const std::array<const char*, 2> wrongHash{"d2eb858d368861f0fab52f1a7f152e3167a1e02d", "ec1302a909c0f2df3f7ed7efc51a42bdbdde6e66"};
    const std::array<const char*, 2> duplicateHash{"e38d9cbcc8655eb70e2b72d615bfffbe8c848b95", "6b0afa5637bd5d1264c8ad151505beeed08ec07f"};
    const std::array<const char*, 2> numericHash{"024cd74598e2d891fa2dc2319b294bb0f5da9ad4", "eb18fc6825c75f191f1e60e1f42297ce2dd896a4"};
    const std::array<const char*, 2> longHash{"dd9e92e339a23717f194d73260df657409c68eb7", "945872ca51fcf35428b03e28389212619b522226"};
    const std::array<const char*, 2> missingEndHash{"466edeaefbdc38e0fa605c82d781929db483be5a", "5a5091300a06480c5b1c321c3019358798608fde"};
    const std::array<const char*, 2> outerEndHash{"03be77df005238042f8b98859005da3eb26d067a", "c98cb8dea321d96b62732ed763c659697aba51e0"};
    for (unsigned version = 0; version < 2; ++version) {
        const bool indexed = version != 0;
        const auto game = payload(indexed, "Game");
        const auto normal = file(indexed, {{100, game, gameHash[version]}, {101, payload(indexed, "Application"), appHash[version]}, {102, payload(indexed, "Demo"), demoHash[version]}});
        require(classify(normal) == std::set<unsigned>{100}, "Game accepted; Application and Demo excluded");
        require(classify(normal, {101}).empty(), "only requested installed app IDs inspected");
        require(classify(normal, {}).empty(), "empty installed set returns immediately");
        require(classify(file(indexed, {{100, payload(indexed, "game"), lowerHash[version]}})) == std::set<unsigned>{100}, "case-insensitive Game value");
        require(classify(file(indexed, {{100, payload(indexed, "Game", "config"), wrongHash[version]}})).empty(), "type in unrelated object cannot identify a game");
        require(classify(file(indexed, {{100, payload(indexed, "Game", "common", 1), duplicateHash[version]}})).empty(), "conflicting duplicate type rejects record");
        require(classify(file(indexed, {{100, payload(indexed, "Game", "common", 2), numericHash[version]}})) == std::set<unsigned>{100}, "integer fields and SHA-1 two-block padding handled");
        require(classify(file(indexed, {{100, payload(indexed, "Game", "common", 3), longHash[version]}})) == std::set<unsigned>{100}, "multi-block payload checksum verified");
        auto incomplete = game; incomplete.pop_back();
        require(classify(file(indexed, {{100, incomplete, missingEndHash[version]}})).empty(), "unterminated VDF rejected despite valid checksum");
        auto terminated = game; terminated.push_back(8);
        require(classify(file(indexed, {{100, terminated, outerEndHash[version]}})) == std::set<unsigned>{100}, "optional outer VDF terminator accepted");
        auto corrupt = normal; corrupt[(indexed ? 16 : 8) + 8 + 40] ^= 1;
        require(classify(corrupt).empty(), "binary SHA-1 mismatch rejects cache");
        corrupt = normal; patch32(corrupt, (indexed ? 16 : 8) + 4, 59);
        require(classify(corrupt).empty(), "undersized entry rejected");
        corrupt = normal; patch32(corrupt, (indexed ? 16 : 8) + 4, 0xffffffffu);
        require(classify(corrupt).empty(), "out-of-bounds entry rejected without allocation");
        corrupt = normal; patch32(corrupt, 0, 0x07564427u);
        require(classify(corrupt).empty(), "unsupported cache version rejected");
        corrupt = normal; patch32(corrupt, 4, 99);
        require(classify(corrupt).empty(), "unknown Steam universe rejected");
        const auto single = file(indexed, {{100, game, gameHash[version]}});
        for (std::size_t size = 0; size < single.size(); ++size)
            require(classify(single.substr(0, size)).empty(), "every truncated prefix rejected");
        require(classify(file(indexed, {{100, game, gameHash[version]}, {100, game, gameHash[version]}})).empty(), "duplicate requested app ID rejected");
        const auto large = file(indexed, {{999, std::string(1024 * 1024, 'x'), nullptr}, {100, game, gameHash[version]}});
        CountReads buffer(large); std::istream stream(&buffer);
        require(mpg::parseSteamGameIds(stream, {100}) == std::set<unsigned>{100}, "unrequested large record skipped");
        require(buffer.bytes < 1024, "large unrequested payload is sought past, not loaded");
    }
    auto indexed = file(true, {{100, payload(true, "Game"), gameHash[1]}});
    patch32(indexed, 8, 0xffffffffu);
    require(classify(indexed).empty(), "invalid string-table offset rejected");
    indexed = file(true, {{100, payload(true, "Game"), gameHash[1]}});
    const auto table = 16u + 8u + 60u + static_cast<unsigned>(payload(true, "Game").size()) + 4u;
    patch32(indexed, table, 0xffffffffu);
    require(classify(indexed).empty(), "oversized string table rejected before allocation");
    auto badKey = payload(true, "Game"); patch32(badKey, 1, 99);
    require(classify(file(true, {{100, badKey, "2a65a89a091bafde154273cb26164076441354bf"}})).empty(), "out-of-range indexed key rejected despite valid checksum");
    require(mpg::readSteamGameIds(L"this-path-does-not-exist/appinfo.vdf", {100}).empty(), "missing cache is silent and empty");
    std::cout << checks << " Steam appinfo classification checks passed\n";
}
