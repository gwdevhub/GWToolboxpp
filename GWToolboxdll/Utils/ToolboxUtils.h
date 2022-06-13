#pragma once

namespace GW {
    struct Player;

    template <typename T>
    class Array;
}

namespace ToolboxUtils {
    // Return player name without guild tag or player number
    std::wstring GetPlayerName(uint32_t player_number = 0);
    // Looks for player whilst sanitising for comparison
    GW::Player* GetPlayerByName(const wchar_t* _name);
    // Get buffer from world context for incoming chat message
    GW::Array<wchar_t>* GetMessageBuffer();
    // Get encoded message from world context for incoming chat message
    const wchar_t* GetMessageCore();
    // Clear incoming chat message buffer, usually needed when blocking an incoming message packet.
    void ClearMessageCore();
};
