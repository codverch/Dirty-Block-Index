filenames = [
    'm5out/=dbicache_lru_k_4_1_mil.txt',
    'm5out/=dbicache_lru_k_4_2_mil.txt',
    'm5out/=dbicache_lru_k_4_3_mil.txt',
    'm5out/=dbicache_lru_k_4_4_mil.txt',
    'm5out/=dbicache_lru_k_4_5_mil.txt'
]


for filename in filenames:
    with open(filename) as f:
        for line in f:
            if 'simSeconds' in line:
                sim_seconds = float(line.split()[1])
                print("simSeconds in {}: {}".format(filename, sim_seconds))
                break
