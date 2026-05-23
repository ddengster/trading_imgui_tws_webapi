
#include "trading_imgui.h"
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include "naett/naett.h"

naettReq* gStatusReq = nullptr;
naettRes* gStatusRes = nullptr;

void ConnectingState()
{
  ImGui::Begin("Connecting", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::NewLine();

  ImGui::Text("Run clientportal/run.bat to launch the client portal/server.");
  ImGui::Text("Pinging https://localhost:5000/v1/api/iserver/auth/status for alive check...");
  ImGui::NewLine();
  ImGui::NewLine();

  if (ImGui::Button("IBKR Sign in")) {
    ShellExecute(NULL, "open", "https://localhost:5000/", NULL, NULL, SW_SHOWNORMAL);
  }

  naettOption* options[] = { naettMethod("GET") };

  if (!gStatusReq)
  {
    gStatusReq = naettRequestWithOptions("https://localhost:5000/v1/api/iserver/auth/status",
      sizeof(options) / sizeof(options[0]), (const naettOption**)&options);

    gStatusRes = naettMake(gStatusReq);
  }

  static int statuscode = -1;
  static bool done = false;
  if (gStatusRes)
  {
    if (naettComplete(gStatusRes))
    {
      printf("response complete\n");
      statuscode = naettGetStatus(gStatusRes);
      
      if (statuscode == 200)
        done = true;
      else
      {
        naettFree(gStatusReq);
        naettClose(gStatusRes);
        gStatusRes = nullptr;
        gStatusReq = nullptr;
      }
    }
  }

  ImGui::Text("statuscode: %d", statuscode);

  ImGui::End();
}