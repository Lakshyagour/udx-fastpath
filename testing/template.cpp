#include <bits/stdc++.h>
#include <jsoncpp/json/json.h>

using namespace std;
using namespace Json;

Value get_thread_template(Value configuration)
{
  Value threads;
  threads["main"]["group"] = "initial";
  threads["main"]["description"] = "setup thread";

  Value netdevs = configuration["lports"];
  for (auto it = netdevs.begin(); it != netdevs.end(); it++) {
    Value dev_name = it.key();
    Value lport = *it;
    int lport_id = 0;
    Value queues_info = lport["rx-tx-queues-info"];

    for (auto rx_tx_queue = queues_info.begin(); rx_tx_queue != queues_info.end(); rx_tx_queue++) {
      string key = "fwd:" + to_string(lport_id);
      threads[key]["group"] = "group" + (*rx_tx_queue)["core-id"].asString();
      threads[key]["lports"] = Json::arrayValue;
      threads[key]["lports"].append(dev_name.asString() + ":" + to_string(lport_id));
      threads[key]["description"] = "fwd thread";
      lport_id++;
    }
  }
  return threads;
}

Value get_lport_template(Value configuration)
{
  Value lports;

  Value netdevs = configuration["lports"];
  for (auto it = netdevs.begin(); it != netdevs.end(); it++) {
    Value dev_name = it.key();
    Value netdev = *it;
    int lport_id = 0;
    Value queues_info = netdev["rx-tx-queues-info"];

    for (auto rx_tx_queue : queues_info) {
      string key = dev_name.asString() + ":" + to_string(lport_id);
      lports[key]["description"] = "fwd thread";
      lports[key]["pmd"] = netdev["pmd"];
      lports[key]["umem"] = netdev["umem"];
      lports[key]["busy_poll"] = netdev["busy_poll"];
      lports[key]["busy_budget"] = netdev["busy_budget"];
      lports[key]["idle_timeout"] = netdev["idle_timeout"];
      lports[key]["intr_timeout"] = netdev["intr_timeout"];
      lports[key]["skb_mode"] = netdev["skb_mode"];
      lports[key]["xsk_pin_path"] = netdev["xsk_pin_path"];

      lports[key]["region"] = rx_tx_queue["region-index"];
      lports[key]["qid"] = rx_tx_queue["queue-id"];
      lport_id++;
    }
  }
  return lports;
}

Value get_default_template(Value configuration)
{
  Value defaults;
  defaults["bufcnt"] = 16;
  defaults["bufsz"] = 2;
  defaults["rxdesc"] = 2;
  defaults["txdesc"] = 2;
  defaults["cache"] = 256;
  defaults["mtype"] = "2MB";

  return defaults;
}

Value get_umem_template(Value configuration)
{
  return configuration["umems"];
}

Value get_lport_groups_template(Value configuration)
{
  Value lport_groups;
  lport_groups["intial"] = Json::arrayValue;
  lport_groups["intial"].append(configuration["main-lcore"]);

  Value netdevs = configuration["lports"];
  for (auto it = netdevs.begin(); it != netdevs.end(); it++) {
    Value dev_name = it.key();
    Value lport = *it;
    Value queues_info = lport["rx-tx-queues-info"];

    for (auto rx_tx_queue = queues_info.begin(); rx_tx_queue != queues_info.end(); rx_tx_queue++) {
      string key = "group" + (*rx_tx_queue)["core-id"].asString();
      lport_groups[key] = Json::arrayValue;
      lport_groups[key].append((*rx_tx_queue)["core-id"].asString());
    }
  }
  return lport_groups;
}

Value get_options_template(Value configuration)
{
  Value options;
  options["pkt_api"] = "xskdev";
  options["no-metrics"] = true;
  options["no-restapi"] = true;
  options["cli"] = false;
  return options;
}

int main()
{
  ifstream ifs("../udx-configs/datapath-config.jsonc");
  Reader reader;
  Value configuration, cndp_configuration;
  reader.parse(ifs, configuration);

  cndp_configuration["threads"] = get_thread_template(configuration["cndp-datapath-params"]);
  cndp_configuration["lport-groups"] =
      get_lport_groups_template(configuration["cndp-datapath-params"]);
  cndp_configuration["default"] = get_default_template(configuration["cndp-datapath-params"]);
  cndp_configuration["lport"] = get_lport_template(configuration["cndp-datapath-params"]);
  cndp_configuration["umem"] = get_umem_template(configuration["cndp-datapath-params"]);
  cndp_configuration["options"] = get_options_template(configuration["cndp-datapath-params"]);

  cout << cndp_configuration.toStyledString();
  return 0;
}