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

# The manifest starts "minetest --go", which loads an existing world (minetest.conf selects
# "world1"); a fresh install has none, so create it here instead of via the main menu.
mkdir -p ./luanti-5.6.1/worlds/world1
[ -f ./luanti-5.6.1/worlds/world1/world.mt ] || cat > ./luanti-5.6.1/worlds/world1/world.mt <<'WORLD'
enable_damage = true
creative_mode = false
mod_storage_backend = sqlite3
auth_backend = sqlite3
player_backend = sqlite3
backend = dummy
gameid = devtest
world_name = world1
server_announce = false
WORLD

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
