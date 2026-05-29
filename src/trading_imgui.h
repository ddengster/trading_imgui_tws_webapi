
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

struct GlobalData
{
  int mConnectionState = 0;
  std::string mAccountId;

  int mTodaysRisk = 100;

  PositionsResult mPositions;
};
extern GlobalData gGlobalData;

bool ConnectingState();
void MainState();
