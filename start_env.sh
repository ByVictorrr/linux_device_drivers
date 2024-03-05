docker build -t linux_cpp_dev_env .
docker run -it --rm -v ${PWD}:/work linux_cpp_dev_env