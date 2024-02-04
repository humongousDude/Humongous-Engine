cd build

if not exist compiledShaders\ (
    mkdir compiledShaders)

ninja
main
cd ../
