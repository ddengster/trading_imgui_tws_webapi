
#include "trading_imgui.h"
#include "im_api.h"
#include "implot/implot.h"
#include "implot/implot_internal.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shellapi.h>

#include <stdio.h>
#include <time.h>
#include <ctime>
#include <unordered_map>


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

template<typename T>
int BinarySearch(const T* arr, int l, int r, T x)
{
  if (r >= l)
  {
    int mid = l + (r - l) / 2;
    if (arr[mid] == x)
      return mid;
    if (arr[mid] > x)
      return BinarySearch(arr, l, mid - 1, x);
    return BinarySearch(arr, mid + 1, r, x);
  }
  return -1;
}

int VolumeFormatter(double value, char* buffer, int buffer_size, void*)
{
  static double v[] = {1000000000000, 1000000000, 1000000, 1000, 1};
  static const char* p[] = {"T", "B", "M", "k", ""};
  if (value == 0)
  {
    return snprintf(buffer, buffer_size, "0");
  }
  for (int i = 0; i < 5; ++i)
  {
    if (fabs(value) >= v[i])
    {
      return snprintf(buffer, buffer_size, "%g%s", value / v[i], p[i]);
    }
  }
  return snprintf(buffer, buffer_size, "%g%s", value / v[4], p[4]);
}

int TimeStampFormatter(double ts_ms, char* buffer, int buffer_size, void*)
{
  std::time_t time_val = static_cast<std::time_t>(ts_ms / 1000.0);
  std::tm* time_ptr = std::localtime(&time_val);
  std::tm time_info = *time_ptr;

  return snprintf(buffer, buffer_size, "%d:%02d", time_ptr->tm_hour, time_ptr->tm_min);
}

void TickerTooltip(const std::vector<MarketDataPoint>& data, const std::vector<double>& time,
                   bool span_subplots = false)
{
  ImDrawList* draw_list = ImPlot::GetPlotDrawList();

  ImPlotRect limits = ImPlot::GetPlotLimits();
  double plotwidth = limits.X.Max - limits.X.Min;
  double widthperbar = plotwidth / (double)time.size();
  double half_width = widthperbar * 0.5;

  const bool hovered = span_subplots ? ImPlot::IsSubplotsHovered() : ImPlot::IsPlotHovered();
  if (hovered)
  {
    ImPlotPoint mouse = ImPlot::GetPlotMousePos();
    mouse.x = ImPlot::RoundTime(ImPlotTime::FromDouble(mouse.x), ImPlotTimeUnit_Min).ToDouble();
    float tool_l = ImPlot::PlotToPixels(mouse.x - half_width, mouse.y).x;
    float tool_r = ImPlot::PlotToPixels(mouse.x + half_width, mouse.y).x;
    float tool_t = ImPlot::GetPlotPos().y;
    float tool_b = tool_t + ImPlot::GetPlotSize().y;
    ImPlot::PushPlotClipRect();
    draw_list->AddRectFilled(ImVec2(tool_l, tool_t), ImVec2(tool_r, tool_b),
                             IM_COL32(128, 128, 128, 64));
    ImPlot::PopPlotClipRect();
    // find mouse location index
    int idx = BinarySearch(time.data(), 0, time.size() - 1, mouse.x);
    /*
    printf("idx: %d\n", idx);
    ImGui::BeginTooltip();
    ImGui::Text("asdsa");
    ImGui::EndTooltip();
    */
    // render tool tip (won't be affected by plot clip rect)
    if (ImPlot::IsPlotHovered() && idx != -1)
    {
      ImGui::BeginTooltip();
      char buff[32];
      ImPlot::FormatDate(ImPlotTime::FromDouble(time[idx]), buff, 32, ImPlotDateFmt_DayMoYr,
                         ImPlot::GetStyle().UseISO8601);
      ImGui::Text("Date:");
      ImGui::SameLine(60);
      ImGui::Text("%s", buff);
      ImGui::Text("Open:");
      ImGui::SameLine(60);
      ImGui::Text("$%.2f", data[idx].open);
      ImGui::Text("Close:");
      ImGui::SameLine(60);
      ImGui::Text("$%.2f", data[idx].close);
      ImGui::Text("High:");
      ImGui::SameLine(60);
      ImGui::Text("$%.2f", data[idx].high);
      ImGui::Text("Low:");
      ImGui::SameLine(60);
      ImGui::Text("$%.2f", data[idx].low);
      ImGui::Text("Volume:");
      ImGui::SameLine(60);
      char buf[64] = {};
      VolumeFormatter(data[idx].volume, buf, sizeof(buf), nullptr);
      ImGui::Text(buf);
      ImGui::EndTooltip();
    }
  }
}

void PlotStockChart(const std::vector<MarketDataPoint>& data)
{
  if (data.empty())
    return;

  static float ratios[] = {2, 1};
  if (ImPlot::BeginSubplots("##Stocks", 2, 1, ImVec2(-1, -1), ImPlotSubplotFlags_LinkCols, ratios))
  {
    ImVec4 greenc(0, 1, 0, 1), redc(1, 0, 0, 1);
    std::vector<double> time, volume, open, close;
    for (int i = 0; i < data.size(); ++i)
    {
      time.push_back(data[i].timestamp);
      volume.push_back(data[i].volume);
      open.push_back(data[i].open);
      close.push_back(data[i].close);
    }

    if (ImPlot::BeginPlot("##OHLCPlot"))
    {
      ImPlot::SetupAxes(0, 0, ImPlotAxisFlags_NoTickLabels,
                        ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit |
                          ImPlotAxisFlags_Opposite);
      ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Linear);
      ImPlot::SetupAxisLimits(ImAxis_X1, data[0].timestamp, data.back().timestamp,
                              ImGuiCond_Always);
      ImPlot::SetupAxisFormat(ImAxis_X1, TimeStampFormatter, nullptr);
      ImPlot::SetupAxisFormat(ImAxis_Y1, "$%.2f");

      TickerTooltip(data, time, true);

      /*
      * // BOLLINGER BANDS
      ImPlot::SetNextFillStyle(ImVec4(0.5, 0.5, 1, 1), 0.25f);
      ImPlot::PlotShaded("BB", data.time.data(), data.bollinger_top.data(),
                         data.bollinger_bot.data(), data.size());
      ImPlot::SetNextLineStyle(ImVec4(0.5, 0.5, 1, 1));
      ImPlot::PlotLine("BB", data.time.data(), data.bollinger_mid.data(), data.size());
      */

      // CANDLES
      {
        ImDrawList* draw_list = ImPlot::GetPlotDrawList();
        // calc real value width
        const double half_width = time.size() > 1 ? (time[1] - time[0]) * 0.4 : 0;
        // begin plot item
        if (ImPlot::BeginItem("Stk"))
        {
          // override legend icon color
          ImPlot::GetCurrentItem()->Color = ImGui::GetColorU32(greenc);
          // fit data if requested
          if (ImPlot::FitThisFrame())
          {
            for (int i = 0; i < data.size(); ++i)
            {
              ImPlot::FitPoint(ImPlotPoint(data[i].timestamp, data[i].low));
              ImPlot::FitPoint(ImPlotPoint(data[i].timestamp, data[i].high));
            }
          }
          // render data
          for (int i = 0; i < data.size(); ++i)
          {
            ImU32 color = ImGui::GetColorU32(data[i].open > data[i].close ? redc : greenc);
            ImVec2 open_pos = ImPlot::PlotToPixels(data[i].timestamp - half_width, data[i].open);
            ImVec2 close_pos = ImPlot::PlotToPixels(data[i].timestamp + half_width, data[i].close);
            draw_list->AddRectFilled(open_pos, close_pos, color);

            ImVec2 low_pos = ImPlot::PlotToPixels(data[i].timestamp, data[i].low);
            ImVec2 high_pos = ImPlot::PlotToPixels(data[i].timestamp, data[i].high);
            draw_list->AddLine(low_pos, high_pos, color,
                               ImMax(1.0f, ImAbs(open_pos.x - close_pos.x) / 10.0f));
          }

          ImPlot::EndItem();
        }
      }  // CANDLES end

      // y tag?
      ImPlotSpec spec;
      spec.LineColor = ImVec4(1, 1, 1, 1);
      ImPlot::PlotLine("Close", time.data(), close.data(), data.size(), spec);
      ImPlotRect bnds = ImPlot::GetPlotLimits();
      int close_idx = BinarySearch(
        time.data(), 0, time.size() - 1,
        ImPlot::RoundTime(ImPlotTime::FromDouble(bnds.X.Max), ImPlotTimeUnit_Day).ToDouble());
      if (close_idx == -1)
        close_idx = time.size() - 1;
      double close_val = close[close_idx];
      ImPlot::TagY(close_val, open[close_idx] < close[close_idx] ? greenc : redc);

      ImPlot::EndPlot();
    }  // OHLC plot


    if (ImPlot::BeginPlot("##VolumePlot"))
    {
      ImPlot::SetupAxes(
        0, 0, 0, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit | ImPlotAxisFlags_Opposite);
      ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Linear);
      ImPlot::SetupAxisLimits(ImAxis_X1, time[0], time.back());
      ImPlot::SetupAxisFormat(ImAxis_X1, TimeStampFormatter, nullptr);

      ImPlot::SetupAxisFormat(ImAxis_Y1, VolumeFormatter, nullptr);
      TickerTooltip(data, time, true);

      ImPlotRect limits = ImPlot::GetPlotLimits();
      double plotwidth = limits.X.Max - limits.X.Min;
      double widthperbar = plotwidth / (double)time.size();

      ImPlot::PlotBars("Volume", time.data(), volume.data(), time.size(), widthperbar);
      ImPlot::EndPlot();
    }

    ImPlot::EndSubplots();
  }
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
  static int concurrent_chart_count = 3;
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
      ImGui::DragInt("Max Concurrent Graphs", &concurrent_chart_count, 1, 1, max_concurrent_charts);
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
          // ImGui::Text("ConId: %d", chartheaders[n].mConnId);
          ImGui::Text("Exchange: %s", chartheaders[n].mExchange.c_str());

          int conid = chartheaders[n].mConnId;

          static std::unordered_map<int, std::vector<MarketDataPoint>> s_chartData;
          static std::unordered_map<int, bool> s_chartReady;

          std::vector<MarketDataPoint>& pts = s_chartData[conid];
          bool& ready = s_chartReady[conid];

          if (!ready)
          {
            int r = PollMarketDataHistory(conid, pts);
            if (r == 1)
              ready = true;
          }

          if (ready && !pts.empty())
          {
            PlotStockChart(pts);
          }

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