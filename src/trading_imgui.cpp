
#include "trading_imgui.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include "naett/naett.h"
#include "parson/parson.h"

#include <stdio.h>
#include <time.h>
#include <string>
#include <vector>

static std::string gAccountId;

struct SummaryData
{
  double cashUSD;
  double cashSGD;
  double netLiquidationValSGD;
};

static int PollAuthStatus(int& out_statuscode)
{
  static naettReq* req = nullptr;
  static naettRes* res = nullptr;

  if (req == nullptr)
  {
    naettOption* options[] = {naettMethod("GET")};
    req =
      naettRequestWithOptions("https://localhost:5000/v1/api/iserver/auth/status",
                              sizeof(options) / sizeof(options[0]), (const naettOption**)&options);
    res = naettMake(req);
    return 0;
  }

  if (res && naettComplete(res))
  {
    out_statuscode = naettGetStatus(res);

    if (out_statuscode == 200)
    {
      int sz = 0;
      const void* body = naettGetBody(res, &sz);
      JSON_Value* val = json_parse_string((const char*)body);
      JSON_Object* obj = json_value_get_object(val);
      int authenticated = json_object_get_boolean(obj, "authenticated");
      json_value_free(val);

      naettFree(req);
      naettClose(res);
      req = nullptr;
      res = nullptr;

      return authenticated ? 1 : 0;
    }

    naettFree(req);
    naettClose(res);
    req = nullptr;
    res = nullptr;
    return 0;
  }

  return 0;
}

static int PollAccountId(std::string& out_accountId)
{
  static naettReq* req = nullptr;
  static naettRes* res = nullptr;

  if (req == nullptr)
  {
    naettOption* options[] = {naettMethod("GET")};
    req =
      naettRequestWithOptions("https://localhost:5000/v1/api/portfolio/accounts",
                              sizeof(options) / sizeof(options[0]), (const naettOption**)&options);
    res = naettMake(req);
    return 0;
  }

  if (res && naettComplete(res))
  {
    int status = naettGetStatus(res);
    if (status == 200)
    {
      int sz = 0;
      const void* body = naettGetBody(res, &sz);
      JSON_Value* val = json_parse_string((const char*)body);
      JSON_Array* arr = json_value_get_array(val);
      int n = json_array_get_count(arr);
      for (int i = 0; i < n; ++i)
      {
        JSON_Object* obj = json_array_get_object(arr, i);
        const char* id = json_object_get_string(obj, "accountId");
        if (id)
        {
          out_accountId = id;
          break;
        }
      }
      json_value_free(val);
    }

    naettFree(req);
    naettClose(res);
    req = nullptr;
    res = nullptr;
    return (status == 200 && !out_accountId.empty()) ? 1 : -1;
  }

  return 0;
}

struct PositionData
{
  std::string symbol;
  std::string secType;
  double size;
  double averageCost;
  double marketPrice;
  double marketValue;
  double realizedPNL;
  double unrealizedPNL;
};

static int PollPositions(const std::string& accountId, std::vector<PositionData>& out_positions)
{
  static naettReq* req = nullptr;
  static naettRes* res = nullptr;
  static int page = 0;
  static bool complete = false;

  if (complete)
    return 1;

  if (req == nullptr)
  {
    char url[256];
    snprintf(url, sizeof(url), "https://localhost:5000/v1/api/portfolio/%s/positions/%d",
             accountId.c_str(), page);
    naettOption* options[] = {naettMethod("GET")};
    req = naettRequestWithOptions(url, sizeof(options) / sizeof(options[0]),
                                  (const naettOption**)&options);
    res = naettMake(req);
    return 0;
  }

  if (res && naettComplete(res))
  {
    int status = naettGetStatus(res);
    if (status == 200)
    {
      int sz = 0;
      const void* body = naettGetBody(res, &sz);
      JSON_Value* val = json_parse_string((const char*)body);
      JSON_Array* arr = json_value_get_array(val);
      int n = json_array_get_count(arr);
      for (int i = 0; i < n; ++i)
      {
        JSON_Object* obj = json_array_get_object(arr, i);
        const char* contractDesc = json_object_get_string(obj, "contractDesc");
        double position = json_object_get_number(obj, "position");
        double avgCost = json_object_get_number(obj, "avgCost");
        double mktPrice = json_object_get_number(obj, "mktPrice");
        double mktValue = json_object_get_number(obj, "mktValue");
        double realizedPnl = json_object_get_number(obj, "realizedPnl");
        double unrealizedPnl = json_object_get_number(obj, "unrealizedPnl");

        std::string desc = contractDesc ? contractDesc : "";
        std::string symbol, secType;
        size_t firstSpace = desc.find(' ');
        if (firstSpace != std::string::npos)
        {
          symbol = desc.substr(0, firstSpace);
          size_t secondSpace = desc.find(' ', firstSpace + 1);
          if (secondSpace != std::string::npos)
            secType = desc.substr(firstSpace + 1, secondSpace - firstSpace - 1);
          else
            secType = desc.substr(firstSpace + 1);
        }
        else
          symbol = desc;

        PositionData pd;
        pd.symbol = symbol;
        pd.secType = secType;
        pd.size = position;
        pd.averageCost = avgCost;
        pd.marketPrice = mktPrice;
        pd.marketValue = mktValue;
        pd.realizedPNL = realizedPnl;
        pd.unrealizedPNL = unrealizedPnl;
        out_positions.push_back(pd);
      }
      json_value_free(val);

      if (n > 0)
        page++;
      else
        complete = true;
    }
    else
    {
      complete = true;
    }

    naettFree(req);
    naettClose(res);
    req = nullptr;
    res = nullptr;
  }

  return complete ? 1 : 0;
}

static int PollLedger(const std::string& accountId, SummaryData& out_summary)
{
  static naettReq* req = nullptr;
  static naettRes* res = nullptr;
  static bool done = false;

  if (done)
    return 1;

  if (req == nullptr)
  {
    char url[256];
    snprintf(url, sizeof(url), "https://localhost:5000/v1/api/portfolio/%s/ledger",
             accountId.c_str());
    naettOption* options[] = {naettMethod("GET")};
    req = naettRequestWithOptions(url, sizeof(options) / sizeof(options[0]),
                                  (const naettOption**)&options);
    res = naettMake(req);
    return 0;
  }

  if (res && naettComplete(res))
  {
    int status = naettGetStatus(res);
    if (status == 200)
    {
      int sz = 0;
      const void* body = naettGetBody(res, &sz);
      JSON_Value* val = json_parse_string((const char*)body);
      JSON_Object* obj = json_value_get_object(val);

      JSON_Object* cashObj = json_object_get_object(obj, "USD");
      if (cashObj)
        out_summary.cashUSD = json_object_get_number(cashObj, "cashbalance");

      cashObj = json_object_get_object(obj, "SGD");
      if (cashObj)
        out_summary.cashSGD = json_object_get_number(cashObj, "cashbalance");

      cashObj = json_object_get_object(obj, "BASE");
      if (cashObj)
        out_summary.netLiquidationValSGD = json_object_get_number(cashObj, "netliquidationvalue");

      json_value_free(val);
      done = true;
    }
    else
    {
      done = true;
    }

    naettFree(req);
    naettClose(res);
    req = nullptr;
    res = nullptr;
  }

  return done ? 1 : 0;
}

bool ConnectingState()
{
  ImGui::Begin("Connecting", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::NewLine();

  ImGui::Text("Run clientportal/run.bat to launch the client portal/server.");
  ImGui::Text("Pinging https://localhost:5000/v1/api/iserver/auth/status for alive check...");
  ImGui::NewLine();
  ImGui::NewLine();

  if (ImGui::Button("IBKR Sign in"))
  {
    ShellExecute(NULL, "open", "https://localhost:5000/", NULL, NULL, SW_SHOWNORMAL);
  }

  static int statuscode = -1;
  int result = PollAuthStatus(statuscode);

  ImGui::Text("statuscode: %d", statuscode);

  ImGui::End();
  return result == 1;
}

void PortfolioUI()
{
  static std::vector<PositionData> positions;
  static SummaryData summary;
  static bool posDone = false;
  static bool sumDone = false;

  if (!posDone)
  {
    int r = PollPositions(gAccountId, positions);
    if (r == 1)
      posDone = true;
  }

  if (!sumDone)
  {
    int r = PollLedger(gAccountId, summary);
    if (r == 1)
      sumDone = true;
  }

  ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImVec2(0, 400), ImGuiCond_FirstUseEver);
  ImGui::Begin(gAccountId.c_str());

  if (sumDone)
  {
    ImGui::Text("Cash Balance Total SGD: %g  USD: %g  NetLiq(SGD): %g", summary.cashSGD,
                summary.cashUSD, summary.netLiquidationValSGD);
    // ImGui::Text("Buying Power SGD: %s", summary.buyingPowerSGD.c_str());
  }
  else
  {
    ImGui::Text("Cash Balance Total SGD: ...   USD: ...   SGD: ...");
    ImGui::Text("Buying Power SGD: ...");
  }
  ImGui::Text("Positions: %d", (int)positions.size());

  static bool traded_this_session = false;
  ImGui::Checkbox("Traded This Session Only", &traded_this_session);

  if (ImGui::BeginTable("split", 10, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
  {
    ImGui::TableSetupColumn("Ticker");
    ImGui::TableSetupColumn("Type");
    ImGui::TableSetupColumn("Size");
    ImGui::TableSetupColumn("AverageCost");
    ImGui::TableSetupColumn("MarketPrice");
    ImGui::TableSetupColumn("MarketValue");
    ImGui::TableSetupColumn("RPnL");
    ImGui::TableSetupColumn("UnPnL");
    ImGui::TableSetupColumn("UnPnL%");
    ImGui::TableSetupColumn("Close?");

    ImGui::TableHeadersRow();

    for (int i = 0; i < (int)positions.size(); ++i)
    {
      auto& p = positions[i];

      ImGui::TableNextRow();

      ImGui::TableNextColumn();
      ImGui::Text(p.symbol.c_str());
      ImGui::TableNextColumn();
      ImGui::Text(p.secType.c_str());
      ImGui::TableNextColumn();

      char t[128] = {0};
      snprintf(t, sizeof(t), "%g", p.size);
      ImGui::Text(t);
      ImGui::TableNextColumn();

      snprintf(t, sizeof(t), "%g", p.averageCost);
      ImGui::Text(t);
      ImGui::TableNextColumn();

      snprintf(t, sizeof(t), "%g", p.marketPrice);
      ImGui::Text(t);
      ImGui::TableNextColumn();

      snprintf(t, sizeof(t), "%g", p.marketValue);
      ImGui::Text(t);
      ImGui::TableNextColumn();

      snprintf(t, sizeof(t), "%g", p.realizedPNL);
      ImGui::Text(t);
      ImGui::TableNextColumn();

      snprintf(t, sizeof(t), "%g", p.unrealizedPNL);
      double unrealized_pnl_percent = p.unrealizedPNL / (p.size * p.averageCost) * 100.0;
      char buf[128] = {0};
      snprintf(buf, sizeof(buf), "%.3g", unrealized_pnl_percent);

      if (p.unrealizedPNL > 0.0)
      {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), t);
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(0, 1, 0, 1), buf);
      }
      else
      {
        ImGui::Text(t);
        ImGui::TableNextColumn();
        ImGui::Text(buf);
      }

      ImGui::TableNextColumn();

      ImGui::PushID(i);
      if (ImGui::Button("Close")) {}
      ImGui::SameLine();
      if (ImGui::Button("TP")) {}
      ImGui::SameLine();
      if (ImGui::Button("SL")) {}
      ImGui::PopID();
    }
    ImGui::EndTable();
  }

  if (positions.empty() && !posDone)
  {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Loading positions...");
  }

  ImGui::End();
}

void MainState()
{
  static int todays_sizing = 100;
  static int target_max_loss = 100;
  static bool fresh_order_window = true;
  static bool contracts_info_window = true;

  if (ImGui::BeginMainMenuBar())
  {
    if (ImGui::BeginMenu("Sizing"))
    {
      ImGui::InputInt("Today's Size", &todays_sizing, 100, 1000);
      ImGui::InputInt("Target Max Loss", &target_max_loss, 100, 1000);
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Windows"))
    {
      ImGui::Checkbox("Fresh order window", &fresh_order_window);
      ImGui::Checkbox("Contract Info window", &contracts_info_window);
      ImGui::EndMenu();
    }

    time_t now = time(NULL);
    struct tm* now_tm = localtime(&now);
    const char* time_text = "\t\t\t\t\t\t\t%s\t\t\tAccountId:%s";
    if (now_tm->tm_sec >= 45)
      ImGui::TextColored(ImVec4(0, 1, 0, 1), time_text, asctime(now_tm), gAccountId.c_str());
    else
      ImGui::TextColored(ImVec4(1, 0, 0, 1), time_text, asctime(now_tm), gAccountId.c_str());

    ImGui::EndMainMenuBar();
  }

  static bool accountsDone = false;
  if (!accountsDone)
  {
    std::string id;
    int r = PollAccountId(id);
    if (r == 1)
    {
      gAccountId = id;
      accountsDone = true;
    }
  }

  if (!gAccountId.empty())
  {
    PortfolioUI();
  }
}