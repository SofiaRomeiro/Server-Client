## Documentation for test cases

### Test 1 - client_server_simple_test.c 
- Author : Teachers

### Test 2 - 2_clients_same_protocol.c
- Author : @ me
- Objective : Forks the main process into two, testing the same protocol of requests
- Args : 2 client paths and 1 server path

### Test 3 - 2_clients_different_protocol.c 
- Author : @ me
- Objective : Forks the main process into two, testing the different protocols of requests
    - 1st protocol : mount, open, close, open, write, close, unmount
    - 2nd protocol : mount, open, write, close, open read, close, unmount
- Args : 2 client paths and 1 server path

### Test 4 - 5_clients_different_protocol.c 
- Author : @ me
- Objective : Forks the main process into two, testing the different protocols of requests
    - 1st protocol : mount, open, close, open, write, close, unmount
    - 2nd protocol : mount, open, close, unmount
    - 3rd protocol : mount, open, write, close, unmount
    - 4th protocol : mount, open, write, close, open, read, close, unmount
    - 5th protocol : mount, open, write, close, unmount, mount, open, close, unmount
- Args : 5 client paths and 1 server path
    