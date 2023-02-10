# filenames = [
#     'm5out/=dbicache_lru_k_4_1_mil.txt',
#     'm5out/=dbicache_lru_k_4_2_mil.txt',
#     'm5out/=dbicache_lru_k_4_3_mil.txt',
#     'm5out/=dbicache_lru_k_4_4_mil.txt',
#     'm5out/=dbicache_lru_k_4_5_mil.txt'
# ]

# filenames = [
#     'm5out/=dbicache_lru_k_8_1_mil.txt',
#     'm5out/=dbicache_lru_k_8_2_mil.txt',
#     'm5out/=dbicache_lru_k_8_3_mil.txt',
#     'm5out/=dbicache_lru_k_8_4_mil.txt',
#     'm5out/=dbicache_lru_k_8_5_mil.txt'
# ]


# filenames = [
#     'm5out/=dbicache_lru_k_16_1_mil.txt',
#     'm5out/=dbicache_lru_k_16_2_mil.txt',
#     'm5out/=dbicache_lru_k_16_3_mil.txt',
#     'm5out/=dbicache_lru_k_16_4_mil.txt',
#     'm5out/=dbicache_lru_k_16_5_mil.txt'
# ]


# filenames = [
#     'm5out/=dbicache_rp_k_4_1_mil.txt',
#     'm5out/=dbicache_rp_k_4_2_mil.txt',
#     'm5out/=dbicache_rp_k_4_3_mil.txt',
#     'm5out/=dbicache_rp_k_4_4_mil.txt',
#     'm5out/=dbicache_rp_k_4_5_mil.txt'
# ]

# filenames = [
#     'm5out/=dbicache_rp_k_8_1_mil.txt',
#     'm5out/=dbicache_rp_k_8_2_mil.txt',
#     'm5out/=dbicache_rp_k_8_3_mil.txt',
#     'm5out/=dbicache_rp_k_8_4_mil.txt',
#     'm5out/=dbicache_rp_k_8_5_mil.txt'
# ]

# filenames = [
#     'm5out/=dbicache_rp_k_16_1_mil.txt',
#     'm5out/=dbicache_rp_k_16_2_mil.txt',
#     'm5out/=dbicache_rp_k_16_3_mil.txt',
#     'm5out/=dbicache_rp_k_16_4_mil.txt',
#     'm5out/=dbicache_rp_k_16_5_mil.txt'
# ]

# filenames = [    'm5out/=cache_rp_k_4_1_mil.txt',   
#              'm5out/=cache_rp_k_4_2_mil.txt',  
#              'm5out/=cache_rp_k_4_3_mil.txt',   
#              'm5out/=cache_rp_k_4_4_mil.txt',   
#              'm5out/=cache_rp_k_4_5_mil.txt']

# filenames = [    'm5out/=cache_rp_k_8_1_mil.txt',   
#              'm5out/=cache_rp_k_8_2_mil.txt',  
#              'm5out/=cache_rp_k_8_3_mil.txt',   
#              'm5out/=cache_rp_k_8_4_mil.txt',   
#              'm5out/=cache_rp_k_8_5_mil.txt']
# filenames = [    'm5out/=cache_rp_k_16_1_mil.txt',   
#              'm5out/=cache_rp_k_16_2_mil.txt',  
#              'm5out/=cache_rp_k_16_3_mil.txt',   
#              'm5out/=cache_rp_k_16_4_mil.txt',   
#              'm5out/=cache_rp_k_16_5_mil.txt']

# filenames = [    'm5out/=cache_lru_k_16_1_mil.txt',   
#              'm5out/=cache_lru_k_16_2_mil.txt',  
#              'm5out/=cache_lru_k_16_3_mil.txt',   
#              'm5out/=cache_lru_k_16_4_mil.txt',   
#              'm5out/=cache_lru_k_16_5_mil.txt']


# filenames = [    'm5out/=cache_lru_k_8_1_mil.txt',   
#              'm5out/=cache_lru_k_8_2_mil.txt',  
#              'm5out/=cache_lru_k_8_3_mil.txt',   
#              'm5out/=cache_lru_k_8_4_mil.txt',   
#              'm5out/=cache_lru_k_8_5_mil.txt']

filenames = [    'm5out/=cache_lru_k_4_1_mil.txt',   
             'm5out/=cache_lru_k_4_2_mil.txt',  
             'm5out/=cache_lru_k_4_3_mil.txt',   
             'm5out/=cache_lru_k_4_4_mil.txt',   
             'm5out/=cache_lru_k_4_5_mil.txt']





# for filename in filenames:
#     with open(filename) as f:
#         for line in f:
#             if "simSeconds" in line:
#                 value = float(line.split()[1])
#                 print(value)

# for filename in filenames:
#     with open(filename) as f:
#         for line in f:
#             if "writeRowHitRate" in line:
#                 value = float(line.split()[1])
#                 print(value)

# for filename in filenames:
#     with open(filename) as f:
#         for line in f:
#             if "writeRowHits" in line:
#                 value = float(line.split()[1])
#                 print(value)
                
for filename in filenames:
    with open(filename) as f:
        for line in f:
            if "system.mem_ctrl.dram.writeBursts" in line:
                value = float(line.split()[1])
                print(value)