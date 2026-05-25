
#include "trading_imgui.h"
#include "im_api.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shellapi.h>

#include <stdio.h>
#include <time.h>


static std::string gAccountId;

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
  static bool auth = false;
  int result = PollAuthStatus(statuscode, auth);

  if (statuscode != 200)
    ImGui::Text("statuscode: %d", statuscode);
  else
    ImGui::Text("statuscode: %d (auth: %d)", statuscode, auth ? 1 : 0);

  ImGui::End();
  return result == 1;
}

void PortfolioUI()
{
  static std::vector<PositionData> positions;
  static SummaryData summary;
  static bool posDone = false;
  static bool sumDone = false;
  static float posRefreshTimer = -1.0f;

  if (posRefreshTimer < 0.f)
  {
    int r = PollPositions(gAccountId, positions);
    if (r == 1)
    {
      posDone = true;
      posRefreshTimer = 0.0f;
    }
  }
  else
  {
    posRefreshTimer += ImGui::GetIO().DeltaTime;
    PollPositions(gAccountId, positions);

    if (posRefreshTimer >= 5.0f)
    {
      PollPositions(gAccountId, positions, true);
      posRefreshTimer = 0.0f;
    }
  }

  if (!sumDone)
  {
    int r = PollLedger(gAccountId, summary);
    if (r == 1)
      sumDone = true;
  }
  PollSummary(gAccountId, summary);

  ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImVec2(0, 400), ImGuiCond_FirstUseEver);
  ImGui::Begin(gAccountId.c_str());

  if (sumDone)
  {
    ImGui::Text("Cash Balance Total SGD: %g  USD: %g  NetLiq(SGD): %g", summary.cashSGD,
                summary.cashUSD, summary.netLiquidationValSGD);
    ImGui::Text("Buying Power SGD: %g", summary.buyingPowerSGD);
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
      ImGui::TextWrapped(p.secType.c_str());
      ImGui::TableNextColumn();
      ImGui::Text(p.assetClass.c_str());
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
  static bool contracts_search_window = true;

  struct ChartHeader
  {
    int mConnId = 0;
    std::string mTicker;
    std::string mExchange;
  };
  static const int max_concurrent_charts = 5;
  static std::string graphs[max_concurrent_charts];
  static int concurrent_chart_count = 0;
  static std::vector<ChartHeader> chartheaders;

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
      ImGui::Checkbox("Contract Info window", &contracts_search_window);
      ImGui::DragInt("Max Concurrent Graphs", &concurrent_chart_count, 1, 1, 5);
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

  if (contracts_search_window)
  {
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(800, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Contract Search");

    static std::string ticker_result = "";
    static std::vector<ExchContractId> conids;
    static std::string full_stockname;
    static char symbol[64] = {0};
    bool changed =
      ImGui::InputText("Symbol", symbol, sizeof(symbol),
                       ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_EnterReturnsTrue |
                         ImGuiInputTextFlags_EscapeClearsAll);
    if (changed)
    {
      ticker_result = symbol;
      PollConId(symbol, conids, true);
    }
    else
    {
      static long long conid = -1;
      int ret = PollConId(symbol, conids);
    }

    ImGui::Separator();
    ImGui::NewLine();
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Contract search for: %s", ticker_result.c_str());

    if (ImGui::BeginTable("Contract search", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
    {
      ImGui::TableSetupColumn("Name");
      ImGui::TableSetupColumn("Exchange");
      ImGui::TableSetupColumn("Conid");
      ImGui::TableSetupColumn("Ops");

      ImGui::TableHeadersRow();

      for (int i = 0; i < (int)conids.size(); ++i)
      {
        auto& c = conids[i];

        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::TextWrapped(c.full_stockname.c_str());
        ImGui::TableNextColumn();
        ImGui::TextWrapped(c.exchange.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%d", c.conid);
        ImGui::TableNextColumn();

        ImGui::PushID(i);
        if (ImGui::Button("Add", ImVec2(-1.f, 30.f)))
        {
          // no repeats
          bool found = false;
          for (int j = 0; j < chartheaders.size(); ++j)
          {
            if (chartheaders[j].mConnId == c.conid)
            {
              found = true;
              break;
            }
          }
          if (!found)
            chartheaders.push_back({c.conid, symbol, c.exchange});
        }
        ImGui::PopID();
      }
      ImGui::EndTable();
    }

    ImGui::End();
  }

  // stock charts
  for (int i = 0; i < concurrent_chart_count; ++i)
  {

    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(0, 400 + i * 300), ImGuiCond_FirstUseEver);

    char title[128] = {0};
    snprintf(title, sizeof(title), "Chart #%d", i + 1);
    ImGui::Begin(title);

    if (ImGui::BeginTabBar("ChartTabs"))
    {
      for (int n = 0; n < (int)chartheaders.size(); n++)
      {
        bool open = true;
        if (ImGui::BeginTabItem(chartheaders[n].mTicker.c_str(), &open))
        {
          ImGui::Text("ConId: %d", chartheaders[n].mConnId);
          ImGui::Text("Exchange: %s", chartheaders[n].mExchange.c_str());
          ImGui::EndTabItem();
        }
        if (!open)
        {
          chartheaders.erase(chartheaders.begin() + n);
          n--;
        }
      }
      ImGui::EndTabBar();
    }

    ImGui::End();
  }
}