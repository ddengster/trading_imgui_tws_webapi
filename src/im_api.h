#pragma once

#include "trading_imgui.h"
#include <string>
#include <vector>
#include "coroutine/coroutine_mgt.h"

// --- Data types ---

struct MarketDataPoint
{
  double mOpen;
  double mHigh;
  double mLow;
  double mClose;
  double mVolume;
  double mTimestamp;
};

struct ExchContractId
{
  std::string mFullStockname;
  std::string mExchange;
  int mConid;
};

// --- Coroutine parameter structs ---

struct AccountIdResult
{
  std::string mAccountId;
  bool mSuccess = false;
};

struct MarketDataResult
{
  int mConid;
  std::vector<MarketDataPoint> mData;
  bool mSuccess = false;
};

struct StockChartData
{
  MarketDataResult mMarketDataResult;
  SnapshotResult mSnapshotResult;

  int mMarketDataCoroHandle = -1;
  int mSnapshotDataCoroHandle = -1;
  float mTimer = -1.0f;
  bool mSuccess = false;
};

struct ConIdResult
{
  std::string mSymbol;
  std::vector<ExchContractId> mContracts;
  bool mSuccess = false;
};

struct OrdersResult
{
  std::vector<OrderData> mOrders;
  bool mSuccess = false;
};

struct SuppressQuestionsData
{
  bool mSuccess = false;
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
void PostCloseOrder(mco_coro* co);
void PostModifyOrder(mco_coro* co);
void CancelOrder(mco_coro* co);
void PostSuppressQuestions(mco_coro* co);
