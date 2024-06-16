#include <bsd/string.h>  // for strlcpy
#include <cndp.h>
#include <cndp.h>        // for fwd_info, fwd, app_options, get_app_mode
#include <cne_common.h>  // for MEMPOOL_CACHE_MAX_SIZE, __cne_unused
#include <cne_log.h>     // for CNE_LOG_ERR, CNE_ERR_RET, CNE_ERR
#include <cne_lport.h>   // for lport_cfg
#include <cne_mmap.h>    // for mmap_addr, mmap_alloc, mmap_size, mmap_t
#include <cne_strings.h>
#include <cne_thread.h>  // for thread_create
#include <errno.h>       // for strncmp
#include <getopt.h>
#include <getopt.h>  // for getopt_long, option
#include <jcfg.h>
#include <jcfg.h>          // for jcfg_obj_t, jcfg_umem_t, jcfg_opt_t
#include <jcfg_process.h>  // for jcfg_process
#include <jsoncpp/json/json.h>
#include <pthread.h>  // for pthread_self, pthread_setaffinity_np
#include <sched.h>    // for cpu_set_t
#include <stdint.h>   // for uint64_t, uint32_t
#include <stdio.h>    // for NULL, printf, EOF
#include <stdlib.h>   // for free, calloc
#include <string.h>   // for strncmp
#include <strings.h>  // for strcasecmp

#include <iostream>

#define foreach_thd_lport(_t, _lp) \
  for (int _i = 0; _i < _t->lport_cnt && (_lp = _t->lports[_i]); _i++, _lp = _t->lports[_i])

void *func_arg_store[32];

static int process_callback(jcfg_info_t *j __cne_unused, void *_obj, void *arg, int idx)
{
  jcfg_obj_t obj;
  struct fwd_info *f = (struct fwd_info *)arg;
  uint32_t cache_sz;
  uint32_t total_region_cnt;
  char *umem_addr;
  size_t nlen;
  jcfg_lport_t *lport;

  if (!_obj) return -1;

  obj.hdr = (jcfg_hdr_t *)_obj;

  nlen = strnlen(obj.opt->name, MAX_STRLEN_SIZE);

  switch (obj.hdr->cbtype) {
    case JCFG_APPLICATION_TYPE:
      break;

    case JCFG_DEFAULT_TYPE:
      break;

    case JCFG_OPTION_TYPE:
      if (!strncmp(obj.opt->name, PKT_API_TAG, nlen)) {
        if (obj.opt->val.type == STRING_OPT_TYPE) {
          f->opts.pkt_api = obj.opt->val.str;
          if (f->pkt_api == UNKNOWN_PKT_API) f->pkt_api = (pkt_api_t)get_pkt_api(f->opts.pkt_api);
        }
      }
      else if (!strncmp(obj.opt->name, NO_METRICS_TAG, nlen)) {
        if (obj.opt->val.type == BOOLEAN_OPT_TYPE) f->opts.no_metrics = obj.opt->val.boolean;
      }
      else if (!strncmp(obj.opt->name, NO_RESTAPI_TAG, nlen)) {
        if (obj.opt->val.type == BOOLEAN_OPT_TYPE) f->opts.no_restapi = obj.opt->val.boolean;
      }
      else if (!strncmp(obj.opt->name, ENABLE_CLI_TAG, nlen)) {
        if (obj.opt->val.type == BOOLEAN_OPT_TYPE) f->opts.cli = obj.opt->val.boolean;
      }
      else if (!strncmp(obj.opt->name, UDS_PATH_TAG, nlen)) {
        if (obj.opt->val.type == STRING_OPT_TYPE) {
          f->xdp_uds = udsc_handshake(obj.opt->val.str);
          if (f->xdp_uds == NULL) CNE_ERR_RET("UDS handshake failed %s\n", strerror(errno));
        }
      }
      break;

    case JCFG_UMEM_TYPE:
      /* Default to xskdev API if not set */
      if (f->pkt_api == UNKNOWN_PKT_API) {
        f->pkt_api = XSKDEV_PKT_API;
        cne_printf("[yellow]**** [magenta]API type defaulting to use [cyan]%s [magenta]APIs[]\n",
                   XSKDEV_API_NAME);
      }

      total_region_cnt = 0;
      for (int i = 0; i < obj.umem->region_cnt; i++) {
        region_info_t *ri = &obj.umem->rinfo[i];

        total_region_cnt += ri->bufcnt;
      }
      if (total_region_cnt != obj.umem->bufcnt)
        CNE_ERR_RET("Total region bufcnt %d does not match UMEM bufcnt %d\n",
                    total_region_cnt / 1024, obj.umem->bufcnt / 1024);

      /* The UMEM object describes the total size of the UMEM space */
      obj.umem->mm = mmap_alloc(obj.umem->bufcnt, obj.umem->bufsz, (mmap_type_t)obj.umem->mtype);
      if (obj.umem->mm == NULL)
        CNE_ERR_RET("**** Failed to allocate mmap memory %ld\n",
                    (uint64_t)obj.umem->bufcnt * (uint64_t)obj.umem->bufsz);

      if (jcfg_default_get_u32(j, "cache", &cache_sz)) cache_sz = MEMPOOL_CACHE_MAX_SIZE;

      umem_addr = (char *)mmap_addr(obj.umem->mm);

      /* Create the pktmbuf pool for each region defined */
      for (int i = 0; i < obj.umem->region_cnt; i++) {
        pktmbuf_info_t *pi;
        region_info_t *ri = &obj.umem->rinfo[i];
        char name[PKTMBUF_INFO_NAME_SZ] = {0};

        /* Find the starting memory address in UMEM for the pktmbuf_t buffers */
        ri->addr = umem_addr;
        umem_addr += (ri->bufcnt * obj.umem->bufsz);

        /* Initialize a pktmbuf_info_t structure for each region in the UMEM space */
        pi = pktmbuf_pool_create(ri->addr, ri->bufcnt, obj.umem->bufsz, cache_sz, NULL);
        if (!pi) {
          CNE_ERR_RET("pktmbuf_pool_init() failed for region %d\n", i);
        }
        snprintf(name, sizeof(name), "%s-%d", obj.umem->name, i);
        pktmbuf_info_name_set(pi, name);

        ri->pool = pi;
      }
      break;

    case JCFG_LPORT_TYPE:
      do {
        lport = obj.lport;
        struct fwd_port *pd;
        mmap_t *mm;
        jcfg_umem_t *umem;
        struct lport_cfg pcfg = {0};

        umem = lport->umem;
        mm = umem->mm;

        pd = (fwd_port *)calloc(1, sizeof(struct fwd_port));
        if (!pd) CNE_ERR_RET("Failed to allocate fwd_port structure\n");

        // Init lport to -1, so in cleanup routine we can know if we need to close it.
        pd->lport = -1;

        lport->priv_ = pd;

        if (lport->flags & LPORT_SKB_MODE)
          cne_printf("[yellow]**** [green]SKB_MODE is [red]enabled[]\n");
        if (lport->flags & LPORT_BUSY_POLLING)
          cne_printf("[yellow]**** [green]BUSY_POLLING is [red]enabled[]\n");

        pcfg.qid = lport->qid;
        pcfg.bufsz = umem->bufsz;
        pcfg.rx_nb_desc = umem->rxdesc;
        pcfg.tx_nb_desc = umem->txdesc;
        pcfg.umem_addr = (char *)mmap_addr(mm);
        pcfg.umem_size = mmap_size(mm, NULL, NULL);
        pcfg.pmd_opts = lport->pmd_opts;
        pcfg.busy_timeout = lport->busy_timeout;
        pcfg.busy_budget = lport->busy_budget;
        pcfg.flags = lport->flags;
        pcfg.flags |= (umem->shared_umem == 1) ? LPORT_SHARED_UMEM : 0;

        if (lport->xsk_map_path) {
          cne_printf("[yellow]**** [green]PINNED_BPF_MAP is [red]enabled[]\n");
          pcfg.xsk_map_path = lport->xsk_map_path;
        }

        if (f->xdp_uds) {
          cne_printf("[yellow]**** [green]UDS is [red]enabled[]\n");
          pcfg.xsk_uds = f->xdp_uds;
          pcfg.flags |= LPORT_UNPRIVILEGED;
        }

        pcfg.addr = jcfg_lport_region(lport, &pcfg.bufcnt);
        if (!pcfg.addr) {
          CNE_ERR_RET("lport %s region index %d >= %d or not configured correctly\n", lport->name,
                      lport->region_idx, umem->region_cnt);
        }
        pcfg.pi = umem->rinfo[lport->region_idx].pool;

        /* Setup the mempool configuration */
        strlcpy(pcfg.pmd_name, lport->pmd_name, sizeof(pcfg.pmd_name));
        strlcpy(pcfg.ifname, lport->netdev, sizeof(pcfg.ifname));
        strlcpy(pcfg.name, lport->name, sizeof(pcfg.name));

        /* Take data from custom load program */

        switch (f->pkt_api) {
          case XSKDEV_PKT_API:
            pd->xsk = xskdev_socket_create(&pcfg);
            if (pd->xsk == NULL) {
              CNE_ERR_RET("xskdev_port_setup(%s) failed\n", lport->name);
            }
            break;
          case PKTDEV_PKT_API:
            pd->lport = pktdev_port_setup(&pcfg);
            if (pd->lport < 0) {
              CNE_ERR_RET("pktdev_port_setup(%s) failed\n", lport->name);
            }
            break;
          default:
            CNE_ERR_RET("lport %s API not supported %d\n", lport->name, f->pkt_api);
        }
      } while ((0));
      break;

    case JCFG_LGROUP_TYPE:
      break;

    case JCFG_THREAD_TYPE:
      if (!strcasecmp("main", obj.thd->thread_type)) { /* Main thread */
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &obj.thd->group->lcore_bitmap);
      }
      else if (!strcasecmp("fwd", obj.thd->thread_type)) {
        struct thread_func_arg_t *func_arg;
        // Allocate memory for func_arg. This will be freed in thread_func.
        func_arg = (thread_func_arg_t *)calloc(1, sizeof(struct thread_func_arg_t));
        if (!func_arg) CNE_ERR_RET("Allocation of struct thread_func_arg_t failed\n");
        func_arg->fwd = f;
        func_arg->thd = obj.thd;
        func_arg_store[func_arg->thd->idx] = (void *)func_arg;

        if (thread_create(obj.thd->name, rx_tx_thread, func_arg) < 0) {
          free(func_arg);
          CNE_ERR_RET("Failed to create thread %d (%s) or type %s\n", idx, obj.thd->name,
                      obj.thd->thread_type);
        }
      }
      else
        CNE_ERR_RET("*** Unknown thread type '%s'\n", obj.thd->thread_type);
      break;

    case JCFG_USER_TYPE:
      break;

    case JCFG_LPORT_GROUP_TYPE:
      break;

    default:
      return -1;
  }

  return 0;
}

/* Long options start at 256 to distinguish from short options */
#define OPT_NO_COLOR "no-color"
#define OPT_NO_COLOR_NUM 256

static void print_usage(char *prog_name)
{
  cne_printf(
      "Usage: %s [-h] [-c json_file] [-b burst] <mode>\n"
      "  <mode>         Mode types [drop | rx-only], tx-only, [lb | loopback], fwd, "
      "acl-strict, acl-permissive\n"
      "                 or [hyperscan | hs] Hyperscan needs to be installed to work\n"
      "  -a <api>       The API type to use xskdev or pktdev APIs, default is xskdev.\n"
      "                 The -a option overrides JSON file.\n"
      "  -b <burst>     Burst size. If not present default burst size %d max %d.\n"
      "  -c <json-file> The JSON configuration file\n"
      "  -C             Wait on unix domain socket for JSON or JSON-C file\n"
      "  -d             More debug stats are displayed\n"
      "  -D             JCFG debug decoding\n"
      "  -V             JCFG information verbose\n"
      "  -P             JCFG debug parsing\n"
      "  -L [level]     Enable a logging level\n"
      "  -h             Display the help information\n"
      "  --%-12s Disable color output\n",
      prog_name, BURST_SIZE, MAX_BURST_SIZE, OPT_NO_COLOR);
}

static int _cleanup(jcfg_info_t *j __cne_unused, void *obj, void *arg, int idx __cne_unused)
{
  jcfg_thd_t *thd = (jcfg_thd_t *)obj;
  jcfg_lport_t *lport;
  struct fwd_info *fwd = (struct fwd_info *)arg;

  foreach_thd_lport(thd, lport)
  {
    if (lport->umem) {
      for (int i = 0; i < lport->umem->region_cnt; i++) {
        pktmbuf_destroy(lport->umem->rinfo[i].pool);
        lport->umem->rinfo[i].pool = NULL;
      }
      mmap_free(lport->umem->mm);
      lport->umem->mm = NULL; /* Make sure we do not free this again */
    }

    struct fwd_port *pd = (struct fwd_port *)lport->priv_;

    if (!pd) continue;

    switch (fwd->pkt_api) {
      case XSKDEV_PKT_API:
        if (pd->xsk) {
          xskdev_socket_destroy(pd->xsk);
          pd->xsk = NULL;
        }
        break;
      case PKTDEV_PKT_API:
        if (pd->lport >= 0) {
          pktdev_close(pd->lport);
          pd->lport = -1;
        }
        break;
      default:
        break;
    }

    free(pd);
    lport->priv_ = NULL;
  }

  if (fwd->xdp_uds) {
    udsc_close(fwd->xdp_uds);
    fwd->xdp_uds = NULL;
  }

  return 0;
}

int parse_args(string configuration, int burst, struct fwd_info *fwd, void **func_arr_array)
{
  // clang-format off
    struct option lgopts[] = {
        {OPT_NO_COLOR, no_argument, NULL, OPT_NO_COLOR_NUM},
        {NULL, 0, 0, 0}
    };
  // clang-format on
  int opt, option_index, flags = 0;
  char log_level[16] = {0};

  fwd->pkt_api = UNKNOWN_PKT_API;

  // Set default burst size to BURST_SIZE.
  fwd->burst = burst;
  flags |= JCFG_PARSE_STRING;
  
  fwd->jinfo = jcfg_parser(flags, configuration.c_str());
  if (fwd->jinfo == NULL) CNE_ERR_RET("*** Did not find any configuration to use ***\n");

  /* create a thread barrier to wait on all threads init */
  if (pthread_barrier_init(&fwd->barrier, NULL, jcfg_num_threads(fwd->jinfo)))
    CNE_ERR_RET("*** Failed to initialize pthread barrier ***\n");
  fwd->barrier_inited = true;

  if (jcfg_process(fwd->jinfo, flags, process_callback, fwd)) {
    jcfg_thread_foreach(fwd->jinfo, _cleanup, fwd);
    CNE_ERR_RET("*** Invalid configuration ***\n");
  }
  memcpy(func_arr_array, func_arg_store, sizeof(void *));

  if (!fwd->opts.no_metrics) {
    int ret = enable_metrics(fwd);
    if (ret == 0) {
      ret = enable_uds_info(fwd);
      if (ret) metrics_destroy();
    }
    if (ret) CNE_ERR_RET("*** Failed to start metrics support ***\n");
  }

  return 0;
}
