#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <fcntl.h>
#include <time.h>

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <array>
#include <iomanip>

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
static int pub_count = 0;

static void show_usage(std::string name)
{
    std::cerr << "Usage: " << name << " DATA_SIZE SUBSCRIBER_NUM TEST_TIMES\n"
              << "DATA_SIZE:\n"
              << "4K, 64K, 256K, 2M, 8M"
              << std::endl;
}

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

static bool WaitSubscribers(int sub_num, bool max_check_loop_enable){
  bool ret = false;
  constexpr int max_check_loop = 100;
  int i = 0;

  // wait for all subscriber building
  while (true){
    flock(g_shm_fd, LOCK_EX);
    int need_subscriber_count = g_shm_addr->need_subscriber_count;
    flock(g_shm_fd, LOCK_UN);

    if (need_subscriber_count == 0){
      ret = true;
      break;
    }

    if (max_check_loop_enable) {
      if (i < max_check_loop) {
        usleep(10000);
        ++i;
      } else {
        break;
      }
    }
  }

  flock(g_shm_fd, LOCK_EX);
  g_shm_addr->need_subscriber_count = sub_num;
  flock(g_shm_fd, LOCK_UN);

  return ret;
}

enum MSG_SIZE {
  MSG_SIZE_4K = 0,
  MSG_SIZE_64K,
  MSG_SIZE_256K,
  MSG_SIZE_2M,
  MSG_SIZE_8M
};

class test_pub {
public:
  explicit test_pub(MSG_SIZE s, int sub_num, int test_times)
  :size_type_(s),
  sub_num_(sub_num),
  test_times_(test_times){
    // Prepare send data
    switch(size_type_) {
      case MSG_SIZE_4K:
        std::cout << "Prepare 4KB data" << std::endl;
        data_4k_ptr_ = boost::make_shared<bounded_message::TestData4k>();
        break;
      case MSG_SIZE_64K:
        std::cout << "Prepare 64KB data" << std::endl;
        data_64k_ptr_ = boost::make_shared<bounded_message::TestData64k>();
        break;
      case MSG_SIZE_256K:
        std::cout << "Prepare 256KB data" << std::endl;
        data_256k_ptr_ = boost::make_shared<bounded_message::TestData256k>();
        break;
      case MSG_SIZE_2M:
        std::cout << "Prepare 2MB data" << std::endl;
        data_2m_ptr_ = boost::make_shared<bounded_message::TestData2m>();
        break;
      case MSG_SIZE_8M:
        std::cout << "Prepare 8MB data" << std::endl;
        data_8m_ptr_ = boost::make_shared<bounded_message::TestData8m>();
        break;
      default:
        break;
    }
  }

  ~test_pub() {
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

    switch(size_type_) {
      case MSG_SIZE_4K:
        pub_ = n_.advertise<bounded_message::TestData4k>("loopback_test", 10);
        break;
      case MSG_SIZE_64K:
        pub_ = n_.advertise<bounded_message::TestData64k>("loopback_test", 10);
        break;
      case MSG_SIZE_256K:
        pub_ = n_.advertise<bounded_message::TestData256k>("loopback_test", 10);
        break;
      case MSG_SIZE_2M:
        pub_ = n_.advertise<bounded_message::TestData2m>("loopback_test", 10);
        break;
      case MSG_SIZE_8M:
        pub_ = n_.advertise<bounded_message::TestData8m>("loopback_test", 10);
        break;
      default:
        return false;
    }

    g_shm_addr->msg_size = size_list_[size_type_];
    g_shm_addr->need_subscriber_count = sub_num_;
    g_shm_addr->send_times = test_times_;

    return true;
  }

  bool run() {
    for(int i = 0; i <= test_times_; i++){
      if(i == 0){
        WaitSubscribers(sub_num_, false);
        std::cout << "All subscribers are connected !\nStart to test" << std::endl;
        usleep(3000000);
      }else{
        if (WaitSubscribers(sub_num_, true)) {
          flock(g_shm_fd, LOCK_EX);
          double duration = (g_shm_addr->ts_transmission_end.tv_sec - ts_transmission_start_.tv_sec)
                  + double(g_shm_addr->ts_transmission_end.tv_nsec - ts_transmission_start_.tv_nsec) / 1000000000;
          flock(g_shm_fd, LOCK_UN);
          transmission_durations_.push_back(duration);
          std::cout << "." << std::flush;
          ++pub_count;
        }

        if(i == test_times_){
          std::cout << "\n" << std::endl;
          if (pub_count != test_times_) {
            std::cerr << "Loss " << (test_times_ - pub_count) << " messages !" << std::endl;
          }
          break;
        }
      }

      flock(g_shm_fd, LOCK_EX);
      g_shm_addr->ts_transmission_end.tv_sec = 0;
      g_shm_addr->ts_transmission_end.tv_nsec = 0;
      flock(g_shm_fd, LOCK_UN);

      switch(size_type_) {
        case MSG_SIZE_4K:
          timespec_get(&ts_transmission_start_, TIME_UTC);
          pub_.publish(data_4k_ptr_);
          break;
        case MSG_SIZE_64K:
          timespec_get(&ts_transmission_start_, TIME_UTC);
          pub_.publish(data_64k_ptr_);
          break;
        case MSG_SIZE_256K:
          timespec_get(&ts_transmission_start_, TIME_UTC);
          pub_.publish(data_256k_ptr_);
          break;
        case MSG_SIZE_2M:
          timespec_get(&ts_transmission_start_, TIME_UTC);
          pub_.publish(data_2m_ptr_);
          break;
        case MSG_SIZE_8M:
          timespec_get(&ts_transmission_start_, TIME_UTC);
          pub_.publish(data_8m_ptr_);
          break;
        default:
          break;
      }

      ros::spinOnce();

      if (!ros::ok()) {
        std::cerr << "Break execution !" << std::endl;
        return false;
      }
    }

    return true;
  }

  void print_result() {
    if (transmission_durations_.size() != 0){
      auto result = std::minmax_element(transmission_durations_.begin(), transmission_durations_.end());
      auto sum = std::accumulate(transmission_durations_.begin(), transmission_durations_.end(), .0f);

      auto latency = sum / transmission_durations_.size();
      auto throughput = (g_shm_addr->msg_size * transmission_durations_.size()) / (sum * 1024 * 1024);

      std::cout << "buffer_size(KB) min(s) max(s) count sum(s) average(s) latency(ms) throughput(MB/s)" << std::endl;
      std::cout << std::right << std::fixed << std::setprecision(9)
          << g_shm_addr->msg_size / 1024 <<" "
          << std::fixed << std::setprecision(9) << *result.first <<" "
          << std::fixed << std::setprecision(9) << *result.second <<" "
          << std::fixed << std::setprecision(9) << transmission_durations_.size() <<" "
          << std::fixed << std::setprecision(9) << sum <<" "
          << std::fixed << std::setprecision(9) << latency << " "
          << std::fixed << std::setprecision(9) << latency * 1000 <<" "
          << std::fixed << std::setprecision(9) << throughput << std::endl;
  }
}

public:
  const long size_list_[5] = {SIZE_4KB, SIZE_64KB, SIZE_256KB, SIZE_2MB, SIZE_8MB};
  MSG_SIZE size_type_;
  int sub_num_;
  int test_times_;
  ros::NodeHandle n_;
  ros::Publisher pub_;
  bounded_message::TestData4k::ConstPtr data_4k_ptr_;
  bounded_message::TestData64k::ConstPtr data_64k_ptr_;
  bounded_message::TestData256k::ConstPtr data_256k_ptr_;
  bounded_message::TestData2m::ConstPtr data_2m_ptr_;
  bounded_message::TestData8m::ConstPtr data_8m_ptr_;
  struct timespec ts_transmission_start_;
  std::vector<double> transmission_durations_;
};


int main(int argc, char **argv)
{
  if (argc < 4) {
    show_usage("loopback_test_talker");
    return 1;
  }

  std::string data_size_str(argv[1]);
  MSG_SIZE size_type;

  if (data_size_str == "4K") {
    size_type = MSG_SIZE_4K;
  } else if (data_size_str == "64K") {
    size_type = MSG_SIZE_64K;
  } else if (data_size_str == "256K") {
    size_type = MSG_SIZE_256K;
  } else if (data_size_str == "2M") {
    size_type = MSG_SIZE_2M;
  } else if (data_size_str == "8M") {
    size_type = MSG_SIZE_8M;
  } else {
    std::cerr << "Wrong DATA_SIZE: " << data_size_str << std::endl;
    show_usage("loopback_test_talker");
    return 1;
  }

  int wait_sub_num = std::stoi(std::string(argv[2]));

  int test_times = std::stoi(std::string(argv[3]));

  std::printf(
    "Test size: %s, Wait subscription number: %d, Test times: %d\n",
    data_size_str.c_str(),
    wait_sub_num,
    test_times);

  ros::init(argc, argv, "talker");

  test_pub tp(size_type, wait_sub_num, test_times);

  if (tp.init() && tp.run()) {
    tp.print_result();
  }

  return 0;
}
