
#include "im_api.h"
#include "trading_imgui.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "coroutine/coroutine_mgt.h"
#include "naett/naett.h"
#include "parson/parson.h"
#include <ctime>
#include <unordered_map>

#define BASE_URL "https://localhost:5000/v1/api"


struct ReqRes
{
  naettReq* mReq = nullptr;
  naettRes* mRes = nullptr;

  bool valid() const { return mReq && mRes; }
  void cleanup()
  {
    if (mReq)
      naettFree(mReq);
    if (mRes)
      naettClose(mRes);
    mReq = nullptr;
    mRes = nullptr;
  }
};

static ReqRes make_get(const char* url)
{
  ReqRes rr;
  naettOption* options[] = {naettMethod("GET")};
  rr.mReq = naettRequestWithOptions(url, 1, (const naettOption**)&options);
  rr.mRes = naettMake(rr.mReq);
  return rr;
}

void PollAuthStatus(mco_coro* co)
{
  ReqRes rr = make_get(BASE_URL "/iserver/auth/status");

  if (!rr.valid())
  {
    int ret = -1;
    mco_push(co, &ret, sizeof(ret));
    ret = 0;
    mco_push(co, &ret, sizeof(ret));
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.mRes);

  int out_statuscode = naettGetStatus(rr.mRes);

  int authenticated = 0;
  if (out_statuscode == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.mRes, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Object* obj = json_value_get_object(val);
    authenticated = json_object_get_boolean(obj, "authenticated");
    json_value_free(val);
  }

  mco_push(co, &out_statuscode, sizeof(out_statuscode));
  mco_push(co, &authenticated, sizeof(authenticated));

  rr.cleanup();
}

void PollAccountId(mco_coro* co)
{
  auto* result = static_cast<AccountIdResult*>(mco_get_user_data(co));

  ReqRes rr = make_get(BASE_URL "/portfolio/accounts");

  if (!rr.valid())
  {
    result->mSuccess = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.mRes);

  int status = naettGetStatus(rr.mRes);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.mRes, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Array* arr = json_value_get_array(val);
    int n = json_array_get_count(arr);
    for (int i = 0; i < n; ++i)
    {
      JSON_Object* obj = json_array_get_object(arr, i);
      const char* id = json_object_get_string(obj, "accountId");
      if (id)
      {
        result->mAccountId = id;
        break;
      }
    }
    json_value_free(val);
    result->mSuccess = !result->mAccountId.empty();
  }
  else
  {
    result->mSuccess = false;
  }

  rr.cleanup();
}

void PollPositions(mco_coro* co)
{
  // printf("query positions\n");
  auto* result = static_cast<PositionsResult*>(mco_get_user_data(co));

  char url[256];
  snprintf(url, sizeof(url), BASE_URL "/portfolio/%s/positions/0", result->mAccountId.c_str());

  ReqRes rr = make_get(url);

  if (!rr.valid())
  {
    result->mSuccess = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.mRes);

  int status = naettGetStatus(rr.mRes);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.mRes, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Array* arr = json_value_get_array(val);
    int n = json_array_get_count(arr);
    result->mPositions.clear();
    result->mPositions.reserve(n);
    for (int i = 0; i < n; ++i)
    {
      JSON_Object* obj = json_array_get_object(arr, i);
      const char* contractDesc = json_object_get_string(obj, "contractDesc");
      int conid = json_object_get_number(obj, "conid");
      double position = json_object_get_number(obj, "position");
      double avg_cost = json_object_get_number(obj, "avgCost");
      double mkt_price = json_object_get_number(obj, "mktPrice");
      double mkt_value = json_object_get_number(obj, "mktValue");
      double realized_pnl = json_object_get_number(obj, "realizedPnl");
      double unrealized_pnl = json_object_get_number(obj, "unrealizedPnl");
      const char* assetclass = json_object_get_string(obj, "assetClass");

      std::string desc = contractDesc ? contractDesc : "";
      std::string symbol;
      size_t first_space = desc.find(' ');
      if (first_space != std::string::npos)
        symbol = desc.substr(0, first_space);
      else
        symbol = desc;

      PositionData pd;
      pd.mSymbol = symbol;
      pd.mSecType = contractDesc;
      pd.mAssetClass = assetclass;
      pd.mConid = conid;
      pd.mSize = position;
      pd.mAverageCost = avg_cost;
      pd.mMarketPrice = mkt_price;
      pd.mMarketValue = mkt_value;
      pd.mRealizedPNL = realized_pnl;
      pd.mUnrealizedPNL = unrealized_pnl;
      result->mPositions.push_back(pd);
    }
    json_value_free(val);
    result->mSuccess = true;
  }
  else
  {
    result->mSuccess = false;
  }

  rr.cleanup();
}

void PollLedger(mco_coro* co)
{
  // printf("query ledger\n");
  auto* result = static_cast<LedgerResult*>(mco_get_user_data(co));

  char url[256];
  snprintf(url, sizeof(url), BASE_URL "/portfolio/%s/ledger", result->mAccountId.c_str());

  ReqRes rr = make_get(url);

  if (!rr.valid())
  {
    result->mSuccess = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.mRes);

  int status = naettGetStatus(rr.mRes);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.mRes, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Object* obj = json_value_get_object(val);

    JSON_Object* cashObj = json_object_get_object(obj, "USD");
    if (cashObj)
      result->mSummary.mCashUSD = json_object_get_number(cashObj, "cashbalance");

    cashObj = json_object_get_object(obj, "SGD");
    if (cashObj)
      result->mSummary.mCashSGD = json_object_get_number(cashObj, "cashbalance");

    cashObj = json_object_get_object(obj, "BASE");
    if (cashObj)
      result->mSummary.mNetLiquidationValSGD = json_object_get_number(cashObj, "netliquidationvalue");

    json_value_free(val);
    result->mSuccess = true;
  }
  else
  {
    result->mSuccess = false;
  }

  rr.cleanup();
}

void PollSummary(mco_coro* co)
{
  // printf("query summary\n");
  auto* result = static_cast<SummaryResult*>(mco_get_user_data(co));

  char url[256];
  snprintf(url, sizeof(url), BASE_URL "/portfolio/%s/summary", result->mAccountId.c_str());

  ReqRes rr = make_get(url);

  if (!rr.valid())
  {
    result->mSuccess = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.mRes);

  int status = naettGetStatus(rr.mRes);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.mRes, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Object* obj = json_value_get_object(val);

    JSON_Object* cashObj = json_object_get_object(obj, "buyingpower");
    if (cashObj)
      result->mSummary.mBuyingPowerSGD = json_object_get_number(cashObj, "amount");

    json_value_free(val);
    result->mSuccess = true;
  }
  else
  {
    result->mSuccess = false;
  }

  rr.cleanup();
}

void PollMarketDataHistory(mco_coro* co)
{
  auto* result = static_cast<MarketDataResult*>(mco_get_user_data(co));

  char url[512];
  snprintf(url, sizeof(url),
           BASE_URL "/iserver/marketdata/"
                    "history?conid=%d&period=2h&bar=5min&outsideRth=true",
           result->mConid);

  ReqRes rr = make_get(url);

  if (!rr.valid())
  {
    result->mSuccess = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.mRes);

  int status = naettGetStatus(rr.mRes);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.mRes, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Object* obj = json_value_get_object(val);
    JSON_Array* arr = json_object_get_array(obj, "data");
    int count = (int)json_array_get_count(arr);
    result->mData.clear();
    result->mData.reserve(count);
    for (int i = 0; i < count; ++i)
    {
      JSON_Object* bar = json_array_get_object(arr, i);
      MarketDataPoint pt;
      pt.mOpen = json_object_get_number(bar, "o");
      pt.mHigh = json_object_get_number(bar, "h");
      pt.mLow = json_object_get_number(bar, "l");
      pt.mClose = json_object_get_number(bar, "c");
      pt.mVolume = json_object_get_number(bar, "v");
      pt.mTimestamp = json_object_get_number(bar, "t");
      result->mData.push_back(pt);
    }
    json_value_free(val);
    result->mSuccess = true;
  }
  else
  {
    result->mSuccess = false;
  }

  rr.cleanup();
}

void PollMarketDataSnapshot(mco_coro* co)
{
  //@note: Reference for field arguments:
  // https://www.interactivebrokers.eu/campus/ibkr-api-page/cpapi-v1/?utm_source=chatgpt.com#market-data-fields
  auto* result = static_cast<SnapshotResult*>(mco_get_user_data(co));


  time_t now = time(NULL);
  now -= 3600;  // go back 1 hour to ensure we get valid data even if the market just opened
  struct tm* tmt = localtime(&now);
  time_t epoch = mktime(tmt);

  char url[512];
  snprintf(url, sizeof(url),
           BASE_URL "/iserver/marketdata/snapshot?conids=%d&since=%lld&fields=31,84,86",
           result->mConid, epoch);

  ReqRes rr = make_get(url);

  if (!rr.valid())
  {
    result->mSuccess = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.mRes);

  int status = naettGetStatus(rr.mRes);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.mRes, &sz);
    JSON_Value* val = json_parse_string((const char*)body);

    JSON_Array* arr = json_value_get_array(val);
    if (arr && json_array_get_count(arr) > 0)
    {
      JSON_Object* obj = json_array_get_object(arr, 0);
      SnapshotPriceData& price = gGlobalData.mSnapshotBidAskLast[result->mConid];
      const char* last_str = json_object_get_string(obj, "31");
      price.mLast = last_str ? atof(last_str) : 0.0;
      const char* bid_str = json_object_get_string(obj, "84");
      price.mBid = bid_str ? atof(bid_str) : 0.0;
      const char* ask_str = json_object_get_string(obj, "86");
      price.mAsk = ask_str ? atof(ask_str) : 0.0;
      price.mTimestamp = time(NULL);
      result->mSuccess = true;
    }

    json_value_free(val);
  }
  else
  {
    result->mSuccess = false;
  }

  rr.cleanup();
}

void PollConId(mco_coro* co)
{
  auto* result = static_cast<ConIdResult*>(mco_get_user_data(co));

  char url[512];
  snprintf(url, sizeof(url), BASE_URL "/trsrv/stocks?symbols=%s", result->mSymbol.c_str());

  ReqRes rr = make_get(url);

  if (!rr.valid())
  {
    result->mSuccess = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.mRes);

  int status = naettGetStatus(rr.mRes);
  if (status == 200)
  {
    const char* skipped_exch[] = {"MEXI"};
    int skipped_exch_count = sizeof(skipped_exch) / sizeof(skipped_exch[0]);

    int sz = 0;
    const void* body = naettGetBody(rr.mRes, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Object* obj = json_value_get_object(val);
    JSON_Array* arr = json_object_get_array(obj, result->mSymbol.c_str());
    int count = (int)json_array_get_count(arr);
    result->mContracts.clear();
    printf("reply: %s\n", (const char*)body);

    if (arr && count > 0)
    {
      for (int i = 0; i < count; ++i)
      {
        JSON_Object* entry = json_array_get_object(arr, i);
        std::string full_stockname = json_object_get_string(entry, "name");

        JSON_Array* contracts_arr = json_object_get_array(entry, "contracts");
        int contracts_count = (int)json_array_get_count(contracts_arr);
        for (int j = 0; j < contracts_count; ++j)
        {
          JSON_Object* contract = json_array_get_object(contracts_arr, j);
          const char* exch = json_object_get_string(contract, "exchange");

          if (!exch)
            continue;
          bool skipped = false;
          for (int k = 0; k < skipped_exch_count; ++k)
          {
            if (strcmp(exch, skipped_exch[k]) == 0)
            {
              skipped = true;
              break;
            }
          }

          if (skipped)
            continue;

          int out_conid = (int)json_object_get_number(contract, "conid");
          result->mContracts.push_back({full_stockname, exch, out_conid});
        }
      }
    }
    json_value_free(val);
    result->mSuccess = true;
  }
  else
  {
    result->mSuccess = false;
  }

  rr.cleanup();
}

void PollOrders(mco_coro* co)
{
  auto* result = static_cast<OrdersResult*>(mco_get_user_data(co));

  ReqRes rr = make_get(BASE_URL "/iserver/account/orders");

  if (!rr.valid())
  {
    result->mSuccess = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.mRes);

  int status = naettGetStatus(rr.mRes);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.mRes, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Object* root = json_value_get_object(val);
    JSON_Array* arr = json_object_get_array(root, "orders");
    int n = json_array_get_count(arr);
    result->mOrders.clear();
    result->mOrders.reserve(n);
    for (int i = 0; i < n; ++i)
    {
      JSON_Object* obj = json_array_get_object(arr, i);
      OrderData od;
      od.mOrderId = (int)json_object_get_number(obj, "orderId");
      od.mConid = (int)json_object_get_number(obj, "conid");
      const char* symbol = json_object_get_string(obj, "ticker");
      od.mSymbol = symbol ? symbol : "";
      const char* side = json_object_get_string(obj, "side");
      od.mSide = side ? side : "";
      const char* order_type = json_object_get_string(obj, "orderType");
      od.mOrderType = order_type ? order_type : "";
      od.mTotalSize = json_object_get_number(obj, "totalSize");
      od.mFilledQuantity = json_object_get_number(obj, "filledQuantity");
      od.mRemainingQuantity = json_object_get_number(obj, "remainingQuantity");
      const char* price_str = json_object_get_string(obj, "price");
      od.mLimitPrice = price_str ? atof(price_str) : 0.0;
      const char* stopprice_str = json_object_get_string(obj, "stop_price");
      od.mStopPrice = stopprice_str ? atof(stopprice_str) : 0.0;
      const char* status_str = json_object_get_string(obj, "status");
      od.mStatus = status_str ? status_str : "";
      od.mLastExecutionTime = json_object_get_string(obj, "lastExecutionTime");
      result->mOrders.push_back(od);
    }
    json_value_free(val);
    result->mSuccess = true;
  }
  else
  {
    result->mSuccess = false;
  }

  rr.cleanup();
}

void PostOrders(mco_coro* co)
{
  auto* data = static_cast<PostOrderData*>(mco_get_user_data(co));

  int n_orders = (int)data->mOrders.size();
  if (n_orders == 0)
  {
    data->mSuccess = false;
    return;
  }

  char json_buf[4096] = {};
  {
    // https://www.interactivebrokers.eu/campus/ibkr-api-page/cpapi-v1/?utm_source=chatgpt.com#place-order
    JSON_Value* base = json_value_init_object();
    auto base_obj = json_value_get_object(base);

    auto arry = json_value_init_array();

    for (auto& entry : data->mOrders)
    {
      JSON_Value* order = json_value_init_object();
      auto order_obj = json_value_get_object(order);
      json_object_set_string(order_obj, "acctId", gGlobalData.mAccountId.c_str());
      json_object_set_number(order_obj, "conid", data->mConid);

      {
        char buf[64] = {};
        snprintf(buf, sizeof(buf), "%ld@SMART", data->mConid);
        json_object_set_string(order_obj, "conidex", buf);

        snprintf(buf, sizeof(buf), "%ld@STK", data->mConid);
        json_object_set_string(order_obj, "secType", buf);
      }

      if (!entry.mCOID.empty())
        json_object_set_string(order_obj, "cOID", entry.mCOID.c_str());
      if (!entry.mParentId.empty())
        json_object_set_string(order_obj, "parentId", entry.mParentId.c_str());

      json_object_set_string(order_obj, "orderType", entry.mOrderType.c_str());
      json_object_set_string(order_obj, "listingExchange", "NASDAQ");
      json_object_set_boolean(order_obj, "isSingleGroup", true);
      if (entry.mOrderType != "STP")
        json_object_set_boolean(order_obj, "outsideRTH", true);

      json_object_set_number(order_obj, "price", (double)entry.mPrice);
      json_object_set_number(order_obj, "auxPrice", (double)entry.mAuxPrice);

      json_object_set_string(order_obj, "side", entry.mBuy ? "BUY" : "SELL");
      // json_object_set_string(order_obj, "ticker", "AAPL");
      json_object_set_string(order_obj, "tif", "GTC");
      json_object_set_number(order_obj, "quantity", (double)entry.mQuantity);
      json_object_set_boolean(order_obj, "allOrNone", false);
      json_object_set_string(order_obj, "referrer", "QuickTrade");

      json_array_append_value(json_value_get_array(arry), order);
    }
    json_object_set_value(base_obj, "orders", arry);

    JSON_Status ret = json_serialize_to_buffer(base, json_buf, sizeof(json_buf));
    if (ret != JSONSuccess)
    {
      json_value_free(base);
      data->mSuccess = false;
      return;
    }
  }
  printf("%s\n", json_buf);

  ReqRes rr;
  naettOption* options[] = {naettMethod("POST"), naettHeader("Content-Type", "application/json"),
                            naettBody(json_buf, strlen(json_buf))};


  char url[512] = {};
  snprintf(url, sizeof(url), BASE_URL "/iserver/account/%s/orders", gGlobalData.mAccountId.c_str());
  rr.mReq = naettRequestWithOptions(url, 3, (const naettOption**)&options);
  rr.mRes = naettMake(rr.mReq);

  if (!rr.valid())
  {
    data->mSuccess = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.mRes);

  data->mAssignedOrderIds.clear();
  data->mOrderStatuses.clear();

  int status = naettGetStatus(rr.mRes);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.mRes, &sz);
    printf("%s\n", (const char*)body);

    JSON_Value* val = json_parse_string((const char*)body);

    JSON_Array* arr = json_value_get_array(val);
    if (arr && json_array_get_count(arr) > 0)
    {
      int n_responses = (int)json_array_get_count(arr);
      data->mSuccess = true;
      for (int i = 0; i < n_responses; i++)
      {
        JSON_Object* obj = json_array_get_object(arr, i);
        if (obj)
        {
          const char* oid = json_object_get_string(obj, "order_id");
          if (oid && strlen(oid) > 0)
          {
            data->mAssignedOrderIds.push_back(oid);
            const char* ost = json_object_get_string(obj, "order_status");
            data->mOrderStatuses.push_back(ost ? ost : "");
            const char* emsg = json_object_get_string(obj, "encrypt_message");
            if (emsg && i == 0)
              data->mEncryptMessage = emsg;
          }
          else
          {
            std::string err_msg;
            JSON_Array* msg_arr = json_object_get_array(obj, "message");
            if (msg_arr && json_array_get_count(msg_arr) > 0)
            {
              const char* err = json_array_get_string(msg_arr, 0);
              if (err) err_msg = err;
            }
            if (err_msg.empty())
            {
              const char* msg = json_object_get_string(obj, "message");
              if (msg) err_msg = msg;
            }
            data->mAssignedOrderIds.push_back("");
            data->mOrderStatuses.push_back(err_msg.empty() ? "FAILED" : err_msg);
            data->mSuccess = false;
          }
        }
      }
    }
    else
    {
      JSON_Object* obj = json_value_get_object(val);
      if (obj)
      {
        const char* err = json_object_get_string(obj, "error");
        data->mOrderStatuses.push_back(err ? err : "Unknown error");
        data->mSuccess = false;
      }
    }

    json_value_free(val);
  }
  else
  {
    int sz = 0;
    const void* body = naettGetBody(rr.mRes, &sz);
    printf("Error placing order: HTTP %d\n", status);
    printf("%s\n", (const char*)body);
    data->mSuccess = false;
  }

  rr.cleanup();
}

void PostCloseOrder(mco_coro* co)
{
  auto* data = static_cast<CloseOrderData*>(mco_get_user_data(co));

  // Step 1: Fetch snapshot bid/ask/last
  time_t now = time(NULL);
  now -= 3600;
  struct tm* tmt = localtime(&now);
  time_t epoch = mktime(tmt);

  char snap_url[512];
  snprintf(snap_url, sizeof(snap_url),
           BASE_URL "/iserver/marketdata/snapshot?conids=%d&since=%lld&fields=31,84,86",
           data->mConid, epoch);

  ReqRes rr = make_get(snap_url);
  if (!rr.valid())
  {
    data->mSuccess = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.mRes);

  double bid = 0.0, ask = 0.0, last = 0.0;
  int snap_status = naettGetStatus(rr.mRes);
  if (snap_status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.mRes, &sz);
    printf("%s\n", (const char*)body);

    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Array* arr = json_value_get_array(val);
    if (arr && json_array_get_count(arr) > 0)
    {
      JSON_Object* obj = json_array_get_object(arr, 0);
      const char* last_str = json_object_get_string(obj, "31");
      last = last_str ? atof(last_str) : 0.0;
      const char* bid_str = json_object_get_string(obj, "84");
      bid = bid_str ? atof(bid_str) : 0.0;
      const char* ask_str = json_object_get_string(obj, "86");
      ask = ask_str ? atof(ask_str) : 0.0;
    }
    json_value_free(val);
  }
  rr.cleanup();

  double limit_price = data->mBuy ? (ask > 0.0 ? ask : last) : (bid > 0.0 ? bid : last);
  if (limit_price <= 0.0)
  {
    data->mSuccess = false;
    return;
  }
  // limitPrice += 100.0; //for testing only


  // Step 2: Place limit order
  char json_buf[1024] = {};
  {
    JSON_Value* base = json_value_init_object();
    auto base_obj = json_value_get_object(base);
    auto arry = json_value_init_array();

    {
      JSON_Value* order = json_value_init_object();
      auto order_obj = json_value_get_object(order);
      json_object_set_string(order_obj, "acctId", gGlobalData.mAccountId.c_str());
      json_object_set_number(order_obj, "conid", data->mConid);

      {
        char buf[64] = {};
        snprintf(buf, sizeof(buf), "%ld@SMART", data->mConid);
        json_object_set_string(order_obj, "conidex", buf);
        snprintf(buf, sizeof(buf), "%ld:STK", data->mConid);
        json_object_set_string(order_obj, "secType", buf);
      }

      json_object_set_null(order_obj, "parentId");
      json_object_set_string(order_obj, "orderType", "LMT");
      json_object_set_string(order_obj, "listingExchange", "NASDAQ");
      json_object_set_boolean(order_obj, "isSingleGroup", true);
      json_object_set_boolean(order_obj, "outsideRTH", true);
      json_object_set_number(order_obj, "price", limit_price);
      json_object_set_string(order_obj, "side", data->mBuy ? "BUY" : "SELL");
      json_object_set_string(order_obj, "ticker", data->mSymbol.c_str());
      json_object_set_string(order_obj, "tif", "GTC");
      json_object_set_number(order_obj, "quantity", (int)data->mQuantity);
      json_object_set_boolean(order_obj, "allOrNone", false);
      json_object_set_string(order_obj, "referrer", "QuickTrade");

      json_array_append_value(json_value_get_array(arry), order);
    }
    json_object_set_value(base_obj, "orders", arry);

    JSON_Status ret = json_serialize_to_buffer(base, json_buf, sizeof(json_buf));
    if (ret != JSONSuccess)
    {
      json_value_free(base);
      data->mSuccess = false;
      return;
    }
  }
  printf("%s\n", json_buf);

  ReqRes rr2;
  naettOption* options[] = {naettMethod("POST"), naettHeader("Content-Type", "application/json"),
                            naettBody(json_buf, strlen(json_buf))};

  char url[512] = {};
  snprintf(url, sizeof(url), BASE_URL "/iserver/account/%s/orders", gGlobalData.mAccountId.c_str());
  rr2.mReq = naettRequestWithOptions(url, 3, (const naettOption**)&options);
  rr2.mRes = naettMake(rr2.mReq);

  if (!rr2.valid())
  {
    data->mSuccess = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr2.mRes);

  int status = naettGetStatus(rr2.mRes);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr2.mRes, &sz);
    printf("%s\n", (const char*)body);

    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Array* arr = json_value_get_array(val);
    if (arr && json_array_get_count(arr) > 0)
    {
      JSON_Object* obj = json_array_get_object(arr, 0);
      if (obj)
      {
        const char* oid = json_object_get_string(obj, "order_id");
        if (oid && strlen(oid) > 0)
        {
          data->mAssignedOrderId = oid;
          const char* ost = json_object_get_string(obj, "order_status");
          if (ost) data->mOrderStatus = ost;
          const char* emsg = json_object_get_string(obj, "encrypt_message");
          if (emsg) data->mEncryptMessage = emsg;
          data->mSuccess = true;
        }
        else
        {
          JSON_Array* msg_arr = json_object_get_array(obj, "message");
          if (msg_arr && json_array_get_count(msg_arr) > 0)
          {
            const char* err = json_array_get_string(msg_arr, 0);
            if (err) data->mOrderStatus = err;
          }
          if (data->mOrderStatus.empty())
          {
            const char* msg = json_object_get_string(obj, "message");
            if (msg) data->mOrderStatus = msg;
          }
          data->mSuccess = false;
        }
      }
    }
    else
    {
      JSON_Object* obj = json_value_get_object(val);
      if (obj)
      {
        const char* err = json_object_get_string(obj, "error");
        if (err) data->mOrderStatus = err;
        data->mSuccess = false;
      }
    }
    json_value_free(val);
  }
  else
  {
    int sz = 0;
    const void* body = naettGetBody(rr2.mRes, &sz);
    printf("Error placing order: HTTP %d\n", status);
    printf("%s\n", (const char*)body);
    data->mSuccess = false;
  }

  rr2.cleanup();
}

void CancelOrder(mco_coro* co)
{
  int order_id = (int)mco_get_user_data(co);
  
  CancelOrderData& pc = gGlobalData.mPendingCancels[order_id];

  char url[512] = {};
  snprintf(url, sizeof(url), BASE_URL "/iserver/account/%s/order/%d", gGlobalData.mAccountId.c_str(), order_id);

  ReqRes rr;
  naettOption* options[] = {naettMethod("DELETE")};
  rr.mReq = naettRequestWithOptions(url, 1, (const naettOption**)&options);
  rr.mRes = naettMake(rr.mReq);

  if (!rr.valid())
    return;

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.mRes);

  int status = naettGetStatus(rr.mRes);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.mRes, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Object* obj = json_value_get_object(val);
    if (obj)
    {
      const char* msg = json_object_get_string(obj, "message");
      if (msg)
        printf(msg);
    }
    json_value_free(val);
    pc.mSuccess = true;
  }
  else
  {
    int sz = 0;
    const void* body = naettGetBody(rr.mRes, &sz);
    printf("Error cancelling order: HTTP %d\n", status);
    if (body && sz > 0) 
      printf("%s\n", (const char*)body);
    pc.mSuccess = false;
  }

  rr.cleanup();
}

void PostModifyOrder(mco_coro* co)
{
  auto* data = static_cast<ModifyOrderData*>(mco_get_user_data(co));

  char json_buf[2048] = {};
  {
    JSON_Value* base = json_value_init_object();
    auto base_obj = json_value_get_object(base);

    json_object_set_number(base_obj, "orderId", data->mOrderId);
    json_object_set_number(base_obj, "conid", data->mConid);
    json_object_set_string(base_obj, "orderType", data->mOrderType.c_str());
    json_object_set_string(base_obj, "side", data->mSide.c_str());
    json_object_set_string(base_obj, "tif", "GTC");
    json_object_set_number(base_obj, "price", data->mNewPrice);
    json_object_set_number(base_obj, "quantity", data->mNewQuantity);

    JSON_Status ret = json_serialize_to_buffer(base, json_buf, sizeof(json_buf));
    if (ret != JSONSuccess)
    {
      json_value_free(base);
      data->mSuccess = false;
      return;
    }
    json_value_free(base);
  }
  printf("%s\n", json_buf);

  ReqRes rr;
  naettOption* options[] = {naettMethod("POST"), naettHeader("Content-Type", "application/json"),
                            naettBody(json_buf, strlen(json_buf))};

  char url[512] = {};
  snprintf(url, sizeof(url), BASE_URL "/iserver/account/%s/order/%d",
           gGlobalData.mAccountId.c_str(), data->mOrderId);
  rr.mReq = naettRequestWithOptions(url, 3, (const naettOption**)&options);
  rr.mRes = naettMake(rr.mReq);

  if (!rr.valid())
  {
    data->mSuccess = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.mRes);

  int status = naettGetStatus(rr.mRes);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.mRes, &sz);
    printf("%s\n", (const char*)body);

    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Array* arr = json_value_get_array(val);
    if (arr && json_array_get_count(arr) > 0)
    {
      JSON_Object* obj = json_array_get_object(arr, 0);
      if (obj)
      {
        const char* oid = json_object_get_string(obj, "order_id");
        if (oid && strlen(oid) > 0)
        {
          data->mAssignedOrderId = oid;
          const char* ost = json_object_get_string(obj, "order_status");
          if (ost) data->mOrderStatus = ost;
          const char* emsg = json_object_get_string(obj, "encrypt_message");
          if (emsg) data->mEncryptMessage = emsg;
          data->mSuccess = true;
        }
        else
        {
          JSON_Array* msg_arr = json_object_get_array(obj, "message");
          if (msg_arr && json_array_get_count(msg_arr) > 0)
          {
            const char* err = json_array_get_string(msg_arr, 0);
            if (err) data->mOrderStatus = err;
          }
          if (data->mOrderStatus.empty())
          {
            const char* msg = json_object_get_string(obj, "message");
            if (msg) data->mOrderStatus = msg;
          }
          data->mSuccess = false;
        }
      }
    }
    else
    {
      JSON_Object* obj = json_value_get_object(val);
      if (obj)
      {
        const char* err = json_object_get_string(obj, "error");
        if (err) data->mOrderStatus = err;
        data->mSuccess = false;
      }
    }
    json_value_free(val);
  }
  else
  {
    int sz = 0;
    const void* body = naettGetBody(rr.mRes, &sz);
    printf("Error modifying order %d: HTTP %d\n", data->mOrderId, status);
    printf("%s\n", (const char*)body);
    data->mSuccess = false;
  }

  rr.cleanup();
}

void PostSuppressQuestions(mco_coro* co)
{
  char json_buf[256] = {};
  {
    JSON_Value* val = json_value_init_object();
    auto obj = json_value_get_object(val);

    JSON_Value* arry = json_value_init_array();
    json_array_append_string(json_value_get_array(arry), "o163");
    json_array_append_string(json_value_get_array(arry), "o354");
    json_array_append_string(json_value_get_array(arry), "o383");
    json_array_append_string(json_value_get_array(arry), "o451");
    json_array_append_string(json_value_get_array(arry), "o10152");
    json_array_append_string(json_value_get_array(arry), "o10153");
    json_array_append_string(json_value_get_array(arry), "o10331");
    json_array_append_string(json_value_get_array(arry), "o10336");
    json_array_append_string(json_value_get_array(arry), "p12");
    json_object_set_value(obj, "messageIds", arry);

    JSON_Status ret = json_serialize_to_buffer(val, json_buf, sizeof(json_buf));
    if (ret != JSONSuccess)
    {
      json_value_free(val);
      return;
    }
    json_value_free(val);
  }

  ReqRes rr;
  naettOption* options[] = {naettMethod("POST"), naettHeader("Content-Type", "application/json"),
                            naettBody(json_buf, strlen(json_buf))};

  rr.mReq = naettRequestWithOptions(BASE_URL "/iserver/questions/suppress", 3, (const naettOption**)&options);
  rr.mRes = naettMake(rr.mReq);

  if (!rr.valid())
    return;

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.mRes);

  int status = naettGetStatus(rr.mRes);
  if (status == 200)
  {
    printf("Successfully suppressed questions\n");
  }
  else
  {
    int sz = 0;
    const void* body = naettGetBody(rr.mRes, &sz);
    printf("Error suppressing questions: HTTP %d\n", status);
    if (body && sz > 0)
      printf("%s\n", (const char*)body);
  }

  rr.cleanup();
}
