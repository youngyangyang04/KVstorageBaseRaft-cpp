rm -rf ./cmake-build-debug/*
rm -rf ./bin/*
rm -rf ./lib/*
cd ./cmake-build-debug/
cmake ..
make -j2