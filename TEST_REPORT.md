# NICKEL Compliance Tests

## eth-test
TODO when 2 nodes connected to switch

## IB-BW (2019/09/17)
```
[root@nickel3 ~]# ib_send_bw -d mlx5_0 -i 1 -D 10 -F --report_gbits
[root@nickel2 ~]# ib_send_bw -d mlx5_0 -i 1 -D 10 -F --report_gbits 10.134.244.5
---------------------------------------------------------------------------------------
Send BW Test
Dual-port : OFF Device : mlx5_0
Number of qps : 1 Transport type : IB
Connection type : RC Using SRQ : OFF
TX depth : 128
CQ Moderation : 100
Mtu : 4096[B]
Link type : IB
Max inline data : 0[B]
rdma_cm QPs : OFF
Data ex. method : Ethernet
---------------------------------------------------------------------------------------
local address: LID 0x04 QPN 0x0093 PSN 0x8dba58
remote address: LID 0x06 QPN 0x0091 PSN 0xf2a8f6
---------------------------------------------------------------------------------------
#bytes #iterations BW peak[Gb/sec] BW average[Gb/sec] MsgRate[Mpps]
65536  1111600        0.00            97.36               0.185708
---------------------------------------------------------------------------------------
```

## mem-test (2019/06/26)

```
mem-test: OK (The measured speed must be >=99.75% of the desired speed.)
----- Test results -----
Test version:    1.0
Desired speed:   702.00 Gbit/s
Measured speed:  701.82 Gbit/s (99.97% of desired)
Average late:    2.387%
```


## gpu-copy (2019/06/26)

```
gpu-copy: OK (The total write speed must be >=180 Gbit/s. The total read speed must be >=180 Gbit/s. )
----- Test results -----
Test version:      1.0
Total write speed: 196.29 Gbit/s
Total read speed:  210.25 Gbit/s
```


## disk-test

### Tests disques (sdb1) /data en raid 0 (2019/06/26)

```
disk-test: OK on empty HDD (Every disk must be capable of >=80 MB/s of write throughput, >=100 MB/s of read throughput, and >=100 MB/sec of raw read throughput.)
---------------------------------------------
Targetting /dev/sdb1, using /data/disk-test.tmp
---------------------------------------------
Flushing caches...
----- Write test...
100+0 enregistrements lus
100+0 enregistrements écrits
107374182400 octets (107 GB) copiés, 83,6633 s, 1,3 GB/s
Flushing caches...
----- Read test...
100+0 enregistrements lus
100+0 enregistrements écrits
107374182400 octets (107 GB) copiés, 77,2467 s, 1,4 GB/s
Flushing caches...
----- Raw read test...
Timing buffered disk reads: 4126 MB in 3.00 seconds = 1375.09 MB/sec
Clean up...
Done.
```


### Tests disques (sdb1) /data en raid 6 (2019/08/30)

```
---------------------------------------------
Targetting /dev/sdb1, using /data/disk-test.tmp_4
---------------------------------------------
Flushing caches...
----- Write test...
100+0 records in
100+0 records out
107374182400 bytes (107 GB) copied, 126.944 s, 846 MB/s
Flushing caches...
----- Read test...
100+0 records in
100+0 records out
107374182400 bytes (107 GB) copied, 118.71 s, 905 MB/s
Flushing caches...
----- Raw read test...
Timing buffered disk reads: 2932 MB in 3.00 seconds = 976.71 MB/sec
```
