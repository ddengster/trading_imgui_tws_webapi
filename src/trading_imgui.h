
#pragma once

#include "imgui.h"
#include <string>
#include <unordered_map>
#include <vector>

struct PositionData
{
  std::string symbol;
  std::string secType;
  std::string assetClass;
  int conid;
  double size;
  double averageCost;
  double marketPrice;
  double marketValue;
  double realizedPNL;
  double unrealizedPNL;
};

struct SummaryData
{
  double cashUSD;
  double cashSGD;
  double netLiquidationValSGD;
  double buyingPowerSGD;
};

struct OrderData
{
  int orderId;
  std::string symbol;
  std::string side;
  std::string orderType;
  double totalSize;
  double filledQuantity;
  double remainingQuantity;
  double limitPrice;
  double stopPrice;
  std::string status;
  std::string lastExecutionTime;
};

struct PositionsResult
{
  std::string accountId;
  std::vector<PositionData> positions;
  bool success = false;
};

struct PostOrderData
{
  int conid;
  std::string orderType;
  float price = 1.f;
  float aux_price = 0.f;
  bool buy = true;
  float quantity;

  std::string order_id;
  std::string order_status;
  std::string encrypt_message;

  bool success = false;
  int coroHandle = -1;
};

struct CancelOrderData
{
  int orderId;
  bool success = false;
  int coroHandle = -1;
};

struct CloseOrderData
{
  int conid;
  double quantity;
  bool buy;
  std::string symbol;

  std::string order_id;
  std::string order_status;
  std::string encrypt_message;

  bool success = false;
  int coroHandle = -1;
};

struct LedgerResult
{
  std::string accountId;
  SummaryData summary;
  bool success = false;
};

struct SnapshotResult
{
  int conid;
  bool success = false;
};

struct SnapshotPriceData
{
  double last = 0.0;
  double bid = 0.0;
  double ask = 0.0;
  time_t timestamp = 0;
};

struct SummaryResult
{
  std::string accountId;
  SummaryData summary;
  bool success = false;
};

struct GlobalData
{
  int mConnectionState = 0;
  std::string mAccountId;

  int mTodaysRisk = 100;

  std::string mPlaceOrderTicker;
  int mPlaceOrderConid = -1;

  LedgerResult mLedgerResult;
  SummaryResult mSummaryResult;

  PositionsResult mPositions;
  std::unordered_map<int, SnapshotPriceData> mSnapshotBidAskLast;
  
  SnapshotResult mFreshOrderSnapshotResult;
  int mFreshOrderSnapshotCoroHandle = -1;
  bool mSummaryAndLedgerDoneOnce = false;
  std::vector<PostOrderData> mPendingPostOrders;
  std::vector<CloseOrderData> mPendingCloseOrders;
  std::unordered_map<int, CancelOrderData> mPendingCancels;
};
extern GlobalData gGlobalData;

bool ConnectingState();
void MainState();
