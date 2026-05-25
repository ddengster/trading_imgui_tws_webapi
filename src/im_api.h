#pragma once

#include "trading_imgui.h"
#include <string>
#include <vector>

int PollAuthStatus(int& out_statuscode, bool& auth);
int PollAccountId(std::string& out_accountId);
int PollPositions(const std::string& accountId, std::vector<PositionData>& out_positions,
                  bool force_reset = false);
int PollLedger(const std::string& accountId, SummaryData& out_summary);
int PollSummary(const std::string& accountId, SummaryData& out_summary);

struct ExchContractId
{
  std::string full_stockname;
  std::string exchange;
  int conid;
};
int PollConId(const std::string& symbol, std::vector<ExchContractId>& out,
              bool force_reset = false);
