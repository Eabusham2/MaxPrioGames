#include "model.hpp"
#include <cassert>
#include <iostream>
#include <set>

int main() {
    std::set<std::wstring> ids;
    std::set<unsigned> steamIds;
    std::set<unsigned> excluded{431960, 1366800, 1905180, 629520};
    std::set<std::wstring> generic{L"java.exe", L"javaw.exe", L"Agent.exe",
        L"Discovery.exe", L"ShooterGame.exe", L"Client-Win64-Shipping.exe"};
    unsigned games = 0, services = 0;
    for (const auto& entry : mpg::catalog()) {
        assert(!entry.id.empty() && !entry.name.empty());
        assert(ids.insert(entry.id).second);
        assert(!entry.executables.empty() || !entry.steamAppIds.empty());
        for (const auto id : entry.steamAppIds) {
            assert(id > 0 && excluded.count(id) == 0);
            assert(steamIds.insert(id).second);
        }
        for (const auto& exe : entry.executables) {
            assert(exe.find(L'*') == std::wstring::npos);
            assert(exe.find(L'?') == std::wstring::npos);
            if (generic.count(exe)) assert(!entry.pathHints.empty());
        }
        if (entry.service) { ++services; assert(!entry.pathHints.empty()); }
        else ++games;
    }
    assert(games >= 100 && steamIds.size() == 96 && services >= 1);
    assert(steamIds.count(730) && steamIds.count(570) && steamIds.count(1938090));
    assert(ids.count(L"fortnite") && ids.count(L"valorant") && ids.count(L"minecraft-java"));
    std::cout << "Catalog: " << games << " games, " << services << " service families verified.\n";
}
