#ifndef __CORE_CONFIG_TAGS__
#define __CORE_CONFIG_TAGS__

#define API_TAG "API"
#define API_VALUE_DPDK_TAG "dpdk"
#define API_VALUE_CNDP_TAG "cndp"
#define API_VALUE_AFPACKET_TAG "afpacket"
#define API_VALUE_DPDK 10
#define API_VALUE_CNDP 20
#define API_VALUE_AFPACKET 30

#define MODEL_TAG "MODEL"
#define MODEL_VALUE_RUN_TO_COMPLETION_TAG "run-to-completion"
#define MODEL_VALUE_MASTER_WORKER_TAG "master-worker"
#define MODEL_VALUE_RUN_TO_COMPLETION 10
#define MODEL_VALUE_MASTER_WORKER 20

#define LOG_LEVEL_TAG "LOG_LEVEL"
#define LOG_LEVEL_VALUE_DEBUG_TAG "DEBUG"
#define LOG_LEVEL_VALUE_INFO_TAG "INFO"
#define LOG_LEVEL_VALUE_WARN_TAG "WARN"
#define LOG_LEVEL_VALUE_ERROR_TAG "ERROR"

#define WORKER_CONFIGURATION_TAG "workers-configuration"
#define WORKER_CORES_TAG "worker-cores"
#define WORKER_RING_SIZE_TAG "worker-ring-size"

#define CREATE_TAP_TAG "create-tap-device"
#define TAP_DEVICE_NUM_QUEUES_TAG "tap-device-num-queues"

#define FASTPATH_PORT_CONFIGURATION_TAG "fastpath-port-configuration"
#define FASTPATH_FILTERING_TAG "fastpath-filtering"
#define FASTPATH_PORTS_TAG "fastpath-ports"

#define DPDK_DATAPATH_TAG "dpdk-datapath-params"
#define CNDP_DATAPATH_TAG "cndp-datapath-params"
#define AFPACKET_DATAPATH_TAG "afpacket-datapath-params"

#define MAIN_LCORE_TAG "main-lcore"
#define LPORTS_TAG "lports" 
#define BURST_TAG "burst"
#define RX_TX_QUEUE_INFO_TAG "rx-tx-queues-info"
#define QUEUE_ID_TAG "queue-id"
#define CORE_ID_TAG "core-id"
#define REGION_INDEX_TAG "region-index"
#define PI_LEN 4

#endif
