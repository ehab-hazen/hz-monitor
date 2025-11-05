curl https://raw.githubusercontent.com/NVIDIA/go-nvml/main/gen/nvml/nvml.h -o /usr/include/nvml.h
mkdir build
cd build
cmake ..
make install
