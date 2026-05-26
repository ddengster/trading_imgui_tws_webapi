
#include "im_api.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "coroutine/coroutine_mgt.h"
#include "naett/naett.h"
#include "parson/parson.h"
#include <unordered_map>

void PollAuthStatus(mco_coro* co)
{
  naettReq* req = nullptr;
  naettRes* res = nullptr;

  naettOption* options[] = {naettMethod("GET")};
  req =
    naettRequestWithOptions("https://localhost:5000/v1/api/iserver/auth/status",
                            sizeof(options) / sizeof(options[0]), (const naettOption**)&options);
  res = naettMake(req);

  if (!req || !res)
  {
    int ret = -1;
    mco_push(co, &ret, sizeof(ret));
    ret = 0;
    mco_push(co, &ret, sizeof(ret));
    return;
  }

  yield();

  auto isComplete = [](void* userdata)
  {
    naettRes* res = static_cast<naettRes*>(userdata);
    return naettComplete(res) != 0;
  };
  yield_until_true(isComplete, res);

  int out_statuscode = naettGetStatus(res);
  
  int authenticated = 0;
  if (out_statuscode == 200)
  {
    int sz = 0;
    const void* body = naettGetBody(res, &sz);
    JSON_Value* val = json_parse_string((const char*)body);
    JSON_Object* obj = json_value_get_object(val);
    authenticated = json_object_get_boolean(obj, "authenticated");
    
    json_value_free(val);
  }
  
  mco_push(co, &out_statuscode, sizeof(out_statuscode));
  mco_push(co, &authenticated, sizeof(authenticated));

  naettFree(req);
  naettClose(res);
  req = nullptr;
  res = nullptr;
}

int PollAuthStatus(int& out_statuscode, bool& auth)
{
  static naettReq* req = nullptr;
  static naettRes* res = nullptr;

  if (req == nullptr)
  {
    naettOption* options[] = {naettMethod("GET")};
    req =
      naettRequestWithOptions("https://localhost:5000/v1/api/iserver/auth/status",
                              sizeof(options) / sizeof(options[0]), (const naettOption**)&options);
    res = naettMake(req);
    return 0;
  }

  if (res && naettComplete(res))
  {
    out_statuscode = naettGetStatus(res);

    if (out_statuscode == 200)
    {
      int sz = 0;
      const void* body = naettGetBody(res, &sz);
      JSON_Value* val = json_parse_string((const char*)body);
      JSON_Object* obj = json_value_get_object(val);
      int authenticated = json_object_get_boolean(obj, "authenticated");
      auth = authenticated ? true : false;
      json_value_free(val);

      naettFree(req);
      naettClose(res);
      req = nullptr;
      res = nullptr;

      return authenticated ? 1 : 0;
    }

    naettFree(req);
    naettClose(res);
    req = nullptr;
    res = nullptr;
    return 0;
  }

  return 0;
}

int PollAccountId(std::string& out_accountId)
{
  static naettReq* req = nullptr;
  static naettRes* res = nullptr;

  if (req == nullptr)
  {
    naettOption* options[] = {naettMethod("GET")};
    req =
      naettRequestWithOptions("https://localhost:5000/v1/api/portfolio/accounts",
                              sizeof(options) / sizeof(options[0]), (const naettOption**)&options);
    res = naettMake(req);
    return 0;
  }

  if (res && naettComplete(res))
  {
    int status = naettGetStatus(res);
    if (status == 200)
    {
      int sz = 0;
      const void* body = naettGetBody(res, &sz);
      JSON_Value* val = json_parse_string((const char*)body);
      JSON_Array* arr = json_value_get_array(val);
      int n = json_array_get_count(arr);
      for (int i = 0; i < n; ++i)
      {
        JSON_Object* obj = json_array_get_object(arr, i);
        const char* id = json_object_get_string(obj, "accountId");
        if (id)
        {
          out_accountId = id;
          break;
        }
      }
      json_value_free(val);
    }

    naettFree(req);
    naettClose(res);
    req = nullptr;
    res = nullptr;
    return (status == 200 && !out_accountId.empty()) ? 1 : -1;
  }

  return 0;
}

int PollPositions(const std::string& accountId, std::vector<PositionData>& out_positions,
                  bool force_reset)
{
  static naettReq* req = nullptr;
  static naettRes* res = nullptr;
  static int page = 0;
  static bool complete = false;
  if (force_reset)
    complete = false;

  if (complete)
    return 1;

  if (req == nullptr)
  {
    //@note: only 1 page, given 30 positions per page
    char url[256];
    snprintf(url, sizeof(url), "https://localhost:5000/v1/api/portfolio/%s/positions/%d",
             accountId.c_str(), page);
    naettOption* options[] = {naettMethod("GET")};
    req = naettRequestWithOptions(url, sizeof(options) / sizeof(options[0]),
                                  (const naettOption**)&options);
    res = naettMake(req);
    return 0;
  }

  if (res && naettComplete(res))
  {
    int status = naettGetStatus(res);
    if (status == 200)
    {
      out_positions.clear();
      int sz = 0;
      const void* body = naettGetBody(res, &sz);
      JSON_Value* val = json_parse_string((const char*)body);
      JSON_Array* arr = json_value_get_array(val);
      int n = json_array_get_count(arr);
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
        out_positions.push_back(pd);
      }
      json_value_free(val);
      complete = true;
    }
    else
    {
      complete = false;
    }

    naettFree(req);
    naettClose(res);
    req = nullptr;
    res = nullptr;
  }

  return complete ? 1 : 0;
}


int PollLedger(const std::string& accountId, SummaryData& out_summary)
{
  static naettReq* req = nullptr;
  static naettRes* res = nullptr;
  static bool done = false;

  if (done)
    return 1;

  if (req == nullptr)
  {
    char url[256];
    snprintf(url, sizeof(url), "https://localhost:5000/v1/api/portfolio/%s/ledger",
             accountId.c_str());
    naettOption* options[] = {naettMethod("GET")};
    req = naettRequestWithOptions(url, sizeof(options) / sizeof(options[0]),
                                  (const naettOption**)&options);
    res = naettMake(req);
    return 0;
  }

  if (res && naettComplete(res))
  {
    int status = naettGetStatus(res);
    if (status == 200)
    {
      int sz = 0;
      const void* body = naettGetBody(res, &sz);
      JSON_Value* val = json_parse_string((const char*)body);
      JSON_Object* obj = json_value_get_object(val);

      JSON_Object* cashObj = json_object_get_object(obj, "USD");
      if (cashObj)
        out_summary.cashUSD = json_object_get_number(cashObj, "cashbalance");

      cashObj = json_object_get_object(obj, "SGD");
      if (cashObj)
        out_summary.cashSGD = json_object_get_number(cashObj, "cashbalance");

      cashObj = json_object_get_object(obj, "BASE");
      if (cashObj)
        out_summary.netLiquidationValSGD = json_object_get_number(cashObj, "netliquidationvalue");

      json_value_free(val);
      done = true;
    }
    else
    {
      done = true;
    }

    naettFree(req);
    naettClose(res);
    req = nullptr;
    res = nullptr;
  }

  return done ? 1 : 0;
}


int PollSummary(const std::string& accountId, SummaryData& out_summary)
{
  static naettReq* req = nullptr;
  static naettRes* res = nullptr;
  static bool done = false;

  if (done)
    return 1;

  if (req == nullptr)
  {
    char url[256];
    snprintf(url, sizeof(url), "https://localhost:5000/v1/api/portfolio/%s/summary",
             accountId.c_str());
    naettOption* options[] = {naettMethod("GET")};
    req = naettRequestWithOptions(url, sizeof(options) / sizeof(options[0]),
                                  (const naettOption**)&options);
    res = naettMake(req);
    return 0;
  }

  if (res && naettComplete(res))
  {
    int status = naettGetStatus(res);
    if (status == 200)
    {
      int sz = 0;
      const void* body = naettGetBody(res, &sz);
      JSON_Value* val = json_parse_string((const char*)body);
      JSON_Object* obj = json_value_get_object(val);

      JSON_Object* cashObj = json_object_get_object(obj, "buyingpower");
      if (cashObj)
        out_summary.buyingPowerSGD = json_object_get_number(cashObj, "amount");

      json_value_free(val);
      done = true;
    }
    else
    {
      done = true;
    }

    naettFree(req);
    naettClose(res);
    req = nullptr;
    res = nullptr;
  }

  return done ? 1 : 0;
}

int PollMarketDataHistory(int conid, std::vector<MarketDataPoint>& out_data, bool force_reset)
{
  struct ReqState
  {
    naettReq* req = nullptr;
    naettRes* res = nullptr;
    bool done = false;
  };
  static std::unordered_map<int, ReqState> states;

  ReqState& s = states[conid];

  if (force_reset)
  {
    if (s.req)
    {
      naettFree(s.req);
      naettClose(s.res);
    }
    s = ReqState();
  }

  if (s.done)
    return 1;

  if (s.req == nullptr)
  {
    char url[512];
    snprintf(url, sizeof(url),
             "https://localhost:5000/v1/api/iserver/marketdata/"
             "history?conid=%d&period=2h&bar=5min&outsideRth=true",
             conid);
    naettOption* options[] = {naettMethod("GET")};
    s.req = naettRequestWithOptions(url, sizeof(options) / sizeof(options[0]),
                                    (const naettOption**)&options);
    s.res = naettMake(s.req);
    return 0;
  }

  if (s.res && naettComplete(s.res))
  {
    int status = naettGetStatus(s.res);
    if (status == 200)
    {
      out_data.clear();
      int sz = 0;
      const void* body = naettGetBody(s.res, &sz);
      JSON_Value* val = json_parse_string((const char*)body);
      JSON_Object* obj = json_value_get_object(val);
      JSON_Array* arr = json_object_get_array(obj, "data");
      int count = (int)json_array_get_count(arr);
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
        out_data.push_back(pt);
      }
      json_value_free(val);
      s.done = true;
    }
    else
    {
      s.done = true;
    }

    naettFree(s.req);
    naettClose(s.res);
    s.req = nullptr;
    s.res = nullptr;
  }

  return s.done ? 1 : 0;
}

int PollConId(const std::string& symbol, std::vector<ExchContractId>& out, bool force_reset)
{
  static naettReq* req = nullptr;
  static naettRes* res = nullptr;
  static bool done = false;

  if (force_reset)
    done = false;

  if (done)
    return 1;

  if (req == nullptr)
  {
    char url[512];
    snprintf(url, sizeof(url), "https://localhost:5000/v1/api/trsrv/stocks?symbols=%s",
             symbol.c_str());
    naettOption* options[] = {naettMethod("GET")};
    req = naettRequestWithOptions(url, sizeof(options) / sizeof(options[0]),
                                  (const naettOption**)&options);
    res = naettMake(req);
    return 0;
  }

  if (res && naettComplete(res))
  {
    int status = naettGetStatus(res);
    if (status == 200)
    {
      out.clear();
      int sz = 0;
      const void* body = naettGetBody(res, &sz);
      JSON_Value* val = json_parse_string((const char*)body);
      JSON_Object* obj = json_value_get_object(val);
      JSON_Array* arr = json_object_get_array(obj, symbol.c_str());
      int count = (int)json_array_get_count(arr);
      if (arr && count > 0)
      {
        const char* allowed_exch[] = {"SMART", "NYSE", "NASDAQ", "BATS"};
        int allowed_exch_count = sizeof(allowed_exch) / sizeof(allowed_exch[0]);

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

            for (int k = 0; k < allowed_exch_count; ++k)
            {
              if (strcmp(exch, allowed_exch[k]) == 0)
              {
                int out_conid = (int)json_object_get_number(contract, "conid");
                out.push_back({full_stockname, exch, out_conid});
                break;
              }
            }
          }
        }
      }
      json_value_free(val);
      done = true;
    }
    else
    {
      done = true;
    }

    naettFree(req);
    naettClose(res);
    req = nullptr;
    res = nullptr;
  }

  return done ? 1 : 0;
}
