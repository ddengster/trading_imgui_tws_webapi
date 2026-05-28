#pragma once

#include "trading_imgui.h"
#include <string>
#include <vector>
#include "coroutine/coroutine_mgt.h"

// --- Data types ---

struct MarketDataPoint
{
  double open;
  double high;
  double low;
  double close;
  double volume;
  double timestamp;
};

struct ExchContractId
{
  std::string full_stockname;
  std::string exchange;
  int conid;
};

// --- Coroutine parameter structs ---

struct AccountIdResult
{
  std::string accountId;
  bool success = false;
};

struct PositionsResult
{
  std::string accountId;
  std::vector<PositionData> positions;
  bool success = false;
};

struct LedgerResult
{
  std::string accountId;
  SummaryData summary;
  bool success = false;
};

struct SummaryResult
{
  std::string accountId;
  SummaryData summary;
  bool success = false;
};

struct MarketDataResult
{
  int conid;
  std::vector<MarketDataPoint> data;
  bool success = false;
};

struct SnapshotResult
{
  // args
  int conid;


  // result
  double last = 0.0;
  double bid = 0.0;
  double ask = 0.0;
  bool success = false;
};

struct StockChartData
{
  MarketDataResult mMarketDataResult;
  SnapshotResult mSnapshotResult;

  int mMarketDataCoroHandle = -1;
  int mSnapshotDataCoroHandle = -1;
  float mTimer = -1.0f;
};

struct ConIdResult
{
  std::string symbol;
  std::vector<ExchContractId> contracts;
  bool success = false;
};

struct OrdersResult
{
  std::vector<OrderData> orders;
  bool success = false;
};

struct PostOrderData
{
  int conid;
  std::string orderType;
  float price = 1.f; // LMT or STOP_LIMIT
  float aux_price = 0.f; // for STOP_LIMIT and traillmt orders
  bool buy = true;
  // float trailing_amount; //  TRAIL and TRAILLMT order
  // std::string trailingType;
  float quantity;

  // response fields
  std::string order_id;
  std::string order_status;
  std::string encrypt_message;

  bool success = false;
};

struct CancelOrderData
{
  int orderId;
  bool success = false;
  int coroHandle = -1;
};

// --- Coroutine functions ---

void PollAuthStatus(mco_coro* co);
void PollAccountId(mco_coro* co);
void PollPositions(mco_coro* co);
void PollLedger(mco_coro* co);
void PollSummary(mco_coro* co);
void PollMarketDataHistory(mco_coro* co);
void PollMarketDataSnapshot(mco_coro* co);
void PollConId(mco_coro* co);
void PollOrders(mco_coro* co);
void PostOrders(mco_coro* co);
void CancelOrder(mco_coro* co);
