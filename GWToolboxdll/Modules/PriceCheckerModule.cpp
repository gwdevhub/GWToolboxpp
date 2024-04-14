#include "stdafx.h"

#include <Logger.h>
#include <Modules/PriceCheckerModule.h>
#include <GWCA/GameEntities/Item.h>
#include <CurlWrapper.h>
#include <ctime>

using nlohmann::json;

namespace {
    std::mutex mutex;
    std::string trader_quotes_url = "https://kamadan.gwtoolbox.com/trader_quotes";
    json* price_json = nullptr;
    std::time_t last_request_time = std::time(0);
}

void PriceChecker::Initialize()
{
    ToolboxModule::Initialize();
    InitCurl();
}

void PriceChecker::Terminate()
{
    ToolboxModule::Terminate();
}

static void EnsureLatestPriceJson()
{
    if (!mutex.try_lock()) {
        return;
    }

    const auto now = std::time(0);
    if (!price_json || difftime(now, last_request_time) > 3600) {}

    auto curl_easy = CurlEasy::CurlEasy();
    curl_easy.SetUrl(trader_quotes_url.c_str());
    curl_easy.SetMethod(HttpMethod::Get);
    curl_easy.Perform();
    const auto& content = curl_easy.GetContent();
    json response = content;
    price_json = &response;
    mutex.unlock();
}

void PriceChecker::Update(const float)
{
    EnsureLatestPriceJson();
}

int PriceChecker::GetPrice(GW::ItemModifier)
{
    if (!price_json) {
        return 0;
    }


    return 0;
}
