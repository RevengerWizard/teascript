def fannkuch(n):
    maxFlipsCount = 0
    permSign = True
    checksum = 0

    perm1 = list(range(n))
    count = perm1[:]
    rxrange = range(2, n - 1)
    nm = n - 1
    while 1:
        k = perm1[0]
        if k:
            perm = perm1[:]
            flipsCount = 1
            kk = perm[k]
            while kk:
                perm[:k+1] = perm[k::-1]
                flipsCount += 1
                k = kk
                kk = perm[kk]
            if maxFlipsCount < flipsCount:
                maxFlipsCount = flipsCount
            checksum += flipsCount if permSign else -flipsCount

        if permSign:
            perm1[0], perm1[1] = perm1[1], perm1[0]
            permSign = False
        else:
            perm1[1], perm1[2] = perm1[2], perm1[1]
            permSign = True
            for r in rxrange:
                if count[r]:
                    break
                count[r] = r
                perm0 = perm1[0]
                perm1[:r+1] = perm1[1:r+2]
                perm1[r+1] = perm0
            else:
                r = nm
                if not count[r]:
                    return checksum, maxFlipsCount
            count[r] -= 1

n = 9
checksum, flips = fannkuch(n)
print("%i\nfannkuchen(%i) = %i" % (checksum, n, flips))