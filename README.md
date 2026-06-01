
![](ss.png)

# Trading imgui (via TWS Web Api)

Trading with IMGUI view. Contract search, risk-based orders, position order/management, stock charts.

TWS Documentation: https://interactivebrokers.github.io/cpwebapi/quickstart

TWS Web Api: https://www.interactivebrokers.eu/campus/ibkr-api-page/cpapi-v1/#accounts

## Setup & running

- Download java https://www.java.com/en/download/ and install it. `java --version` to see if it works

- Download https://download2.interactivebrokers.com/portal/clientportal.gw.zip and unzip it.

- Open a cmd prompt in the unzipped folder and run `bin\run.bat root\conf.yaml`

- Open https://localhost:5000 on a web browser, skip the ssl insecure notice. You should see a IBKR login screen, login. The message 'Client login succeeds' will be displayed if the login is successful.

- Run the program `bin/trading_imgui_tws_webapi.exe`. It will make webapi calls through this medium

- Optionally, can test with `curl --url https://localhost:5000/v1/api/iserver/auth/status --request GET -k`

