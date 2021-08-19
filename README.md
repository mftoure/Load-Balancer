# Load-Balancer


This project aims to to implement a program scheduling on a subset of machines in the network. This scheduling will be achieved by a software layer, the load balancer, which will sit on top of the operating system of each machine.

### Architecture of the network:
Here we have a P2P network. Each channel is bidirectional.

### Commands:
```
gstart prog arguments
```

Create a process running "prog arguments" on the least busy machine on the system. This process will be assigned a unique global identifier on the network (gpid).



```
gps [-l]
```

Displays a list of all processes that have been started.



```
gkill -sig gpid
```

Sends the sig signal to the process identified by gpid.



