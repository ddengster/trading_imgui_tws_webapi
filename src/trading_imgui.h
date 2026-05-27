
#pragma once

#include "imgui.h"
#include <string>
#include <vector>

struct PositionData
{
  std::string symbol;
  std::string secType;
  std::string assetClass;
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

bool ConnectingState();
void MainState();
