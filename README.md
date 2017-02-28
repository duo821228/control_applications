# control_applications

This repository includes motor control application written in C using IgH EtherCAT Master Stack. Since there are two synchronized methods (Frame event, Distributed Clock event) in EtherCAT, We try to support both.

Support commercial servo drives:

1.Sanyo (SANMOTION R ADVANCED MODEL, RS2A01A0KA4 (Drive) / R2AA06020FXH00 (Motor))
 - Should be operate with DC if you don't have a jitter less than 5 us at master.
 - PDO overlapping enabled.
2.Panasonic (MINAS A5 Series, MBDHT2510BD1 (Drive))
 - Mode of Operation (0x6060) initialization needed.
3.Yaskawa
 - Mode of Operation (0x6060) initialization needed.
 - Does not handle LRW command, create two domains for RxPDO, TxPDO.
4.Higen (EDA 7002 (Drive))
5.Texas Instruments AM3359 ICE
 - PDO overlapping enabled.
