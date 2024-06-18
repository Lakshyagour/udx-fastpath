#include <cndp.h>
#include <cne.h>        // for cne_init, cne_on_exit, CNE_CALLED_EXIT, CNE_...
#include <cne_common.h> // for __cne_unused, cne_countof
#include <cne_gettid.h>
#include <cne_log.h> // for CNE_LOG_ERR, CNE_ERR, CNE_DEBUG, CNE_LOG_DEBUG
#include <cne_ring.h>
#include <cne_ring_api.h>
#include <cne_system.h> // for cne_lcore_id
#include <common.h>
#include <core.h>
#include <idlemgr.h>
#include <jcfg.h> // for jcfg_thd_t, jcfg_lport_t, jcfg_lport_by_index
#include <log.h>
#include <metrics.h> // for metrics_destroy
#include <pthread.h> // for pthread_barrier_wait, pthread_self, pthread_...
#include <sched.h>   // for cpu_set_t
#include <signal.h>  // for SIGUSR2, SIGINT
#include <stdio.h>   // for fflush, stdout
#include <stdlib.h>  // for calloc, free
#include <string.h>  // for memset
#include <tap.h>
#include <unistd.h> // for getpid, sleep, gettid

#define MAX_LPORTS 32
static struct fwd_info fwd_info;
static struct fwd_info* fwd = &fwd_info;
void* func_arg_for_thread_func[MAX_LPORTS];
pthread_barrier_t barrier;
cne_ring_t* worker_rx_rings[MAX_LPORTS];
cne_ring_t* worker_tx_rings[MAX_LPORTS];
vector<ether_addr> cndp_lport_mac;
DataPlaneManager* cndp_dataplane_manager;
CndpCallBackWrapper* cndp_callback_wrapper;

#define foreach_thd_lport(_t, _lp) \
  for (int _i = 0; _i < _t->lport_cnt && (_lp = _t->lports[_i]); _i++, _lp = _t->lports[_i])

#define TIMEOUT_VALUE                                    \
  1000 /* Number of times to wait for each usleep() time \
        */
enum thread_quit_state {
  THD_RUN = 0, /**< Thread should continue running */
  THD_QUIT,    /**< Thread should stop itself */
  THD_DONE,    /**< Thread should set this state when done */
};

uint16_t CndpCallBackWrapper::GetHeadRoom(void* pktbuffer) {
  return pktmbuf_headroom(reinterpret_cast<pktmbuf_t*>(pktbuffer));
}

uint16_t CndpCallBackWrapper::GetTailRoom(void* pktbuffer) {
  return pktmbuf_tailroom(reinterpret_cast<pktmbuf_t*>(pktbuffer));
}

uint16_t CndpCallBackWrapper::GetDataLength(void* pktbuffer) {
  return pktmbuf_data_len(reinterpret_cast<pktmbuf_t*>(pktbuffer));
}

char* CndpCallBackWrapper::GetDataAtOffset(void* pktbuffer, uint16_t offset) {
  return rte_pktmbuf_mtod_offset(reinterpret_cast<pktmbuf_t*>(pktbuffer), char*, offset);
}

char* CndpCallBackWrapper::PrependData(void* pktbuffer, uint16_t len) {
  return pktmbuf_prepend(reinterpret_cast<pktmbuf_t*>(pktbuffer), len);
}

char* CndpCallBackWrapper::AppendData(void* pktbuffer, uint16_t len) {
  return pktmbuf_append(reinterpret_cast<pktmbuf_t*>(pktbuffer), len);
}

char* CndpCallBackWrapper::AdjustOffset(void* pktbuffer, uint16_t offset) {
  return pktmbuf_adj_offset(reinterpret_cast<pktmbuf_t*>(pktbuffer), offset);
}

void* CndpCallBackWrapper::TrimPacket(void* pktbuffer, uint16_t len) {
  if (pktmbuf_trim(reinterpret_cast<pktmbuf_t*>(pktbuffer), len) < 0) {
    log_error("TrimPacket operation failed on buff of len:%d with offset:%d",
    pktmbuf_data_len(reinterpret_cast<pktmbuf_t*>(pktbuffer)), len);

    return NULL;
  }
  return pktbuffer;
}

Value get_thread_template(Value configuration) {
  Value threads;
  threads["main"]["group"]       = "initial";
  threads["main"]["description"] = "setup thread";
  int fwd_thread                 = 0;
  Value netdevs                  = configuration["lports"];
  for (auto it = netdevs.begin(); it != netdevs.end(); it++) {
    Value dev_name    = it.key();
    Value lport       = *it;
    int lport_id      = 0;
    Value queues_info = lport["rx-tx-queues-info"];

    for (auto rx_tx_queue = queues_info.begin(); rx_tx_queue != queues_info.end(); rx_tx_queue++) {
      string key             = "fwd:" + to_string(fwd_thread);
      threads[key]["group"]  = "group" + (*rx_tx_queue)["core-id"].asString();
      threads[key]["lports"] = Json::arrayValue;
      threads[key]["lports"].append(dev_name.asString() + ":" + to_string(lport_id));
      if (configuration.isMember("idle_timeout"))
        threads[key]["idle_timeout"] = configuration["idle_timeout"].asInt();
      if (configuration.isMember("intr_timeout"))
        threads[key]["intr_timeout"] = configuration["intr_timeout"].asInt();
      threads[key]["description"] = "fwd thread";
      lport_id++;
      fwd_thread++;
    }
  }
  return threads;
}

Value get_lport_template(Value configuration) {
  Value lports;
  Value netdevs = configuration["lports"];
  ether_addr dev_mac;
  for (auto it = netdevs.begin(); it != netdevs.end(); it++) {
    Value dev_name = it.key().asString();
    Value netdev   = *it;
    int queue_id   = 0;

    get_mac_address(dev_name.asCString(), dev_mac.ether_addr_octet);

    Value queues_info = netdev["rx-tx-queues-info"];
    for (auto rx_tx_queue : queues_info) {
      string key                 = dev_name.asString() + ":" + to_string(queue_id);
      lports[key]["description"] = "fwd thread";
      lports[key]["pmd"]         = netdev["pmd"];
      lports[key]["umem"]        = netdev["umem"];
      if (netdev.isMember("busy_poll"))
        lports[key]["busy_poll"] = netdev["busy_poll"].asBool();
      if (netdev.isMember("busy_budget"))
        lports[key]["busy_budget"] = netdev["busy_budget"].asInt();
      if (netdev.isMember("skb_mode"))
        lports[key]["skb_mode"] = netdev["skb_mode"].asBool();
      if (netdev.isMember("xsk_pin_path"))
        lports[key]["xsk_pin_path"] = netdev["xsk_pin_path"].asString();

      lports[key]["region"] = rx_tx_queue["region-index"].asInt();
      lports[key]["qid"]    = rx_tx_queue["queue-id"].asInt();
      queue_id++;
      cndp_lport_mac.push_back(dev_mac);
    }
  }
  return lports;
}

Value get_default_template(Value configuration) {
  Value defaults;
  defaults["bufcnt"] = 16;
  defaults["bufsz"]  = 2;
  defaults["rxdesc"] = 2;
  defaults["txdesc"] = 2;
  defaults["cache"]  = 256;
  defaults["mtype"]  = "2MB";

  return defaults;
}

Value get_umem_template(Value configuration) { return configuration["umems"]; }

Value get_lcore_groups_template(Value configuration) {
  Value lport_groups;
  lport_groups["initial"] = Json::arrayValue;
  lport_groups["initial"].append(configuration["main-lcore"]);

  Value netdevs = configuration["lports"];
  for (auto it = netdevs.begin(); it != netdevs.end(); it++) {
    Value dev_name    = it.key();
    Value lport       = *it;
    Value queues_info = lport["rx-tx-queues-info"];

    for (auto rx_tx_queue = queues_info.begin(); rx_tx_queue != queues_info.end(); rx_tx_queue++) {
      string key        = "group" + (*rx_tx_queue)["core-id"].asString();
      lport_groups[key] = Json::arrayValue;
      lport_groups[key].append((*rx_tx_queue)["core-id"].asInt());
    }
  }
  return lport_groups;
}

Value get_options_template(Value configuration) {
  Value options;
  options["pkt_api"]    = "xskdev";
  options["no-metrics"] = true;
  options["no-restapi"] = true;
  options["cli"]        = false;
  return options;
}

String get_cndp_configuration(Value configuration) {
  Value cndp_configuration;
  cndp_configuration["threads"]      = get_thread_template(configuration);
  cndp_configuration["lcore-groups"] = get_lcore_groups_template(configuration);
  cndp_configuration["default"]      = get_default_template(configuration);
  cndp_configuration["lports"]       = get_lport_template(configuration);
  cndp_configuration["umems"]        = get_umem_template(configuration);
  cndp_configuration["options"]      = get_options_template(configuration);

  return cndp_configuration.toStyledString();
}

static __cne_always_inline int __rx_burst(pkt_api_t api, struct fwd_port* pd, pktmbuf_t** mbufs, int n_pkts) {
  switch (api) {
    case XSKDEV_PKT_API: return xskdev_rx_burst(pd->xsk, (void**)mbufs, n_pkts);
    case PKTDEV_PKT_API: return pktdev_rx_burst(pd->lport, mbufs, n_pkts);
    default: break;
  }
  return 0;
}

static __cne_always_inline int __tx_burst(pkt_api_t api, struct fwd_port* pd, pktmbuf_t** mbufs, int n_pkts) {
  switch (api) {
    case XSKDEV_PKT_API: return xskdev_tx_burst(pd->xsk, (void**)mbufs, n_pkts);
    case PKTDEV_PKT_API: return pktdev_tx_burst(pd->lport, mbufs, n_pkts);
    default: break;
  }
  return 0;
}

static __cne_always_inline uint16_t __tx_flush(struct fwd_port* pd, pkt_api_t api, pktmbuf_t** mbufs, uint16_t n_pkts) {
  while (n_pkts > 0) {
    uint16_t n = __tx_burst(api, pd, mbufs, n_pkts);
    if (n == PKTDEV_ADMIN_STATE_DOWN)
      return n;

    n_pkts -= n;
    mbufs += n;
  }

  return n_pkts;
}

static void destroy_per_thread_txbuff(jcfg_thd_t* thd, struct fwd_info* fwd) {
  if (thd->priv_) {
    struct create_txbuff_thd_priv_t* thd_private = (struct create_txbuff_thd_priv_t*)thd->priv_;
    txbuff_t** txbuffs                           = thd_private->txbuffs;
    int i;

    for (i = 0; i < jcfg_num_lports(fwd->jinfo); i++) {
      if (txbuffs[i])
        txbuff_free(txbuffs[i]);
      txbuffs[i] = NULL;
    }
    free(thd_private->txbuffs);
    thd_private->txbuffs = NULL;
    free(thd->priv_);
    thd->priv_ = NULL;
  }
}

static int _create_txbuff(jcfg_info_t* jinfo __cne_unused, void* obj, void* arg, int idx) {
  jcfg_lport_t* lport                          = (jcfg_lport_t*)obj;
  struct create_txbuff_thd_priv_t* thd_private = (struct create_txbuff_thd_priv_t*)arg;
  txbuff_t** txbuffs                           = thd_private->txbuffs;
  struct fwd_port* pd;

  pd = (struct fwd_port*)lport->priv_;
  if (!pd)
    CNE_ERR_RET("fwd_port passed in lport private data is NULL\n");

  pkt_api_t pkt_api = thd_private->pkt_api;
  switch (pkt_api) {
    case XSKDEV_PKT_API:
      txbuffs[idx] = txbuff_xskdev_create(fwd->burst, txbuff_count_callback, &pd->tx_overrun, pd->xsk);
      break;
    case PKTDEV_PKT_API:
      txbuffs[idx] = txbuff_pktdev_create(fwd->burst, txbuff_count_callback, &pd->tx_overrun, pd->lport);
      break;
    default: txbuffs[idx] = NULL; break;
  }
  if (!txbuffs[idx])
    CNE_ERR_RET("Failed to create txbuff for lport %d\n", idx);

  return 0;
}

static int create_per_thread_txbuff(jcfg_thd_t* thd, struct fwd_info* fwd) {
  jcfg_lport_t* lport;

  if (thd->priv_) {
    CNE_ERR("Expected thread's private data to be unused but it is %p\n", thd->priv_);
    return -1;
  }
  struct create_txbuff_thd_priv_t* thd_private;
  thd_private = (struct create_txbuff_thd_priv_t*)calloc(1, sizeof(struct create_txbuff_thd_priv_t));
  if (!thd_private) {
    CNE_ERR_RET("Failed to allocate thd_private for %d lport(s)\n", jcfg_num_lports(fwd->jinfo));
  }

  thd_private->txbuffs = (txbuff_t**)calloc(jcfg_num_lports(fwd->jinfo), sizeof(txbuff_t*));
  if (!thd_private->txbuffs) {
    free(thd_private);
    CNE_ERR_RET("Failed to allocate txbuff(s) for %d lport(s)\n", jcfg_num_lports(fwd->jinfo));
  }

  thd_private->pkt_api = fwd->pkt_api;
  thd->priv_           = thd_private;

  /* Allocate a Tx buffer for all lports, not just the receiving ones */
  if (jcfg_lport_foreach(fwd->jinfo, _create_txbuff, thd->priv_)) {
    destroy_per_thread_txbuff(thd, fwd);
    return -1;
  }

  /* Set reference for this thread's receiving lports, not all lports */
  foreach_thd_lport(thd, lport)((struct fwd_port*)lport->priv_)->thd = thd;

  return 0;
}

int send_single_packet(char* buffer, int read_len, int lpid) {

  struct thread_func_arg_t* func_arg = (struct thread_func_arg_t*)func_arg_for_thread_func[lpid];
  struct fwd_info* fwd               = func_arg->fwd;
  jcfg_thd_t* thd                    = func_arg->thd;
  jcfg_lport_t* lport                = thd->lports[0];


  struct fwd_port* pd = (struct fwd_port*)lport->priv_;
  pktmbuf_t* tx_mbufs[1];
  int n_pkts, n;


  if (!pd)
    CNE_ERR("fwd_port passed in lport private data is NULL\n");

  if (fwd->pkt_api == PKTDEV_PKT_API)
    n_pkts = pktdev_buf_alloc(pd->lport, tx_mbufs, 1);
  else {
    region_info_t* ri = &lport->umem->rinfo[lport->region_idx];
    n_pkts            = pktmbuf_alloc_bulk(ri->pool, tx_mbufs, 1);
  }

  if (n_pkts > 0) {
    pktmbuf_t* xb = tx_mbufs[0];
    uint64_t* p   = pktmbuf_mtod(xb, uint64_t*);
    memcpy(p, buffer, read_len);
    pktmbuf_data_len(xb) = read_len;
    n                    = __tx_flush(pd, fwd->pkt_api, tx_mbufs, n_pkts);
    if (unlikely(n == PKTDEV_ADMIN_STATE_DOWN))
      printf("Failure Here!!!!!!!");
    pd->tx_overrun += n;
  }
  return 0;
}

void tap_handler_cndp() {
  char buffer[2048];
  memset(buffer, 0, 2048);
  int read_len;

  while (!cndp_dataplane_manager->is_started())
    sleep(1);

  cne_register("TAP_THREAD");
  while ((read_len = read(cndp_dataplane_manager->get_tap_device_fd(), buffer, 2048)) > 0 && cndp_dataplane_manager->is_running()) {
    int ether_type = cndp_dataplane_manager->get_ether_type(buffer + PI_LEN, read_len - PI_LEN);

    if (ether_type == PTYPE_L2_ETHER_ARP) { // This must be a arp request send via all lports

      unsigned char src_mac[ETH_ALEN];
      struct cne_arp_hdr* arp_packet = (cne_arp_hdr*)(buffer + PI_LEN + ETH_HLEN);
      struct ethhdr* eth_hdr         = (struct ethhdr*)(buffer + PI_LEN);

      for (int lport = 0; lport < cndp_lport_mac.size(); lport++) {
        memcpy(src_mac, cndp_lport_mac[lport].ether_addr_octet, ETH_ALEN);
        memcpy(arp_packet->arp_data.arp_sha.ether_addr_octet, src_mac, ETH_ALEN);
        memcpy(eth_hdr->h_source, src_mac, ETH_ALEN);
        send_single_packet(buffer + PI_LEN, read_len - PI_LEN, lport);
      }
      continue;
    }

    int verdict = cndp_dataplane_manager->tap_processor(buffer + PI_LEN, read_len - PI_LEN, NULL);
    if (verdict == DROP || verdict == KERNEL)
      continue;

    send_single_packet(buffer + PI_LEN, read_len - PI_LEN, verdict);
  }
}

int run_to_completion(jcfg_lport_t* lport, struct fwd_info* fwd) {
  struct fwd_port* pd                          = (struct fwd_port*)lport->priv_;
  struct create_txbuff_thd_priv_t* thd_private = (struct create_txbuff_thd_priv_t*)pd->thd->priv_;
  txbuff_t** txbuff;
  int n_pkts;

  jcfg_data_t* data = (jcfg_data_t*)fwd->jinfo->cfg;
  jcfg_list_t* lst  = &data->lport_list;
  if (!pd)
    CNE_ERR_RET("fwd_port passed in lport private data is NULL\n");

  txbuff = thd_private->txbuffs;

  n_pkts = __rx_burst(fwd->pkt_api, pd, pd->rx_mbufs, fwd->burst);

  if (n_pkts == PKTDEV_ADMIN_STATE_DOWN)
    return -1;
  for (int i = 0; i < n_pkts; i++) {

    int ether_type = cndp_dataplane_manager->get_ether_type(
    cndp_callback_wrapper->GetDataAtOffset(pd->rx_mbufs[i], 0), cndp_callback_wrapper->GetDataLength(pd->rx_mbufs[i]));

    if (ether_type == PTYPE_L2_ETHER_ARP) {
      log_info("received a arp request/reply");
      int verdict =
      cndp_dataplane_manager->arp_handler(cndp_callback_wrapper->GetDataAtOffset(pd->rx_mbufs[i], 0), lport->lpid);
      if (verdict == KERNEL) {
        cndp_callback_wrapper->PrependData(pd->rx_mbufs[i], 4);
        if (tap_write(cndp_dataplane_manager->get_tap_device_fd(), pktmbuf_mtod(pd->rx_mbufs[i], char*),
            pktmbuf_data_len(pd->rx_mbufs[i])) < 0)
          log_error("Tap Write Failed\n");
        pktmbuf_free(pd->rx_mbufs[i]);
      } else if (verdict == DROP) {
        log_info("Dropped the arp packet");
        pktmbuf_free(pd->rx_mbufs[i]);
      } else {
        log_info("sent an arp packet");
        (void)txbuff_add(txbuff[lport->lpid], pd->rx_mbufs[i]);
      }
      continue;
    }

    if (cndp_dataplane_manager->is_fastpath_filtering_enabled()) {

      int port_number = cndp_dataplane_manager->get_packet_port_number(
      cndp_callback_wrapper->GetDataAtOffset(pd->rx_mbufs[i], 0), cndp_callback_wrapper->GetDataLength(pd->rx_mbufs[i]));

      if (!cndp_dataplane_manager->filter_packet(port_number)) {
        log_debug("This packet with port number %d is filtered", port_number);
        struct ethhdr* ethhdr = (struct ethhdr*)(pktmbuf_mtod(pd->rx_mbufs[i], char*));
        cndp_callback_wrapper->PrependData(pd->rx_mbufs[i], PI_LEN);
        memcpy(ethhdr->h_dest, cndp_dataplane_manager->get_tap_ether_addr(), ETH_ALEN);
        if (tap_write(cndp_dataplane_manager->get_tap_device_fd(), pktmbuf_mtod(pd->rx_mbufs[i], char*),
            pktmbuf_data_len(pd->rx_mbufs[i])) < 0)
          printf("Tap Write Failed\n");
        pktmbuf_free(pd->rx_mbufs[i]);
        continue;
      }
    }

    int verdict = cndp_dataplane_manager->packet_processor(
    pd->rx_mbufs[i], lport->lpid, cndp_dataplane_manager->callbackwrapper, cndp_dataplane_manager->application_data);
    switch (verdict) {
      case KERNEL: {
        cndp_callback_wrapper->PrependData(pd->rx_mbufs[i], 4);
        struct ethhdr* ethhdr = (struct ethhdr*)(pktmbuf_mtod(pd->rx_mbufs[i], char*));
        memcpy(ethhdr->h_dest, cndp_dataplane_manager->get_tap_ether_addr(), ETH_ALEN);
        if (tap_write(cndp_dataplane_manager->get_tap_device_fd(), pktmbuf_mtod(pd->rx_mbufs[i], char*),
            pktmbuf_data_len(pd->rx_mbufs[i])) < 0)
          printf("Tap Write Failed\n");
        pktmbuf_free(pd->rx_mbufs[i]);
        break;
      }
      case DROP: {
        pktmbuf_free(pd->rx_mbufs[i]);
        break;
      }

      default:
        jcfg_lport_t* dst_verdict = (jcfg_lport_t*)(lst->list[verdict]);
        (void)txbuff_add(txbuff[dst_verdict->lpid], pd->rx_mbufs[i]);
        break;
    }
  }
  int tx_pkts   = 0;
  int nb_lports = jcfg_num_lports(fwd->jinfo);
  for (int i = 0; i < nb_lports; i++) {
    jcfg_lport_t* dst = jcfg_lport_by_index(fwd->jinfo, i);

    if (!dst)
      continue;

    /* Could hang here if we can never flush the TX packets */
    while (txbuff_count(txbuff[dst->lpid]) > 0)
      tx_pkts += txbuff_flush(txbuff[dst->lpid]);
  }

  return n_pkts;
}

int master_worker(jcfg_lport_t* lport, struct fwd_info* fwd) {
  struct fwd_port* pd                          = (struct fwd_port*)lport->priv_;
  struct create_txbuff_thd_priv_t* thd_private = (struct create_txbuff_thd_priv_t*)pd->thd->priv_;
  txbuff_t** txbuff;
  int n_pkts;

  if (!pd)
    CNE_ERR_RET("fwd_port passed in lport private data is NULL\n");

  txbuff = thd_private->txbuffs;

  n_pkts = __rx_burst(fwd->pkt_api, pd, pd->rx_mbufs, fwd->burst);
  if (n_pkts == PKTDEV_ADMIN_STATE_DOWN)
    return -1;

  cne_ring_enqueue_bulk(worker_rx_rings[lport->lpid], (void**)(pd->rx_mbufs), n_pkts, NULL);

  unsigned int available = 0;
  do {
    unsigned int dq = cne_ring_dequeue_burst(worker_tx_rings[lport->lpid], (void**)pd->rx_mbufs, fwd->burst, &available);
    __tx_burst(fwd->pkt_api, pd, pd->rx_mbufs, dq);
  } while (available != 0);
  return n_pkts;
}

void worker_thread(int core) {
  cpu_set_t t;
  CPU_SET(core, &t);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &t);

  pktmbuf_t* rx_mbufs[MAX_BURST_SIZE];
  int num_worker_rings = jcfg_num_lports(fwd->jinfo);
  while (1) {
    for (int lpid = 0; lpid < num_worker_rings; lpid++) {
      int dq = cne_ring_dequeue_bulk(worker_rx_rings[lpid], (void**)rx_mbufs, fwd->burst, NULL);
      for (int i = 0; i < dq; i++) {
        int ether_type = cndp_dataplane_manager->get_ether_type(
        cndp_callback_wrapper->GetDataAtOffset(rx_mbufs[i], 0), cndp_callback_wrapper->GetDataLength(rx_mbufs[i]));

        if (ether_type == PTYPE_L2_ETHER_ARP) {
          int verdict = cndp_dataplane_manager->arp_handler(cndp_callback_wrapper->GetDataAtOffset(rx_mbufs[i], 0), lpid);
          if (verdict != DROP)
            cne_ring_enqueue_bulk(worker_tx_rings[lpid], (void**)(rx_mbufs), 1, NULL);
          continue;
        }

        int port_number = cndp_dataplane_manager->get_packet_port_number(
        cndp_callback_wrapper->GetDataAtOffset(rx_mbufs[i], 0), cndp_callback_wrapper->GetDataLength(rx_mbufs[i]));

        if (cndp_dataplane_manager->filter_packet(port_number))
          continue;

        int verdict = cndp_dataplane_manager->packet_processor(
        rx_mbufs[i], lpid, cndp_dataplane_manager->callbackwrapper, cndp_dataplane_manager->application_data);
        switch (verdict) {
          case KERNEL: {
            struct ethhdr* ethhdr = (struct ethhdr*)(pktmbuf_mtod(rx_mbufs[i], char*));
            memcpy(ethhdr->h_dest, cndp_dataplane_manager->get_tap_ether_addr(), ETH_ALEN);
            if (tap_write(cndp_dataplane_manager->get_tap_device_fd(), pktmbuf_mtod(rx_mbufs[i], char*),
                pktmbuf_data_len(rx_mbufs[i])) < 0)
              printf("Tap Write Failed\n");
            pktmbuf_free(rx_mbufs[i]);
            break;
          }
          case DROP: pktmbuf_free(rx_mbufs[i]); break;
          default: cne_ring_enqueue_bulk(worker_tx_rings[verdict], (void**)(rx_mbufs), 1, NULL); break;
        }
      }
    }
  }
}

void rx_tx_thread(void* arg) {
  struct thread_func_arg_t* func_arg = (struct thread_func_arg_t*)arg;
  struct fwd_info* fwd               = func_arg->fwd;
  jcfg_thd_t* thd                    = func_arg->thd;

  jcfg_lport_t* lport;
  idlemgr_t* imgr = NULL;
  struct {
    int (*func)(jcfg_lport_t* lport, struct fwd_info* fwd);
  } models[]      = {{run_to_completion}, {master_worker}};
  int model_index = 0;

  if (thd->group->lcore_cnt > 0)
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &thd->group->lcore_bitmap);
  thd->tid = gettid();

  /* Wait for main thread to initialize */
  if (pthread_barrier_wait(&fwd->barrier) > 0)
    CNE_ERR_GOTO(leave, "Failed to wait on barrier\n");

  if (!thd->lport_cnt)
    goto leave_no_lport;

  if (create_per_thread_txbuff(thd, fwd))
    cne_exit("Failed to create txbuff(s) for \"%s\" thread\n", thd->name);

  cne_printf("   [green]Forwarding Thread ID [orange]%d [green]on lcore "
             "[orange]%d[]\n",
  thd->tid, cne_lcore_id());

  if (thd->idle_timeout) {
    struct fwd_port* pd;
    struct pktdev_info info;
    int fd = -1;

    cne_printf("   [green]Create idlemgr for thread [orange]%s [green]idle/intr "
               "timeout [orange]%d[]/[orange]%d [green]ms[]\n",
    thd->name, thd->idle_timeout, thd->intr_timeout);
    imgr = idlemgr_create(thd->name, thd->lport_cnt, thd->idle_timeout, thd->intr_timeout);
    if (!imgr)
      CNE_ERR_GOTO(leave, "failed to create idle managed\n");

    foreach_thd_lport(thd, lport) {
      switch (fwd->pkt_api) {
        case XSKDEV_PKT_API:
          pd = (fwd_port*)lport->priv_;
          if (xskdev_get_fd(pd->xsk, &fd, NULL) < 0)
            CNE_ERR_GOTO(leave, "failed to get file descriptors for %s\n", lport->name);
          break;
        case PKTDEV_PKT_API:
          memset(&info, 0, sizeof(info));
          if (pktdev_info_get(lport->lpid, &info) < 0)
            CNE_ERR_GOTO(leave, "failed to get info for %s\n", lport->name);
          fd = info.rx_fd;
          break;
        default: break;
      }
      if (fd == -1) /* account for PMDs that are not based on file descriptors */
        continue;
      if (idlemgr_add(imgr, fd, 0) < 0)
        goto leave;
    }
  }

  if (cndp_dataplane_manager->get_model() == MODEL_VALUE_MASTER_WORKER) {
    model_index = 1;
  }
  for (;;) {
    foreach_thd_lport(thd, lport) {
      int n_pkts;

      if (thd->quit == THD_QUIT) /* Make sure we check quit often to break out ASAP */
        goto leave;
      if (thd->pause) {
        usleep(1000); // sleep for 1ms
        continue;
      }

      if ((n_pkts = models[model_index].func(lport, fwd)) < 0)
        goto leave;

      if (thd->idle_timeout) {
        if (idlemgr_process(imgr, n_pkts) < 0)
          CNE_ERR_GOTO(leave, "idlemgr_process failed\n");
      }
    }
  }

leave:

  destroy_per_thread_txbuff(thd, fwd);
leave_no_lport:
  if (imgr)
    idlemgr_destroy(imgr);

  while (thd->quit != THD_QUIT)
    usleep(1000);
  free(func_arg);

  /* There is a race between threads exiting and the destruction of thread
   * resources. Avoid this by notifying the cleanup signal handler that this
   * thread is done.
   */
  thd->quit = THD_DONE;
}

int setup_worker_threads(Value worker_configuration) {
  vector<thread> workers;
  vector<int> worker_cores;
  unsigned int ring_size = worker_configuration["worker_ring_size"].asInt64();

  for (int i = 0; i < jcfg_num_lports(fwd->jinfo); i++) {
    worker_rx_rings[i] = cne_ring_create("worker_rx_ring", sizeof(void*), ring_size, 0);
    worker_tx_rings[i] = cne_ring_create("worker_tx_ring", sizeof(void*), ring_size, 0);
  }

  if (!worker_configuration["worker-cores"].isArray()) {
    log_error("Inavlid Configuration: worker-cores should be an array\n");
  }
  auto it = worker_configuration["worker-cores"].begin();
  while (it != worker_configuration["worker-cores"].end()) {
    if ((*it).isInt()) {
      worker_cores.push_back((*it).asUInt());
    } else if ((*it).isString()) {
      string cores = (*it).asString();
      int ind      = cores.find_first_of('-');
      int start    = stoi(string(cores.begin(), cores.begin() + ind));
      int end      = stoi(string(cores.begin() + ind + 1, cores.end()));

      for (int i = start; i <= end; i++)
        worker_cores.push_back(i);
    }
    it++;
  }
  workers.resize(worker_cores.size());
  for (int i = 0; i < worker_cores.size(); i++)
    workers[i] = thread(worker_thread, worker_cores[i]);
  return 0;
}

int setup_rss(Value rss_configuration) {
  for (auto it = rss_configuration.begin(); it != rss_configuration.end(); it++) {
    String dev_name = it.key().asString();
    Value rss_conf  = (*it);
    string command1 = "ethtool -L " + dev_name + " combined " + rss_conf["num-dev-queues-combined"].asString();
    system(command1.c_str());
    string command2 =
    "ethtool -X " + dev_name + " start " + rss_conf["rss-start"].asString() + " equal " + rss_conf["rss-equal"].asString();
    system(command2.c_str());
  }
  return 0;
}

int DataPlaneManager::start_cndp_datapath() {
  cndp_dataplane_manager = this;
  cndp_callback_wrapper  = new CndpCallBackWrapper();
  if (datapath_configuration.isMember("rss-configuration")) {
    setup_rss(datapath_configuration["rss-configuration"]);
  }

  thread tap_handler_thread;
  string cndp_config_string = get_cndp_configuration(datapath_configuration);
  cout << cndp_config_string << endl;
  lport_mac = cndp_lport_mac;

  memset(&fwd_info, 0, sizeof(struct fwd_info));
  if (cne_init() || parse_args(cndp_config_string, datapath_configuration["burst"].asInt(), fwd, func_arg_for_thread_func))
    goto err;

  if (MODEL == MODEL_VALUE_MASTER_WORKER) {
    setup_worker_threads(worker_configuration);
  }

  if (pthread_barrier_wait(&fwd->barrier) > 0) {
    log_error("Failed to wait for barrier\n");
    goto err;
  };

  if (create_tap) {
    tap_handler_thread = thread(tap_handler_cndp);
  }

  char c;
  printf("Press enter to exit\n");
  while (is_running()) {
    if (read(0, &c, 1) >= 0)
      break;

    sleep(1);
  }

err:
  return 0;
}
