
#pragma once

#include "imgui.h"
#include <string>
#include <vector>

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

struct SummaryData
{
  double cashUSD;
  double cashSGD;
  double netLiquidationValSGD;
  double buyingPowerSGD;
};

bool ConnectingState();
void MainState();
