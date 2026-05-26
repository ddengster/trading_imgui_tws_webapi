
#include "coroutine_mgt.h"

#define MINICORO_IMPL
#include "minicoro.h"

#include <assert.h>
#include <unordered_map>

static std::unordered_map<int, mco_coro*> gCoroutineMgr;
static int gNextCoroutineId = 0;

int create_managed_coroutine(void (*co_func)(mco_coro*), void* user_data)
{
  mco_desc desc = mco_desc_init(co_func, 0);
  desc.user_data = user_data;

  mco_coro* co = nullptr;
  mco_result res = mco_create(&co, &desc);
  assert(res == MCO_SUCCESS && "Failed to make coroutine");

  gCoroutineMgr[gNextCoroutineId] = co;
  ++gNextCoroutineId;
  return gNextCoroutineId - 1;
}

mco_coro* get_coroutine(int handle)
{
  assert(handle >= 0 && handle < gNextCoroutineId && "Invalid coroutine handle");
  auto it = gCoroutineMgr.find(handle);
  if (it != gCoroutineMgr.end())
    return it->second;
  return nullptr;
}

void destroy_coroutine(int handle)
{
  mco_coro* co = get_coroutine(handle);
  if (co)
  {
    mco_destroy(co);
    gCoroutineMgr.erase(handle);
  }
}

void process_coroutines()
{
  for (auto& pair : gCoroutineMgr)
  {
    mco_coro* co = pair.second;
    if (mco_status(co) == MCO_SUSPENDED)
      mco_resume(co);
  }
}

void destroy_all_coroutines()
{
  for (auto& pair : gCoroutineMgr)
  {
    mco_coro* co = pair.second;
    mco_destroy(co);
  }
  gCoroutineMgr.clear();
}