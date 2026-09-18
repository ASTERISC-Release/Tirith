echo "--------------------- Setting things up for ya --------------------"
mkdir /root/.yq2
git clone git@github.com:yquake2/yquake2.git ./quake2_bin
sudo apt install build-essential \
    libgl1-mesa-dev libsdl2-dev libopenal-dev \
    libcurl4-openssl-dev
pushd ./quake2_bin
    git checkout QUAKE2_8_41
    make clean
    make -j`nproc`
    pushd ./release/baseq2
        wget https://github.com/drags/docker-quake2/raw/refs/heads/master/baseq2/pak0.pak
    popd
    echo "--------------------------------------------------"
    echo "You are now ready to launch the game from ./quake2"
    echo "--------------------------------------------------"
popd