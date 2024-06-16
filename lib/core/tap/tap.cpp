#include <log.h>
#include <tap.h>

int create_virtual_device(char* name, int flags, char* ether_addr, int num_queues, int& tap_fd) {
  int fd, err = -1;
  if (strlen(name) == 0) {
    log_error("Tap Device name is NULL!!");
    return err;
  }

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strcpy(ifr.ifr_name, name);
  ifr.ifr_flags = flags;

  for (int i = 0; i < num_queues; i++) {
    const char* clonedev = "/dev/net/tun";
    if ((fd = open(clonedev, O_RDWR)) < 0) {
      log_error("Error open() failed on /dev/net/tun");
      return err;
    }
    if ((err = ioctl(fd, TUNSETIFF, (void*)&ifr)) < 0) {
      log_error("Ioctl Failed errno: %d %s", errno, strerror(errno));
      close(fd);
      return err;
    }
    // tap_device_fds.push_back(fd);
    tap_fd = fd;
  }

  int sock;
  if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
    log_error("Error creating socket");
    return sock;
  }

  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);
  ifr.ifr_addr.sa_family = AF_INET;
  if ((err = ioctl(sock, SIOCGIFHWADDR, &ifr)) < 0) {
    log_error("Error getting address of TUN Device: %s", strerror(errno));
    close(sock);
    return err;
  }

  close(sock);
  memcpy(ether_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

  log_info("Successfully Create TAP Device %s", name);
  return 0;
}

int tap_write(int tap_fd, char* buf, int bufLen) {
  int wlen;
  wlen = write(tap_fd, buf, bufLen);
  if (wlen < 0) {
    return -1;
  }
  return wlen;
}

int netlink_connect() {
  int netlink_fd, rc;
  struct sockaddr_nl sockaddr;

  netlink_fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
  if (netlink_fd == -1) {
    return -1;
  }

  memset(&sockaddr, 0, sizeof sockaddr);
  sockaddr.nl_family = AF_NETLINK;
  rc                 = bind(netlink_fd, (struct sockaddr*)&sockaddr, sizeof sockaddr);
  if (rc == -1) {
    int bind_errno = errno;
    close(netlink_fd);
    errno = bind_errno;
    return -1;
  }
  return netlink_fd;
}

int netlink_set_addr_ipv4(int netlink_fd, const char* iface_name, const char* address, uint8_t network_prefix_bits) {
  struct {
    struct nlmsghdr header;
    struct ifaddrmsg content;
    char attributes_buf[64];
  } request;

  struct rtattr* request_attr;
  size_t attributes_buf_avail = sizeof request.attributes_buf;

  memset(&request, 0, sizeof request);
  request.header.nlmsg_len      = NLMSG_LENGTH(sizeof request.content);
  request.header.nlmsg_flags    = NLM_F_REQUEST | NLM_F_EXCL | NLM_F_CREATE;
  request.header.nlmsg_type     = RTM_NEWADDR;
  request.content.ifa_index     = if_nametoindex(iface_name);
  request.content.ifa_family    = AF_INET;
  request.content.ifa_prefixlen = network_prefix_bits;

  /* request.attributes[IFA_LOCAL] = address */
  request_attr           = IFA_RTA(&request.content);
  request_attr->rta_type = IFA_LOCAL;
  request_attr->rta_len  = RTA_LENGTH(sizeof(struct in_addr));
  request.header.nlmsg_len += request_attr->rta_len;
  inet_pton(AF_INET, address, RTA_DATA(request_attr));

  /* request.attributes[IFA_ADDRESS] = address */
  request_attr           = RTA_NEXT(request_attr, attributes_buf_avail);
  request_attr->rta_type = IFA_ADDRESS;
  request_attr->rta_len  = RTA_LENGTH(sizeof(struct in_addr));
  request.header.nlmsg_len += request_attr->rta_len;
  inet_pton(AF_INET, address, RTA_DATA(request_attr));

  if (send(netlink_fd, &request, request.header.nlmsg_len, 0) == -1) {
    return -1;
  }
  return 0;
}

int netlink_link_up(int netlink_fd, const char* iface_name) {
  struct {
    struct nlmsghdr header;
    struct ifinfomsg content;
  } request;

  memset(&request, 0, sizeof request);
  request.header.nlmsg_len   = NLMSG_LENGTH(sizeof request.content);
  request.header.nlmsg_flags = NLM_F_REQUEST;
  request.header.nlmsg_type  = RTM_NEWLINK;
  request.content.ifi_index  = if_nametoindex(iface_name);
  request.content.ifi_flags  = IFF_UP;
  request.content.ifi_change = 1;

  if (send(netlink_fd, &request, request.header.nlmsg_len, 0) == -1) {
    return -1;
  }
  return 0;
}
