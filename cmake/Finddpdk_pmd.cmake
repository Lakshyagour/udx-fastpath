set(components net_i40e net_bond net_ixgbe net_bnxt net_dpaa)
foreach(c ${components})
  find_library(DPDK_rte_${c}_LIBRARY rte_${c})
endforeach()

foreach(c ${components})
  list(APPEND check_LIBRARIES "${DPDK_rte_${c}_LIBRARY}")
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(dpdk_pmd check_LIBRARIES)

if(DPDK_PMD_FOUND)
  set(DPDK_PMD ${check_LIBRARIES})
endif(DPDK_PMD_FOUND)