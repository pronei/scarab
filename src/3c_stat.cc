#include "3c_stat.h"
#include <deque>
#include <vector>
#include <iostream>
#include <bits/stdc++.h>
#include <unordered_map>

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_DECOUPLED_FE, ##args)

std::unordered_set<Addr> seen_blocks;

Addr get_block_address(Addr addr, Addr offset_mask) {
  return addr & ~offset_mask;
}

/*
    The approach for each of the 3Cs:
    1. Compulsory - simply maintain a set of previously seen block addresses, if
   not seen before, classify as compulsory
    2. Conflict - Maintain another cache mirrored with Dcache in terms of
   access/insertions but is fully associative; If an address is found in this
   cache, classify as conflict
    3. Capacity - The rest of the misses are classified as this
*/
MissType classify_miss(Addr addr, Cache* dcache, Cache* conflict_cache) {
  Addr block_addr = get_block_address(addr, dcache->offset_mask);

  if(seen_blocks.find(block_addr) == seen_blocks.end()) {
    seen_blocks.insert(block_addr);
    return COMPULSORY_MISS;
  }

  Addr line_addr;
  bool in_conflict_cache = cache_access(conflict_cache, addr, &line_addr,
                                        TRUE) != NULL;

  if(in_conflict_cache) {
    return CONFLICT_MISS;
  } else {
    return CAPACITY_MISS;
  }
}
