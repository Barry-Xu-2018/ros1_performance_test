#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <fcntl.h>
#include <time.h>

#include <string>
#include <thread>
#include <functional>

#include "ros/ros.h"
#include "bounded_message/TestData4k.h"
#include "bounded_message/TestData64k.h"
#include "bounded_message/TestData256k.h"
#include "bounded_message/TestData2m.h"
#include "bounded_message/TestData8m.h"

#define SIZE_4KB    4*1024
#define SIZE_64KB   64*1024
#define SIZE_256KB  256*1024
#define SIZE_2MB    2*1024*1024
#define SIZE_8MB    8*1024*1024

#define FILE_PERM 0600
#define SHM_MAP_FILE "/tmp/test_shmmapfile"

typedef struct{
  int need_subscriber_count;
  int send_times;
  long int msg_size;
  struct timespec ts_transmission_end;
}ShmData;

static int g_shm_fd = -1;
static ShmData* g_shm_addr = NULL;

static int sub_count = 0;
static int g_count = 0;

static int shm_open(void){
  // setup shm data
  g_shm_fd = open(SHM_MAP_FILE, O_RDWR | O_CREAT, FILE_PERM);
  if (g_shm_fd < 0){
    ROS_ERROR("open file %s failed", SHM_MAP_FILE);
    return EXIT_FAILURE;
  }

  int size = sizeof (ShmData);
  int sysret = fallocate(g_shm_fd, 0, 0, (off_t) size);
  if (sysret != 0){
    if (errno == ENOSYS || errno == EOPNOTSUPP){
      sysret = posix_fallocate(g_shm_fd, 0, (off_t) size);
    }
  }
  if (sysret < 0){
    ROS_ERROR("fallocate error");
    return EXIT_FAILURE;
  }
  g_shm_addr = (ShmData*) mmap(NULL, (size_t) size, PROT_WRITE | PROT_READ,
        MAP_SHARED | MAP_POPULATE, g_shm_fd, 0);
  if (g_shm_addr == NULL)  {
    ROS_ERROR("mmap error");
    return EXIT_FAILURE;
  }
  std::printf("shm_open %s [%d], share memory [0x%lx]\n", SHM_MAP_FILE, g_shm_fd, reinterpret_cast<uint64_t>(g_shm_addr));
  return EXIT_SUCCESS;
}

static void UpdataShmData(const struct timespec& ts_end){
  flock(g_shm_fd, LOCK_EX);
  g_shm_addr->ts_transmission_end.tv_sec = ts_end.tv_sec;
  g_shm_addr->ts_transmission_end.tv_nsec = ts_end.tv_nsec;
  g_shm_addr->need_subscriber_count--;
  flock(g_shm_fd, LOCK_UN);
}

class test_sub {
public:
  test_sub() {
  }

  ~ test_sub() {
    if (g_shm_fd != -1){
      close(g_shm_fd);
      g_shm_fd = -1;
      unlink(SHM_MAP_FILE);
    }
  }

  bool init() {
    if (shm_open()) {
      return false;
    }

    msg_size_ = g_shm_addr->msg_size;
    test_times_ = g_shm_addr->send_times;

    if (msg_size_ == SIZE_4KB) {
      std::cout << "Receive 4KB data" << std::endl;
      auto callback = std::bind(&test_sub::download<bounded_message::TestData4kConstPtr>, this, std::placeholders::_1);
      sub_ = n_.subscribe<bounded_message::TestData4k>("loopback_test", 10, callback, ros::VoidConstPtr(), ros::TransportHints().tcp().tcpNoDelay());
    } else if (msg_size_ == SIZE_64KB) {
      std::cout << "Receive 64KB data" << std::endl;
      auto callback = std::bind(&test_sub::download<bounded_message::TestData64kConstPtr>, this, std::placeholders::_1);
      sub_ = n_.subscribe<bounded_message::TestData64k>("loopback_test", 10, callback, ros::VoidConstPtr(), ros::TransportHints().tcp().tcpNoDelay());
    } else if (msg_size_ == SIZE_256KB) {
      std::cout << "Receive 256KB data" << std::endl;
      auto callback = std::bind(&test_sub::download<bounded_message::TestData256kConstPtr>, this, std::placeholders::_1);
      sub_ = n_.subscribe<bounded_message::TestData256k>("loopback_test", 10, callback, ros::VoidConstPtr(), ros::TransportHints().tcp().tcpNoDelay());
    } else if (msg_size_ == SIZE_2MB) {
      std::cout << "Receive 2MB data" << std::endl;
      auto callback = std::bind(&test_sub::download<bounded_message::TestData2mConstPtr>, this, std::placeholders::_1);
      sub_ = n_.subscribe<bounded_message::TestData2m>("loopback_test", 10, callback, ros::VoidConstPtr(), ros::TransportHints().tcp().tcpNoDelay());
    } else {
      std::cout << "Receive 8MB data" << std::endl;
      auto callback = std::bind(&test_sub::download<bounded_message::TestData8mConstPtr>, this, std::placeholders::_1);
      sub_ = n_.subscribe<bounded_message::TestData8m>("loopback_test", 10, callback, ros::VoidConstPtr(), ros::TransportHints().tcp().tcpNoDelay());
    }

    if(0 < g_shm_addr->need_subscriber_count){
      flock(g_shm_fd, LOCK_EX);
      g_shm_addr->need_subscriber_count--;
      flock(g_shm_fd, LOCK_UN);
    }

    return true;
  }

  void run() {
    ros::spin();
  }
private:
  long int msg_size_;
  ros::NodeHandle n_;
  ros::Subscriber sub_;
  int test_times_;

  inline void download_impl(void) {
    struct timespec _ts_transmission_end;
    timespec_get(&_ts_transmission_end, TIME_UTC);

    UpdataShmData(_ts_transmission_end);

    sub_count = ++g_count;

    /* print_log("[%s][%s] UpdataShmData [%dKB] [%d/%d] \n",
    _node_name.c_str(), _topic_name.c_str(),
    g_shm_addr->msg_size/1024,
    g_shm_addr->need_subscriber_count, sub_count);*/

    if (sub_count >= test_times_){
      ros::shutdown();
    }
  }

  template<class T>
  void download(const T& msg){
    (void)msg;
    download_impl();
  }
};

int main(int argc, char **argv)
{
  std::stringstream id;
  id << std::this_thread::get_id();

  ros::init(argc, argv, "listener_" + id.str());

  test_sub ts;

  if (ts.init()) {
    ts.run();
  }

  return 0;
}
