
#pragma once

#include "imgui.h"
#include <string>
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

struct GlobalData
{
  int mConnectionState = 0;
  std::string mAccountId;

  int mTodaysRisk = 100;

  PositionsResult mPositions;
  std::vector<PostOrderData> mPendingPostOrders;
};
extern GlobalData gGlobalData;

bool ConnectingState();
void MainState();
