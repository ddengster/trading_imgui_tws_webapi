
#pragma once

#include "imgui.h"
#include <string>
#include <unordered_map>
#include <vector>

struct PositionData
{
  std::string mSymbol;
  std::string mSecType;
  std::string mAssetClass;
  int mConid;
  double mSize;
  double mAverageCost;
  double mMarketPrice;
  double mMarketValue;
  double mRealizedPNL;
  double mUnrealizedPNL;
};

struct SummaryData
{
  double mCashUSD;
  double mCashSGD;
  double mNetLiquidationValSGD;
  double mBuyingPowerSGD;
};

struct OrderData
{
  int mOrderId;
  int mConid;
  std::string mSymbol;
  std::string mSide;
  std::string mOrderType;
  double mTotalSize;
  double mFilledQuantity;
  double mRemainingQuantity;
  double mLimitPrice;
  double mStopPrice;
  std::string mStatus;
  std::string mLastExecutionTime;
};

struct PositionsResult
{
  std::string mAccountId;
  std::vector<PositionData> mPositions;
  bool mSuccess = false;
};

struct PostOrderEntry
{
  std::string mOrderType;
  float mPrice = 1.f;
  float mAuxPrice = 0.f;
  bool mBuy = true;
  float mQuantity;

  std::string mCOID;  // identifier used for SL orders linked to a main order
  std::string mParentId;  // for TP/SL orders
};

struct PostOrderData
{
  int mConid;
  std::vector<PostOrderEntry> mOrders;

  std::vector<std::string> mAssignedOrderIds;
  std::vector<std::string> mOrderStatuses;
  std::string mEncryptMessage;

  bool mSuccess = false;
  int mCoroHandle = -1;
};

struct CancelOrderData
{
  int mOrderId;
  bool mSuccess = false;
  int mCoroHandle = -1;
};

struct ModifyOrderData
{
  int mOrderId;
  int mConid;
  std::string mSymbol;
  std::string mSide;
  std::string mOrderType;
  double mNewPrice;
  double mNewQuantity;

  std::string mAssignedOrderId;
  std::string mOrderStatus;
  std::string mEncryptMessage;

  bool mSuccess = false;
  int mCoroHandle = -1;
};

struct CloseOrderData
{
  int mConid;
  double mQuantity;
  bool mBuy;
  std::string mSymbol;

  std::string mAssignedOrderId;
  std::string mOrderStatus;
  std::string mEncryptMessage;

  bool mSuccess = false;
  int mCoroHandle = -1;
};

struct LedgerResult
{
  std::string mAccountId;
  SummaryData mSummary;
  bool mSuccess = false;
};

struct SnapshotResult
{
  int mConid;
  bool mSuccess = false;
};

struct SnapshotPriceData
{
  double mLast = 0.0;
  double mBid = 0.0;
  double mAsk = 0.0;
  time_t mTimestamp = 0;
};

struct SummaryResult
{
  std::string mAccountId;
  SummaryData mSummary;
  bool mSuccess = false;
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
  std::vector<ModifyOrderData> mPendingModifies;
};
extern GlobalData gGlobalData;

bool ConnectingState();
void MainState();
