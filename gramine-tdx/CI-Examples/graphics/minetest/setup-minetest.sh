set -e

echo "------------------------------------------------"

echo "------------------Setting up minetest dep------------------------------"
echo "------------------------------------------------"
sudo apt update
sudo apt install -y unzip patch g++ make libc6-dev cmake libpng-dev libjpeg-dev libxi-dev libgl1-mesa-dev libsqlite3-dev libogg-dev libvorbis-dev libopenal-dev libcurl4-gnutls-dev libfreetype6-dev zlib1g-dev libgmp-dev libjsoncpp-dev libzstd-dev libluajit-5.1-dev
wget https://github.com/minetest/minetest/archive/refs/tags/5.6.1.zip
unzip 5.6.1.zip
rm -r 5.6.1.zip
pushd luanti-5.6.1/
    pushd ./lib
            wget https://github.com/minetest/irrlicht/archive/refs/tags/1.9.0mt8.zip
            unzip 1.9.0mt8.zip
            rm -r 1.9.0mt8.zip
            mv irrlicht-1.9.0mt8 irrlichtmt
    popd

    echo "------------------------------------------------"
    echo "------------------Compiling and building minetest------------------------------"
    echo "------------------------------------------------"
popd

# Patch Files
cp ./patch_files/minetest.conf ./luanti-5.6.1/                          # General config settings

pushd luanti-5.6.1/
    patch --batch -p1 < ../patch_files/minetest-source-changes.patch
    cmake . -DRUN_IN_PLACE=TRUE -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    cd ../

    echo "------------------------------------------------"
    echo "------------------------------------------------"
    echo "------------------ Start the game using minetest-5.6.1/bin/minetest ------------------------------"
    echo "------------------------------------------------"
    echo "------------------------------------------------"
popd
