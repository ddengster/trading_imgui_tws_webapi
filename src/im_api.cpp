
#include "im_api.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "coroutine/coroutine_mgt.h"
#include "naett/naett.h"
#include "parson/parson.h"
#include <ctime>
#include <unordered_map>

#define BASE_URL "https://localhost:5000/v1/api"

extern std::string gAccountId;
extern std::unordered_map<int, CancelOrderData> gPendingCancels;

struct ReqRes
{
  naettReq* req = nullptr;
  naettRes* res = nullptr;

  bool valid() const { return req && res; }
  void cleanup()
  {
    if (req)
      naettFree(req);
    if (res)
      naettClose(res);
    req = nullptr;
    res = nullptr;
  }
};

static ReqRes make_get(const char* url)
{
  ReqRes rr;
  naettOption* options[] = {naettMethod("GET")};
  rr.req = naettRequestWithOptions(url, 1, (const naettOption**)&options);
  rr.res = naettMake(rr.req);
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
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.res);

  int out_statuscode = naettGetStatus(rr.res);

  int authenticated = 0;
  if (out_statuscode == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.res, &sz);
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
    result->success = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.res);

  int status = naettGetStatus(rr.res);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.res, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Array* arr = json_value_get_array(val);
    int n = json_array_get_count(arr);
    for (int i = 0; i < n; ++i)
    {
      JSON_Object* obj = json_array_get_object(arr, i);
      const char* id = json_object_get_string(obj, "accountId");
      if (id)
      {
        result->accountId = id;
        break;
      }
    }
    json_value_free(val);
    result->success = !result->accountId.empty();
  }
  else
  {
    result->success = false;
  }

  rr.cleanup();
}

void PollPositions(mco_coro* co)
{
  // printf("query positions\n");
  auto* result = static_cast<PositionsResult*>(mco_get_user_data(co));

  char url[256];
  snprintf(url, sizeof(url), BASE_URL "/portfolio/%s/positions/0", result->accountId.c_str());

  ReqRes rr = make_get(url);

  if (!rr.valid())
  {
    result->success = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.res);

  int status = naettGetStatus(rr.res);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.res, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Array* arr = json_value_get_array(val);
    int n = json_array_get_count(arr);
    result->positions.clear();
    result->positions.reserve(n);
    for (int i = 0; i < n; ++i)
    {
      JSON_Object* obj = json_array_get_object(arr, i);
      const char* contractDesc = json_object_get_string(obj, "contractDesc");
      double position = json_object_get_number(obj, "position");
      double avgCost = json_object_get_number(obj, "avgCost");
      double mktPrice = json_object_get_number(obj, "mktPrice");
      double mktValue = json_object_get_number(obj, "mktValue");
      double realizedPnl = json_object_get_number(obj, "realizedPnl");
      double unrealizedPnl = json_object_get_number(obj, "unrealizedPnl");
      const char* assetclass = json_object_get_string(obj, "assetClass");

      std::string desc = contractDesc ? contractDesc : "";
      std::string symbol;
      size_t firstSpace = desc.find(' ');
      if (firstSpace != std::string::npos)
        symbol = desc.substr(0, firstSpace);
      else
        symbol = desc;

      PositionData pd;
      pd.symbol = symbol;
      pd.secType = contractDesc;
      pd.assetClass = assetclass;
      pd.size = position;
      pd.averageCost = avgCost;
      pd.marketPrice = mktPrice;
      pd.marketValue = mktValue;
      pd.realizedPNL = realizedPnl;
      pd.unrealizedPNL = unrealizedPnl;
      result->positions.push_back(pd);
    }
    json_value_free(val);
    result->success = true;
  }
  else
  {
    result->success = false;
  }

  rr.cleanup();
}

void PollLedger(mco_coro* co)
{
  // printf("query ledger\n");
  auto* result = static_cast<LedgerResult*>(mco_get_user_data(co));

  char url[256];
  snprintf(url, sizeof(url), BASE_URL "/portfolio/%s/ledger", result->accountId.c_str());

  ReqRes rr = make_get(url);

  if (!rr.valid())
  {
    result->success = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.res);

  int status = naettGetStatus(rr.res);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.res, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Object* obj = json_value_get_object(val);

    JSON_Object* cashObj = json_object_get_object(obj, "USD");
    if (cashObj)
      result->summary.cashUSD = json_object_get_number(cashObj, "cashbalance");

    cashObj = json_object_get_object(obj, "SGD");
    if (cashObj)
      result->summary.cashSGD = json_object_get_number(cashObj, "cashbalance");

    cashObj = json_object_get_object(obj, "BASE");
    if (cashObj)
      result->summary.netLiquidationValSGD = json_object_get_number(cashObj, "netliquidationvalue");

    json_value_free(val);
    result->success = true;
  }
  else
  {
    result->success = false;
  }

  rr.cleanup();
}

void PollSummary(mco_coro* co)
{
  // printf("query summary\n");
  auto* result = static_cast<SummaryResult*>(mco_get_user_data(co));

  char url[256];
  snprintf(url, sizeof(url), BASE_URL "/portfolio/%s/summary", result->accountId.c_str());

  ReqRes rr = make_get(url);

  if (!rr.valid())
  {
    result->success = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.res);

  int status = naettGetStatus(rr.res);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.res, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Object* obj = json_value_get_object(val);

    JSON_Object* cashObj = json_object_get_object(obj, "buyingpower");
    if (cashObj)
      result->summary.buyingPowerSGD = json_object_get_number(cashObj, "amount");

    json_value_free(val);
    result->success = true;
  }
  else
  {
    result->success = false;
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
           result->conid);

  ReqRes rr = make_get(url);

  if (!rr.valid())
  {
    result->success = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.res);

  int status = naettGetStatus(rr.res);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.res, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Object* obj = json_value_get_object(val);
    JSON_Array* arr = json_object_get_array(obj, "data");
    int count = (int)json_array_get_count(arr);
    result->data.clear();
    result->data.reserve(count);
    for (int i = 0; i < count; ++i)
    {
      JSON_Object* bar = json_array_get_object(arr, i);
      MarketDataPoint pt;
      pt.open = json_object_get_number(bar, "o");
      pt.high = json_object_get_number(bar, "h");
      pt.low = json_object_get_number(bar, "l");
      pt.close = json_object_get_number(bar, "c");
      pt.volume = json_object_get_number(bar, "v");
      pt.timestamp = json_object_get_number(bar, "t");
      result->data.push_back(pt);
    }
    json_value_free(val);
    result->success = true;
  }
  else
  {
    result->success = false;
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
           result->conid, epoch);

  ReqRes rr = make_get(url);

  if (!rr.valid())
  {
    result->success = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.res);

  int status = naettGetStatus(rr.res);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.res, &sz);
    JSON_Value* val = json_parse_string((const char*)body);

    JSON_Array* arr = json_value_get_array(val);
    if (arr && json_array_get_count(arr) > 0)
    {
      JSON_Object* obj = json_array_get_object(arr, 0);
      const char* last_str = json_object_get_string(obj, "31");
      result->last = last_str ? atof(last_str) : 0.0;
      const char* bid_str = json_object_get_string(obj, "84");
      result->bid = bid_str ? atof(bid_str) : 0.0;
      const char* ask_str = json_object_get_string(obj, "86");
      result->ask = ask_str ? atof(ask_str) : 0.0;
      result->success = true;
    }

    json_value_free(val);
  }
  else
  {
    result->success = false;
  }

  rr.cleanup();
}

void PollConId(mco_coro* co)
{
  auto* result = static_cast<ConIdResult*>(mco_get_user_data(co));

  char url[512];
  snprintf(url, sizeof(url), BASE_URL "/trsrv/stocks?symbols=%s", result->symbol.c_str());

  ReqRes rr = make_get(url);

  if (!rr.valid())
  {
    result->success = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.res);

  int status = naettGetStatus(rr.res);
  if (status == 200)
  {
    const char* skipped_exch[] = {"MEXI"};
    int skipped_exch_count = sizeof(skipped_exch) / sizeof(skipped_exch[0]);

    int sz = 0;
    const void* body = naettGetBody(rr.res, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Object* obj = json_value_get_object(val);
    JSON_Array* arr = json_object_get_array(obj, result->symbol.c_str());
    int count = (int)json_array_get_count(arr);
    result->contracts.clear();
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
          result->contracts.push_back({full_stockname, exch, out_conid});
        }
      }
    }
    json_value_free(val);
    result->success = true;
  }
  else
  {
    result->success = false;
  }

  rr.cleanup();
}

void PollOrders(mco_coro* co)
{
  auto* result = static_cast<OrdersResult*>(mco_get_user_data(co));

  ReqRes rr = make_get(BASE_URL "/iserver/account/orders");

  if (!rr.valid())
  {
    result->success = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.res);

  int status = naettGetStatus(rr.res);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.res, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Object* root = json_value_get_object(val);
    JSON_Array* arr = json_object_get_array(root, "orders");
    int n = json_array_get_count(arr);
    result->orders.clear();
    result->orders.reserve(n);
    for (int i = 0; i < n; ++i)
    {
      JSON_Object* obj = json_array_get_object(arr, i);
      OrderData od;
      od.orderId = (int)json_object_get_number(obj, "orderId");
      const char* symbol = json_object_get_string(obj, "ticker");
      od.symbol = symbol ? symbol : "";
      const char* side = json_object_get_string(obj, "side");
      od.side = side ? side : "";
      const char* orderType = json_object_get_string(obj, "orderType");
      od.orderType = orderType ? orderType : "";
      od.totalSize = json_object_get_number(obj, "totalSize");
      od.filledQuantity = json_object_get_number(obj, "filledQuantity");
      od.remainingQuantity = json_object_get_number(obj, "remainingQuantity");
      const char* pricestr = json_object_get_string(obj, "price");
      od.limitPrice = pricestr ? atof(pricestr) : 0.0;
      const char* stopprice_str = json_object_get_string(obj, "stop_price");
      od.stopPrice = stopprice_str ? atof(stopprice_str) : 0.0;
      const char* statusStr = json_object_get_string(obj, "status");
      od.status = statusStr ? statusStr : "";
      od.lastExecutionTime = json_object_get_string(obj, "lastExecutionTime");
      result->orders.push_back(od);
    }
    json_value_free(val);
    result->success = true;
  }
  else
  {
    result->success = false;
  }

  rr.cleanup();
}

void PostOrders(mco_coro* co)
{
  auto* data = static_cast<PostOrderData*>(mco_get_user_data(co));

  char json_buf[1024] = {};
  {
    // https://www.interactivebrokers.eu/campus/ibkr-api-page/cpapi-v1/?utm_source=chatgpt.com#place-order
    JSON_Value* base = json_value_init_object();
    auto base_obj = json_value_get_object(base);

    auto arry = json_value_init_array();

    {
      JSON_Value* order = json_value_init_object();
      auto order_obj = json_value_get_object(order);
      json_object_set_string(order_obj, "acctId", gAccountId.c_str());
      json_object_set_number(order_obj, "conid", data->conid);

      {
        char buf[64] = {};
        snprintf(buf, sizeof(buf), "%ld@SMART", data->conid);
        json_object_set_string(order_obj, "conidex", buf);

        snprintf(buf, sizeof(buf), "%ld:STK", data->conid);
        json_object_set_string(order_obj, "secType", buf);
      }

      //json_object_set_string(order_obj, "cOID", "AAPL-BUY-1");
      json_object_set_null(order_obj, "parentId");

      json_object_set_string(order_obj, "orderType", data->orderType.c_str());
      json_object_set_string(order_obj, "listingExchange", "NASDAQ");
      json_object_set_boolean(order_obj, "isSingleGroup", true);
      json_object_set_boolean(order_obj, "outsideRTH", true);

      json_object_set_number(order_obj, "price", (double)data->price);
      //if (data->orderType == "STOP_LIMIT" || data->orderType == "TRAILLMT")
      json_object_set_number(order_obj, "auxPrice", (double)data->aux_price);

      json_object_set_string(order_obj, "side", data->buy ? "BUY" : "SELL");
      json_object_set_string(order_obj, "ticker", "AAPL");
      json_object_set_string(order_obj, "tif", "GTC");
      //json_object_set_number(order_obj, "trailingAmt", 0.0);
      json_object_set_number(order_obj, "quantity", (double)data->quantity);
      json_object_set_boolean(order_obj, "allOrNone", false);
      json_object_set_string(order_obj, "referrer", "QuickTrade");

      json_array_append_value(json_value_get_array(arry), order);
    }
    json_object_set_value(base_obj, "orders", arry);

    JSON_Status ret = json_serialize_to_buffer(base, json_buf, sizeof(json_buf));
    if (ret != JSONSuccess)
    {
      json_value_free(base);
      data->success = false;
      return;
    }
  }
  printf("%s\n", json_buf);

  ReqRes rr;
  naettOption* options[] = {naettMethod("POST"), naettHeader("Content-Type", "application/json"),
                            naettBody(json_buf, strlen(json_buf))};


  char url[512] = {};
  snprintf(url, sizeof(url), BASE_URL "/iserver/account/%s/orders", gAccountId.c_str());
  rr.req = naettRequestWithOptions(url, 3, (const naettOption**)&options);
  rr.res = naettMake(rr.req);

  if (!rr.valid())
  {
    data->success = false;
    return;
  }

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.res);

  int status = naettGetStatus(rr.res);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.res, &sz);
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
          data->order_id = oid;
          const char* ost = json_object_get_string(obj, "order_status");
          if (ost)
            data->order_status = ost;
          const char* emsg = json_object_get_string(obj, "encrypt_message");
          if (emsg)
            data->encrypt_message = emsg;
          data->success = true;
        }
        else
        {
          JSON_Array* msg_arr = json_object_get_array(obj, "message");
          if (msg_arr && json_array_get_count(msg_arr) > 0)
          {
            const char* err = json_array_get_string(msg_arr, 0);
            if (err) data->order_status = err;
          }
          if (data->order_status.empty())
          {
            const char* msg = json_object_get_string(obj, "message");
            if (msg) data->order_status = msg;
          }
          data->success = false;
        }
      }
    }
    else
    {
      JSON_Object* obj = json_value_get_object(val);
      if (obj)
      {
        const char* err = json_object_get_string(obj, "error");
        if (err) data->order_status = err;
        data->success = false;
      }
    }

    json_value_free(val);
  }
  else
  {
    int sz = 0;
    const void* body = naettGetBody(rr.res, &sz);
    printf("Error placing order: HTTP %d\n", status);
    printf("%s\n", (const char*)body);
    data->success = false;
  }

  rr.cleanup();
}

void CancelOrder(mco_coro* co)
{
  int orderId = (int)mco_get_user_data(co);
  
  CancelOrderData& pc = gPendingCancels[orderId];

  char url[512] = {};
  snprintf(url, sizeof(url), BASE_URL "/iserver/account/%s/order/%d", gAccountId.c_str(), orderId);

  ReqRes rr;
  naettOption* options[] = {naettMethod("DELETE")};
  rr.req = naettRequestWithOptions(url, 1, (const naettOption**)&options);
  rr.res = naettMake(rr.req);

  if (!rr.valid())
    return;

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.res);

  int status = naettGetStatus(rr.res);
  if (status == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(rr.res, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Object* obj = json_value_get_object(val);
    if (obj)
    {
      const char* msg = json_object_get_string(obj, "message");
      if (msg)
        printf(msg);
    }
    json_value_free(val);
    pc.success = true;
  }
  else
  {
    int sz = 0;
    const void* body = naettGetBody(rr.res, &sz);
    printf("Error cancelling order: HTTP %d\n", status);
    if (body && sz > 0) 
      printf("%s\n", (const char*)body);
    pc.success = false;
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

  rr.req = naettRequestWithOptions(BASE_URL "/iserver/questions/suppress", 3, (const naettOption**)&options);
  rr.res = naettMake(rr.req);

  if (!rr.valid())
    return;

  yield();
  yield_until_true([](void* ud) { return naettComplete((naettRes*)ud) != 0; }, rr.res);

  int status = naettGetStatus(rr.res);
  if (status == 200)
  {
    printf("Successfully suppressed questions\n");
  }
  else
  {
    int sz = 0;
    const void* body = naettGetBody(rr.res, &sz);
    printf("Error suppressing questions: HTTP %d\n", status);
    if (body && sz > 0)
      printf("%s\n", (const char*)body);
  }

  rr.cleanup();
}
