## How to build

```bash
$ mkdir -p ros1_ws/src && cd ros1_ws/src
$ git clone https://github.com/Barry-Xu-2018/ros1_performance_test.git
$ cd ../..
$ source /opt/ros/noetic/setup.bash
$ catkin_make -DCMAKE_BUILD_TYPE=Release
```

## How to run

### Publisher
```
$ source devel/setup.bash
$ roscore &
$ rosrun loopback_test loopback_test_talker 64K 1 10
```
- First parameter

	Supported data size: 4K, 64K, 256K, 2M, 8M

- Second parameter

	The number of subscriber. If not find enough subscriber is launched, publisher will be in waiting.

- Third parameter

	The number of test times
	
If want to exit running, you should kill this process.

### Subscriber

Open new terminal to run subscriber
```
$ source devel/setup.bash
$ rosrun loopback_test loopback_test_listener
```
Subscriber may not exist if message from publisher is lost.  
You should press ctrl+c to exit.
