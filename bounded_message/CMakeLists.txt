cmake_minimum_required(VERSION 3.0.2)
project(bounded_message)

find_package(catkin REQUIRED COMPONENTS message_generation)

# Generate messages in the 'msg' folder
add_message_files(
	DIRECTORY msg
  FILES
  TestData4k.msg
	TestData64k.msg
	TestData256k.msg
	TestData2m.msg
	TestData8m.msg
)

## Generate added messages and services with any dependencies listed here
generate_messages()


###################################
## catkin specific configuration ##
###################################
## The catkin_package macro generates cmake config files for your package
## Declare things to be passed to dependent projects
## INCLUDE_DIRS: uncomment this if your package contains header files
## LIBRARIES: libraries you create in this project that dependent projects also need
## CATKIN_DEPENDS: catkin_packages dependent projects also need
## DEPENDS: system dependencies of this project that dependent projects also need
catkin_package(
  CATKIN_DEPENDS message_runtime)

#############
## Install ##
#############

install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
  PATTERN "*.h")
