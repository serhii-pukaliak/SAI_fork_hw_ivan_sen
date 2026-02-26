SAI (Switch Abstraction Interface)
==============================================

The Switch Abstraction Interface defines the API to provide a
vendor-independent way of controlling forwarding elements, such as a switching
ASIC, an NPU or a software switch in a uniform manner.


## Testing results:

```bash
$ ./unit_test_lag 
CREATE LAG: 0x2 (empty list)
CREATE LAG_MEMBER: 0x19 (#0 PORT ID val:Port type 0 #1 LAG ID val:LAG type 0 )
CREATE LAG_MEMBER: 0x100000019 (#0 PORT ID val:Port type 1 #1 LAG ID val:LAG type 0 )
CREATE LAG: 0x100000002 (empty list)
CREATE LAG_MEMBER: 0x200000019 (#0 PORT ID val:Port type 2 #1 LAG ID val:LAG type 1 )
CREATE LAG_MEMBER: 0x300000019 (#0 PORT ID val:Port type 3 #1 LAG ID val:LAG type 1 )
LAG1 PORT_LIST (count=2): 0x1 0x100000001 
LAG2 PORT_LIST (count=2): 0x200000001 0x300000001 
LAG_MEMBER#1 LAG_ID: 0x2 (expected 0x2)
LAG_MEMBER#3 PORT_ID: 0x200000001 (expected 0x200000001)
REMOVE LAG_MEMBER: 0x100000019
REMOVE LAG: 0x2 failed (still has members)
OK: remove_lag(lag1) blocked while members exist
LAG1 PORT_LIST after removing member#2 (count=1): 0x1 
REMOVE LAG_MEMBER: 0x200000019
LAG2 PORT_LIST after removing member#3 (count=1): 0x300000001 
REMOVE LAG_MEMBER: 0x19
REMOVE LAG_MEMBER: 0x300000019
REMOVE LAG: 0x100000002
REMOVE LAG: 0x2
PASS
```
