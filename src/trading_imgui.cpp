
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
#include <unordered_set>

#include "coroutine/coroutine_mgt.h"

std::string gAccountId;
std::string gFreshOrderTicker;
int gFreshOrderConid = -1;

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
  static int authenticated = 0;

  static int coro_handle = -1;
  if (coro_handle == -1)
    coro_handle = create_managed_coroutine(PollAuthStatus);
  else
  {
    mco_coro* co = get_coroutine(coro_handle);
    if (co == nullptr || mco_status(co) == MCO_DEAD)
    {
      mco_pop(co, &authenticated, sizeof(authenticated));
      mco_pop(co, &statuscode, sizeof(statuscode));

      destroy_coroutine(coro_handle);
      coro_handle = -1;
    }
  }

  if (statuscode != 200)
    ImGui::Text("statuscode: %d", statuscode);
  else
    ImGui::Text("statuscode: %d (auth: %d)", statuscode, authenticated ? 1 : 0);

  ImGui::End();
  return statuscode == 200 && authenticated;
}

void PortfolioUI()
{
  static PositionsResult posResult;
  static int posCoroHandle = -1;
  static float posRefreshTimer = -1.0f;
  static int ledgerCoroHandle = -1;
  static LedgerResult ledgerResult;
  static int summaryCoroHandle = -1;
  static SummaryResult summaryResult;

  static float ledgerRefreshTimer = -1.0f;
  static float summaryRefreshTimer = -1.0f;
  static bool summaryAndLedgerDoneOnce = false;

  if (posRefreshTimer < 0.f)
  {
    if (posCoroHandle == -1)
    {
      posResult.accountId = gAccountId;
      posCoroHandle = create_managed_coroutine(PollPositions, &posResult);
    }
    else
    {
      mco_coro* co = get_coroutine(posCoroHandle);
      if (!co || mco_status(co) == MCO_DEAD)
      {
        destroy_coroutine(posCoroHandle);
        posCoroHandle = -1;
        posRefreshTimer = 0.0f;
      }
    }
  }
  else
  {
    posRefreshTimer += ImGui::GetIO().DeltaTime;
    if (posRefreshTimer >= 5.0f)
    {
      posResult.accountId = gAccountId;
      posCoroHandle = create_managed_coroutine(PollPositions, &posResult);
      posRefreshTimer = 0.0f;
    }
  }

  if (ledgerRefreshTimer < 0.f)
  {
    if (ledgerCoroHandle == -1)
    {
      ledgerResult.accountId = gAccountId;
      ledgerCoroHandle = create_managed_coroutine(PollLedger, &ledgerResult);
    }
    else
    {
      mco_coro* co = get_coroutine(ledgerCoroHandle);
      if (!co || mco_status(co) == MCO_DEAD)
      {
        destroy_coroutine(ledgerCoroHandle);
        ledgerCoroHandle = -1;
        ledgerRefreshTimer = 0.0f;
      }
    }
  }
  else
  {
    ledgerRefreshTimer += ImGui::GetIO().DeltaTime;
    if (ledgerRefreshTimer >= 5.0f)
    {
      ledgerResult.accountId = gAccountId;
      ledgerCoroHandle = create_managed_coroutine(PollLedger, &ledgerResult);
      ledgerRefreshTimer = 0.0f;
    }
  }

  if (summaryRefreshTimer < 0.f)
  {
    if (summaryCoroHandle == -1)
    {
      summaryResult.accountId = gAccountId;
      summaryCoroHandle = create_managed_coroutine(PollSummary, &summaryResult);
    }
    else
    {
      mco_coro* co = get_coroutine(summaryCoroHandle);
      if (!co || mco_status(co) == MCO_DEAD)
      {
        destroy_coroutine(summaryCoroHandle);
        summaryCoroHandle = -1;
        summaryRefreshTimer = 0.0f;
      }
    }
  }
  else
  {
    summaryRefreshTimer += ImGui::GetIO().DeltaTime;
    if (summaryRefreshTimer >= 5.0f)
    {
      summaryResult.accountId = gAccountId;
      summaryCoroHandle = create_managed_coroutine(PollSummary, &summaryResult);
      summaryRefreshTimer = 0.0f;
    }
  }

  bool ledgerReady = (ledgerCoroHandle == -1 && ledgerResult.success);
  bool summaryReady = summaryCoroHandle == -1 && summaryResult.success;

  if (!summaryAndLedgerDoneOnce)
    summaryAndLedgerDoneOnce = ledgerReady && summaryReady;

  ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImVec2(0, 400), ImGuiCond_FirstUseEver);
  ImGui::Begin(gAccountId.c_str());

  if (summaryAndLedgerDoneOnce)
  {
    ImGui::Text("Cash Balance Total SGD: %g  USD: %g  NetLiq(SGD): %g",
                ledgerResult.summary.cashSGD, ledgerResult.summary.cashUSD,
                ledgerResult.summary.netLiquidationValSGD);
    ImGui::Text("Buying Power SGD: %g", summaryResult.summary.buyingPowerSGD);
  }
  else
  {
    ImGui::Text("Cash Balance Total SGD: ...   USD: ...   SGD: ...");
    ImGui::Text("Buying Power SGD: ...");
  }
  ImGui::Text("Positions: %d", (int)posResult.positions.size());

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

    for (int i = 0; i < (int)posResult.positions.size(); ++i)
    {
      auto& p = posResult.positions[i];

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

  if (posResult.positions.empty() && posCoroHandle != -1)
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
  //@todo: mouse coordinate to data index mapping is wrong
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

static int IBKRTimeStrToLocalTime(const char* ts, char* buf, int buf_sz)
{
  struct tm tm_utc = {0};

  tm_utc.tm_year = 2000 + (ts[0] - '0') * 10 + (ts[1] - '0') - 1900;
  tm_utc.tm_mon = ((ts[2] - '0') * 10 + (ts[3] - '0')) - 1;
  tm_utc.tm_mday = (ts[4] - '0') * 10 + (ts[5] - '0');
  tm_utc.tm_hour = (ts[6] - '0') * 10 + (ts[7] - '0');
  tm_utc.tm_min = (ts[8] - '0') * 10 + (ts[9] - '0');
  tm_utc.tm_sec = (ts[10] - '0') * 10 + (ts[11] - '0');

  tm_utc.tm_isdst = -1;  // let system determine DST

  time_t t;

#ifdef _WIN32
  t = _mkgmtime(&tm_utc);  // Windows
#else
  t = timegm(&tm_utc);  // POSIX (Linux/macOS)
#endif
  struct tm* tm_local = localtime(&t);
  strftime(buf, buf_sz, "%Y-%m-%d %H:%M:%S", tm_local);
  return 0;
}

void OrderWindowUI()
{
  static OrdersResult ordersResult;
  static int ordersCoroHandle = -1;
  static float ordersRefreshTimer = -1.0f;

  if (ordersRefreshTimer < 0.f)
  {
    if (ordersCoroHandle == -1)
    {
      ordersResult = OrdersResult();
      ordersCoroHandle = create_managed_coroutine(PollOrders, &ordersResult);
    }
    else
    {
      mco_coro* co = get_coroutine(ordersCoroHandle);
      if (!co || mco_status(co) == MCO_DEAD)
      {
        destroy_coroutine(ordersCoroHandle);
        ordersCoroHandle = -1;
        ordersRefreshTimer = 0.0f;
      }
    }
  }
  else
  {
    ordersRefreshTimer += ImGui::GetIO().DeltaTime;
    if (ordersRefreshTimer >= 5.0f)
    {
      ordersCoroHandle = create_managed_coroutine(PollOrders, &ordersResult);
      ordersRefreshTimer = 0.0f;
    }
  }

  ImGui::SetNextWindowSize(ImVec2(600, 300), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImVec2(800, 400), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Orders"))
  {
    static bool show_open_orders_only = true;
    ImGui::Checkbox("Open Orders Only", &show_open_orders_only);

    if (ImGui::BeginTable("orders", 9, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
    {
      ImGui::TableSetupColumn("LastExecTime");
      ImGui::TableSetupColumn("Symbol");
      ImGui::TableSetupColumn("Side");
      ImGui::TableSetupColumn("Type");
      ImGui::TableSetupColumn("Filled");
      ImGui::TableSetupColumn("Limit Price");
      ImGui::TableSetupColumn("Stop Price");
      ImGui::TableSetupColumn("Status");
      ImGui::TableSetupColumn("Actions");

      ImGui::TableHeadersRow();

      for (int i = 0; i < (int)ordersResult.orders.size(); ++i)
      {
        auto& o = ordersResult.orders[i];

        if (show_open_orders_only && o.status == "Filled")
          continue;

        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        {
          char buf[32] = {};
          IBKRTimeStrToLocalTime(o.lastExecutionTime.c_str(), buf, sizeof(buf));
          ImGui::TextWrapped("%s", buf);
        }

        ImGui::TableNextColumn();
        ImGui::Text(o.symbol.c_str());
        ImGui::TableNextColumn();
        ImGui::Text(o.side.c_str());
        ImGui::TableNextColumn();
        ImGui::Text(o.orderType.c_str());
        ImGui::TableNextColumn();

        char t[64] = {0};
        snprintf(t, sizeof(t), "%g/%g", o.filledQuantity, o.totalSize);
        ImGui::Text(t);
        ImGui::TableNextColumn();

        snprintf(t, sizeof(t), "%.2f", o.limitPrice);
        ImGui::Text(t);
        ImGui::TableNextColumn();

        snprintf(t, sizeof(t), "%.2f", o.stopPrice);
        ImGui::Text(t);
        
        ImGui::TableNextColumn();
        ImGui::Text(o.status.c_str());

        ImGui::TableNextColumn();
        if (o.status != "Filled" && ImGui::Button("Cancel"))
        {
          /* CancelOrderData data;
          data.orderId = o.orderId;
          create_managed_coroutine(CancelOrder, &data);
          */
        }
      }

      ImGui::EndTable();
    }

    if (ordersResult.orders.empty() && ordersCoroHandle != -1)
    {
      ImGui::TextColored(ImVec4(1, 1, 0, 1), "Loading orders...");
    }
    else if (ordersResult.orders.empty() && ordersCoroHandle == -1)
    {
      ImGui::TextColored(ImVec4(1, 1, 0, 1), "No orders");
    }

  }
  ImGui::End();
}

void FreshOrderWindowUI()
{
  if (gFreshOrderConid == -1)
    return;

  char n[32] = {};
  snprintf(n, sizeof(n), "Fresh Order - %s(%d)", gFreshOrderTicker.c_str(), gFreshOrderConid);

  static PostOrderData postOrderData;
  static int quantity = 1;
  static int postOrderCoroHandle = -1;

  ImGui::SetNextWindowSize(ImVec2(600, 300), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImVec2(800, 400), ImGuiCond_FirstUseEver);
  if (ImGui::Begin(n))
  {
    ImGui::NewLine();

    ImGui::InputFloat("Price:", &postOrderData.price);
    ImGui::InputInt("Quantity:", &quantity);

    const char* possible_actions[] = {"BUY", "SELL"};

    static const char* current_action = possible_actions[0];
    if (ImGui::BeginCombo("Action", current_action))
    {
      for (int n = 0; n < IM_ARRAYSIZE(possible_actions); n++)
      {
        bool is_selected = (current_action == possible_actions[n]);
        if (ImGui::Selectable(possible_actions[n], is_selected))
          current_action = possible_actions[n];
        if (is_selected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    static const char* possible_order_types[] = {"LMT"};
    static const char* current_order_type = possible_order_types[0];
    if (ImGui::BeginCombo("Order Type", current_order_type))
    {
      for (int n = 0; n < IM_ARRAYSIZE(possible_order_types); n++)
      {
        bool is_selected = (current_order_type == possible_order_types[n]);
        if (ImGui::Selectable(possible_order_types[n], is_selected))
          current_order_type = possible_order_types[n];
        if (is_selected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    ImGui::NewLine();

    if (ImGui::Button("Submit", ImVec2(-1.f, 20.f)))
    {
      postOrderData.conid = gFreshOrderConid;
      postOrderData.orderType = current_order_type;
      postOrderData.buy = current_action == "BUY";
      postOrderData.quantity = (float)quantity;
      postOrderCoroHandle = create_managed_coroutine(PostOrders, &postOrderData);
    }

    if (postOrderData.success)
    {
      ImGui::TextColored(ImVec4(0, 1, 0, 1), "Order submitted successfully! Order ID: %s",
                         postOrderData.order_id.c_str());
      ImGui::TextColored(ImVec4(0, 1, 0, 1), postOrderData.order_status.c_str());
    }
    else if (!postOrderData.success && !postOrderData.order_status.empty())
    {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Order submission failed! Error: %s",
                         postOrderData.order_status.c_str());
    }

  }
  ImGui::End();

  if (postOrderCoroHandle != -1)
  {
    mco_coro* co = get_coroutine(postOrderCoroHandle);
    if (!co || mco_status(co) == MCO_DEAD)
    {
      destroy_coroutine(postOrderCoroHandle);
      postOrderCoroHandle = -1;
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
  static int accountCoroHandle = -1;
  static AccountIdResult accountIdResult;
  if (!accountsDone)
  {
    if (accountCoroHandle == -1)
    {
      accountIdResult = AccountIdResult();
      accountCoroHandle = create_managed_coroutine(PollAccountId, &accountIdResult);
    }
    else
    {
      mco_coro* co = get_coroutine(accountCoroHandle);
      if (!co || mco_status(co) == MCO_DEAD)
      {
        if (accountIdResult.success)
        {
          gAccountId = accountIdResult.accountId;
          accountsDone = true;
        }
        destroy_coroutine(accountCoroHandle);
        accountCoroHandle = -1;
      }
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

    static ConIdResult conIdResult;
    static int conidCoroHandle = -1;
    static std::string ticker_result = "";
    static std::string full_stockname;
    static char symbol[64] = {0};
    bool changed =
      ImGui::InputText("Symbol", symbol, sizeof(symbol),
                       ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_EnterReturnsTrue |
                         ImGuiInputTextFlags_EscapeClearsAll);
    if (changed)
    {
      ticker_result = symbol;
      conIdResult = ConIdResult();
      conIdResult.symbol = symbol;
      if (conidCoroHandle != -1)
      {
        destroy_coroutine(conidCoroHandle);
        conidCoroHandle = -1;
      }
      conidCoroHandle = create_managed_coroutine(PollConId, &conIdResult);
    }
    else if (conidCoroHandle != -1)
    {
      mco_coro* co = get_coroutine(conidCoroHandle);
      if (!co || mco_status(co) == MCO_DEAD)
      {
        destroy_coroutine(conidCoroHandle);
        conidCoroHandle = -1;
      }
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

      for (int i = 0; i < (int)conIdResult.contracts.size(); ++i)
      {
        auto& c = conIdResult.contracts[i];

        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::TextWrapped(c.full_stockname.c_str());
        ImGui::TableNextColumn();
        ImGui::TextWrapped(c.exchange.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%d", c.conid);
        ImGui::TableNextColumn();

        ImGui::PushID(i);
        if (ImGui::Button("Add", ImVec2(-1.f, 20.f)))
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

        if (ImGui::Button("FreshOrder", ImVec2(-1.f, 20.f)))
        {
          gFreshOrderConid = c.conid;
          gFreshOrderTicker = ticker_result;
        }
        ImGui::PopID();
      }
      ImGui::EndTable();
    }

    ImGui::End();
  }

  // stock charts
  static std::unordered_map<int, StockChartData> s_chartData;
  static std::unordered_set<int> s_active_conid_thisframe;
  static std::unordered_set<int> s_to_remove_conid_thisframe;

  s_active_conid_thisframe.clear();
  s_to_remove_conid_thisframe.clear();

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
        int conid = chartheaders[n].mConnId;

        bool open = true;
        if (ImGui::BeginTabItem(chartheaders[n].mTicker.c_str(), &open))
        {
          ImGui::Text("Exchange: %s", chartheaders[n].mExchange.c_str());

          if (s_chartData.find(conid) == s_chartData.end())
          {
            ImGui::Text("Bid: --  |  Ask: --");
            ImGui::Text("Last: --");

            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Loading..");
          }
          else
          {
            StockChartData& chartData = s_chartData[conid];

            if (chartData.mSnapshotResult.success)
            {
              ImGui::Text("Bid: %.2f  |  Ask: %.2f", chartData.mSnapshotResult.bid,
                          chartData.mSnapshotResult.ask);
              ImGui::Text("Last: %.2f", chartData.mSnapshotResult.last);
            }
            else
            {
              ImGui::Text("Bid: --  |  Ask: --");
              ImGui::Text("Last: %.2f", chartData.mSnapshotResult.last);
            }

            PlotStockChart(chartData.mMarketDataResult.data);
          }

          ImGui::EndTabItem();
        }

        if (open)
          s_active_conid_thisframe.insert(conid);
        else
          s_to_remove_conid_thisframe.insert(conid);
      }
      ImGui::EndTabBar();
    }

    ImGui::End();
  }

  // update conid's chartData, initiating coroutines as needed
  {
    for (int conid : s_active_conid_thisframe)
    {
      if (s_chartData.find(conid) == s_chartData.end())
      {
        StockChartData dat;
        dat.mMarketDataResult.conid = conid;
        dat.mSnapshotResult.conid = conid;
        s_chartData.insert({conid, dat});
      }
    }
    for (int conid : s_to_remove_conid_thisframe)
    {
      if (s_chartData.find(conid) != s_chartData.end())
      {
        StockChartData& cd = s_chartData[conid];
        if (cd.mMarketDataCoroHandle != -1)
        {
          mco_coro* co = get_coroutine(cd.mMarketDataCoroHandle);
          if (co)
          {
            destroy_coroutine(cd.mMarketDataCoroHandle);
            cd.mMarketDataCoroHandle = -1;
          }
        }
        if (cd.mSnapshotDataCoroHandle != -1)
        {
          mco_coro* co = get_coroutine(cd.mSnapshotDataCoroHandle);
          if (co)
          {
            destroy_coroutine(cd.mSnapshotDataCoroHandle);
            cd.mSnapshotDataCoroHandle = -1;
          }
        }
        s_chartData.erase(conid);

        for (int i = 0; i < chartheaders.size(); ++i)
        {
          if (chartheaders[i].mConnId == conid)
          {
            chartheaders.erase(chartheaders.begin() + i);
            break;
          }
        }
      }
    }

    for (auto& itr : s_chartData)
    {
      StockChartData& cd = itr.second;
      static const float s_chartDataInterval = 5.0f;

      if (cd.mTimer < 0.0f || cd.mTimer >= s_chartDataInterval)
      {
        cd.mTimer = 0.0f;

        // destroy whatever in progress coroutines if any and reinitiate new calls
        if (cd.mMarketDataCoroHandle != -1)
        {
          mco_coro* co = get_coroutine(cd.mMarketDataCoroHandle);
          if (co)
          {
            destroy_coroutine(cd.mMarketDataCoroHandle);
            cd.mMarketDataCoroHandle = -1;
          }
        }
        if (cd.mSnapshotDataCoroHandle != -1)
        {
          mco_coro* co = get_coroutine(cd.mSnapshotDataCoroHandle);
          if (co)
          {
            destroy_coroutine(cd.mSnapshotDataCoroHandle);
            cd.mSnapshotDataCoroHandle = -1;
          }
        }

        cd.mMarketDataCoroHandle =
          create_managed_coroutine(PollMarketDataHistory, &cd.mMarketDataResult);

        cd.mSnapshotDataCoroHandle =
          create_managed_coroutine(PollMarketDataSnapshot, &cd.mSnapshotResult);
      }
      else
      {
        cd.mTimer += ImGui::GetIO().DeltaTime;

        if (cd.mMarketDataCoroHandle != -1)
        {
          mco_coro* co = get_coroutine(cd.mMarketDataCoroHandle);
          if (!co || mco_status(co) == MCO_DEAD)
          {
            destroy_coroutine(cd.mMarketDataCoroHandle);
            cd.mMarketDataCoroHandle = -1;
          }
        }
        if (cd.mSnapshotDataCoroHandle != -1)
        {
          mco_coro* co = get_coroutine(cd.mSnapshotDataCoroHandle);
          if (!co || mco_status(co) == MCO_DEAD)
          {
            printf("connid: %d, fin time: %g\n", itr.first, cd.mTimer);
            destroy_coroutine(cd.mSnapshotDataCoroHandle);
            cd.mSnapshotDataCoroHandle = -1;
          }
        }
      }
    }
  }

  // order window
  OrderWindowUI();

  // fresh order window
  FreshOrderWindowUI();
}