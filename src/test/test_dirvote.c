/* Copyright (c) 2020, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file test_dirvote.c
 * \brief Unit tests for dirvote related functions
 */
#define DIRVOTE_PRIVATE

#include "tinytest.h"
#include "core/or/or.h"
#include "feature/dirauth/dirvote.h"
#include "feature/nodelist/dirlist.h"
#include "feature/nodelist/node_st.h"
#include "feature/nodelist/nodelist.h"
#include "feature/nodelist/routerinfo_st.h"
#include "feature/nodelist/signed_descriptor_st.h"
#include "tinytest_macros.h"


typedef struct simple_status {
    int is_running;
    int is_auth;
    int bw_kb;
} simple_status;
simple_status first_status, second_status;

int mock_router_digest_is_trusted(const char *digest, dirinfo_type_t type) {
  (void) type;
  if (strcmp(digest, "first") == 0) {
    return first_status.is_auth;
  } else {
    return second_status.is_auth;
  }
}


const node_t *node_get_mock(const char *identity_digest) {
  node_t *node_running;
  node_running = malloc(sizeof(node_t * ));
  if (strcmp(identity_digest, "first") == 0) {
    node_running->is_running = first_status.is_running;
  } else {
    node_running->is_running = second_status.is_running;
  }
  return node_running;
}

uint32_t dirserv_bw_mock(const routerinfo_t *ri) {
  const char *digest = ri->cache_info.identity_digest;
  if (strcmp(digest, "first") == 0) {
    return first_status.bw_kb;
  } else {
    return second_status.bw_kb;
  }
}


static void
test_dirvote_compare_routerinfo_by_ip_and_bw_(void *arg) {
  MOCK(router_digest_is_trusted_dir_type, mock_router_digest_is_trusted);
  MOCK(node_get_by_id, node_get_mock);
  MOCK(dirserv_get_bandwidth_for_router_kb, dirserv_bw_mock);

  // Initialize the routerinfo
  printf("\n");
  (void) arg;
  routerinfo_t *first = malloc(sizeof(routerinfo_t));
  routerinfo_t *second = malloc(sizeof(routerinfo_t));

  // Give different IP versions
  tor_addr_t first_addr, second_addr;
  first_addr.family = AF_INET6; // IPv6
  second_addr.family = AF_INET; // IPv4
  first->ipv6_addr = first_addr;
  second->ipv6_addr = second_addr;
  int a = compare_routerinfo_by_ip_and_bw_((const void **) first, (const void **) second);
  tt_assert(a == -1);


  // Give different addresses but both are IPv6 or IPv4

  // Both have IPv4 addresses
  first->ipv6_addr.family = AF_INET;
  first->addr = 256;
  second->addr = 512;
  a = compare_routerinfo_by_ip_and_bw_((const void **) first, (const void **) second);
  tt_assert(a == -1);

  // Both have IPv6 addresses
  tor_addr_t first_ipv6, second_ipv6;
  first_ipv6.family = AF_INET6;
  int addr_size = sizeof(uint8_t) * 16;
  struct in6_addr my_address;
  memcpy(my_address.__in6_u.__u6_addr8, malloc(addr_size), addr_size);
  first_ipv6.addr.in6_addr = my_address;
  memcpy(&second_ipv6, &first_ipv6, sizeof(first_ipv6));
  for (size_t i = 0; i < 16; i++) {
    second_ipv6.addr.in6_addr.s6_addr[i] = 1;
    first_ipv6.addr.in6_addr.s6_addr[i] = 0xF;
  }
  first->ipv6_addr = first_ipv6;
  second->ipv6_addr = second_ipv6;
  a = compare_routerinfo_by_ip_and_bw_((const void **) first, (const void **) second);
  tt_assert(a == -1);

  // Give same address but different auth status
  for (size_t i = 0; i < 16; i++) {
    second_ipv6.addr.in6_addr.s6_addr[i] = 0xF;
  }
  second->ipv6_addr = second_ipv6;
  signed_descriptor_t first_cache_info, second_cache_info;
  strcpy(first_cache_info.identity_digest, "first");
  strcpy(second_cache_info.identity_digest, "second");
  first->cache_info = first_cache_info;
  second->cache_info = second_cache_info;
  first_status.is_auth = 1;
  second_status.is_auth = 0;
  a = compare_routerinfo_by_ip_and_bw_((const void **) first, (const void **) second);
  tt_assert(a == -1);

  // Give same auth status but different running status
  second_status.is_auth = 1;
  first_status.is_running = 1;
  a = compare_routerinfo_by_ip_and_bw_((const void **) first, (const void **) second);
  tt_assert(a == -1);

  // Give same running status but different bandwidth
  second_status.is_running = 1;
  first_status.bw_kb = 512;
  second_status.bw_kb = 256;
  a = compare_routerinfo_by_ip_and_bw_((const void **) first, (const void **) second);
  tt_assert(a == -1);

  // Give same bandwidth but different digest
  second_status.bw_kb = 512;
  a = compare_routerinfo_by_ip_and_bw_((const void **) first, (const void **) second);
  tt_assert(a < 0);


  end:
  UNMOCK(router_digest_is_trusted_dir_type);
  UNMOCK(node_get_by_id);
  UNMOCK(dirserv_get_bandwidth_for_router_kb);
  free(first);
  free(second);
  return;
}


#define NODE(name, flags) \
  { #name, test_dirvote_##name, (flags), NULL, NULL}

struct testcase_t dirvote_tests[] = {
        NODE(compare_routerinfo_by_ip_and_bw_, TT_FORK),
        END_OF_TESTCASES
};
