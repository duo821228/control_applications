# control_applications

This repository includes motor control application written in C using IgH EtherCAT Master Stack. 
Since there are two synchronized methods (Frame event, Distributed Clock event) in EtherCAT, we try to support both.

Support commercial servo drives:

1.Sanyo (SANMOTION R ADVANCED MODEL, RS2A01A0KA4 (Drive) / R2AA06020FXH00 (Motor))
 - Should be operate with DC (if you don't have a jitter less than 5 us at master).
 - 0x6060 (Mode of operation) should not be mapped to EtherCAT domain.
 - Only support CSV operation mode; Supporting for CSP mode is not available..

2.Panasonic (MINAS A5 Series, MBDHT2510BD1 (Drive))
 - Drive operation mode (Mode of Operation (0x6060)) initialization is needed.

3.Yaskawa
 - Drive operation mode (Mode of Operation (0x6060)) initialization is needed.
 - This drive does not handle LRW command, we should be create two domains for RxPDO, TxPDO.

4.Higen (EDA 7002 (Drive))

5.Texas Instruments AM3359 ICE
 - Drive operation mode (Mode of Operation (0x6060)) initialization is needed.
 - PDO overlapping feature should be applied.
